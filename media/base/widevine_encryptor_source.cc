// Copyright (c) 2013 Google Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/widevine_encryptor_source.h"

#include "base/base64.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/sha1.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "media/base/aes_encryptor.h"
#include "media/base/httpfetcher.h"

// TODO(kqyang): Move media/mp4/rcheck.h to media/base/.
//               Remove this definition and use RCHECK in rcheck.h instead.
#define RCHECK(x)                                       \
  do {                                                  \
    if (!(x)) {                                         \
      LOG(ERROR) << "Failure while processing: " << #x; \
      return false;                                     \
    }                                                   \
  } while (0)

namespace {

bool Base64StringToBytes(const std::string& base64_string,
                         std::vector<uint8>* bytes) {
  DCHECK(bytes);
  std::string str;
  if (!base::Base64Decode(base64_string, &str))
    return false;
  bytes->assign(str.begin(), str.end());
  return true;
}

bool GetKeyAndKeyId(const base::DictionaryValue& track_dict,
                    std::vector<uint8>* key,
                    std::vector<uint8>* key_id) {
  DCHECK(key);
  DCHECK(key_id);

  std::string key_base64_string;
  RCHECK(track_dict.GetString("key", &key_base64_string));
  VLOG(1) << "Key:" << key_base64_string;
  RCHECK(Base64StringToBytes(key_base64_string, key));

  std::string key_id_base64_string;
  RCHECK(track_dict.GetString("key_id", &key_id_base64_string));
  VLOG(1) << "Keyid:" << key_id_base64_string;
  RCHECK(Base64StringToBytes(key_id_base64_string, key_id));

  return true;
}

bool GetPssh(const base::DictionaryValue& track_dict,
             std::vector<uint8>* pssh) {
  DCHECK(pssh);

  // TODO(kqyang): Add support for multiple pssh.
  const base::ListValue* pssh_list;
  RCHECK(track_dict.GetList("pssh", &pssh_list));
  // Invariant check. We don't want to crash in release mode if possible.
  // The following code handles it gracefully if GetSize() does not return 1.
  DCHECK_EQ(1, pssh_list->GetSize());

  const base::DictionaryValue* pssh_dict;
  RCHECK(pssh_list->GetDictionary(0, &pssh_dict));
  std::string drm_type;
  RCHECK(pssh_dict->GetString("drm_type", &drm_type));
  if (drm_type != "WIDEVINE") {
    LOG(ERROR) << "Expecting drm_type 'WIDEVINE', get '" << drm_type << "'.";
    return false;
  }
  std::string pssh_base64_string;
  RCHECK(pssh_dict->GetString("data", &pssh_base64_string));

  VLOG(1) << "Pssh:" << pssh_base64_string;
  RCHECK(Base64StringToBytes(pssh_base64_string, pssh));
  return true;
}

}  // namespace

namespace media {

WidevineEncryptorSource::WidevineEncryptorSource(const std::string& server_url,
                                                 const std::string& content_id,
                                                 TrackType track_type)
    : server_url_(server_url),
      content_id_(content_id),
      track_type_(track_type) {
  DCHECK(!server_url.empty());
  DCHECK(!content_id.empty());
}
WidevineEncryptorSource::~WidevineEncryptorSource() {}

bool WidevineEncryptorSource::SetAesSigningKey(const std::string& signer,
                                               const std::string& aes_key_hex,
                                               const std::string& iv_hex) {
  DCHECK(!aes_cbc_encryptor_);
  signer_ = signer;

  std::vector<uint8> aes_key;
  if (!base::HexStringToBytes(aes_key_hex, &aes_key)) {
    LOG(ERROR) << "Failed to convert hex string to bytes: " << aes_key_hex;
    return false;
  }
  std::vector<uint8> iv;
  if (!base::HexStringToBytes(iv_hex, &iv)) {
    LOG(ERROR) << "Failed to convert hex string to bytes: " << iv_hex;
    return false;
  }

  scoped_ptr<AesCbcEncryptor> encryptor(new AesCbcEncryptor());
  if (!encryptor->InitializeWithIv(aes_key, iv)) {
    LOG(ERROR) << "Failed to initialize encryptor with key: " << aes_key_hex
               << " iv:" << iv_hex;
    return false;
  }
  aes_cbc_encryptor_ = encryptor.Pass();
  return true;
}

bool WidevineEncryptorSource::SetRsaSigningKey(
    const std::string& signer,
    const std::string& pkcs8_rsa_key) {
  DCHECK(!aes_cbc_encryptor_);
  // TODO(kqyang): Implement it.
  NOTIMPLEMENTED();
  return false;
}

Status WidevineEncryptorSource::Initialize() {
  std::string request;
  FillRequest(content_id_, &request);

  std::string message;
  Status status = SignRequest(request, &message);
  if (!status.ok())
    return status;
  DLOG(INFO) << "Message: " << message;

  HTTPFetcher fetcher;
  std::string raw_response;
  status = fetcher.Post(server_url_, message, &raw_response);
  if (!status.ok())
    return status;
  DLOG(INFO) << "Response:" << raw_response;

  std::string response;
  if (!DecodeResponse(raw_response, &response)) {
    return Status(error::INVALID_ARGUMENT,
                  "Failed to decode response '" + raw_response + "'.");
  }

  std::vector<uint8> key_id;
  std::vector<uint8> key;
  std::vector<uint8> pssh;
  if (!ExtractEncryptionKey(response, &key_id, &key, &pssh)) {
    return Status(error::INVALID_ARGUMENT,
                  "Failed to extract encryption key from '" + response + "'.");
  }

  set_key_id(key_id);
  set_key(key);
  set_pssh(pssh);
  return Status::OK;
}

void WidevineEncryptorSource::GenerateSignature(const std::string& message,
                                                std::string* signature) {
  DCHECK(signature);
  if (aes_cbc_encryptor_)
    aes_cbc_encryptor_->Encrypt(base::SHA1HashString(message), signature);
  else
    NOTIMPLEMENTED() << "Rsa signing is not implemented yet.";
}

void WidevineEncryptorSource::FillRequest(const std::string& content_id,
                                          std::string* request) {
  DCHECK(request);

  std::string content_id_base64_string;
  CHECK(base::Base64Encode(content_id, &content_id_base64_string));

  base::DictionaryValue request_dict;
  request_dict.SetString("content_id", content_id_base64_string);
  // TODO(kqyang): Do we care about policy?
  request_dict.SetString("policy", "");

  // Build tracks.
  base::ListValue* tracks = new base::ListValue();

  base::DictionaryValue* track_sd = new base::DictionaryValue();
  track_sd->SetString("type", "SD");
  tracks->Append(track_sd);
  base::DictionaryValue* track_hd = new base::DictionaryValue();
  track_hd->SetString("type", "HD");
  tracks->Append(track_hd);
  base::DictionaryValue* track_audio = new base::DictionaryValue();
  track_audio->SetString("type", "AUDIO");
  tracks->Append(track_audio);

  request_dict.Set("tracks", tracks);

  // Build DRM types.
  base::ListValue* drm_types = new base::ListValue();
  drm_types->AppendString("WIDEVINE");
  request_dict.Set("drm_types", drm_types);

  base::JSONWriter::Write(&request_dict, request);
}

Status WidevineEncryptorSource::SignRequest(const std::string& request,
                                            std::string* signed_request) {
  DCHECK(signed_request);

  // Sign the request.
  std::string signature;
  GenerateSignature(request, &signature);

  // Encode request and signature using Base64 encoding.
  std::string request_base64_string;
  CHECK(base::Base64Encode(request, &request_base64_string));

  std::string signature_base64_string;
  CHECK(base::Base64Encode(signature, &signature_base64_string));

  base::DictionaryValue signed_request_dict;
  signed_request_dict.SetString("request", request_base64_string);
  signed_request_dict.SetString("signature", signature_base64_string);
  signed_request_dict.SetString("signer", signer_);

  base::JSONWriter::Write(&signed_request_dict, signed_request);
  return Status::OK;
}

bool WidevineEncryptorSource::DecodeResponse(const std::string& raw_response,
                                             std::string* response) {
  DCHECK(response);

  // Extract base64 formatted response from JSON formatted raw response.
  scoped_ptr<base::Value> root(base::JSONReader::Read(raw_response));
  if (!root) {
    LOG(ERROR) << "'" << raw_response << "' is not in JSON format.";
    return false;
  }
  const base::DictionaryValue* response_dict = NULL;
  RCHECK(root->GetAsDictionary(&response_dict));

  std::string response_base64_string;
  RCHECK(response_dict->GetString("response", &response_base64_string));
  RCHECK(base::Base64Decode(response_base64_string, response));
  return true;
}

bool WidevineEncryptorSource::IsExpectedTrackType(
    const std::string& track_type_string) {
  switch (track_type_) {
    case TRACK_TYPE_SD:
      return track_type_string == "SD";
    case TRACK_TYPE_HD:
      return track_type_string == "HD";
    case TRACK_TYPE_AUDIO:
      return track_type_string == "AUDIO";
    default:
      NOTREACHED() << "Unexpected track type " << track_type_;
      return false;
  }
}

bool WidevineEncryptorSource::ExtractEncryptionKey(const std::string& response,
                                                   std::vector<uint8>* key_id,
                                                   std::vector<uint8>* key,
                                                   std::vector<uint8>* pssh) {
  scoped_ptr<base::Value> root(base::JSONReader::Read(response));
  if (!root) {
    LOG(ERROR) << "'" << response << "' is not in JSON format.";
    return false;
  }

  const base::DictionaryValue* license_dict = NULL;
  RCHECK(root->GetAsDictionary(&license_dict));

  std::string license_status;
  RCHECK(license_dict->GetString("status", &license_status));
  if (license_status != "OK") {
    LOG(ERROR) << "Received non-OK license response: " << response;
    return false;
  }

  const base::ListValue* tracks;
  RCHECK(license_dict->GetList("tracks", &tracks));

  for (base::ListValue::const_iterator it = tracks->begin();
       it != tracks->end();
       ++it) {
    const base::DictionaryValue* track_dict;
    RCHECK((*it)->GetAsDictionary(&track_dict));

    std::string track_type;
    RCHECK(track_dict->GetString("type", &track_type));
    if (!IsExpectedTrackType(track_type))
      continue;

    return GetKeyAndKeyId(*track_dict, key, key_id) &&
           GetPssh(*track_dict, pssh);
  }
  LOG(ERROR) << "Cannot find key of type " << track_type_ << " from '"
             << response << "'.";
  return false;
}

}  // namespace media
