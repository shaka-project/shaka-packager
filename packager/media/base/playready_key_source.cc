// Copyright 2017 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/base/playready_key_source.h"

#include <algorithm>

#include "packager/base/base64.h"
#include "packager/base/logging.h"
#include "packager/base/strings/string_number_conversions.h"
#include "packager/base/strings/string_util.h"
#include "packager/media/base/buffer_writer.h"
#include "packager/media/base/http_key_fetcher.h"
#include "packager/media/base/key_source.h"
#include "packager/media/base/protection_system_ids.h"
#include "packager/status_macros.h"

namespace shaka {
namespace media {

namespace {

const uint32_t kHttpFetchTimeout = 60;  // In seconds
const std::string kAcquireLicenseRequest =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
    "<soap:Envelope xmlns=\"http://schemas.xmlsoap.org/soap/envelope/\" "
    "xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" "
    "xmlns:xsd=\"http://www.w3.org/2001/XMLSchema\" "
    "xmlns:soap=\"http://schemas.xmlsoap.org/soap/envelope/\">"
    "<soap:Body>"
    "<AcquirePackagingData "
    "xmlns=\"http://schemas.microsoft.com/DRM/2007/03/protocols\">"
    "<challenge "
    "xmlns=\"http://schemas.microsoft.com/DRM"
    "/2007/03/protocols/AcquirePackagingData/v1.0\">"
    "<ProtectionSystems>"
    "<ProtectionSystemId>9A04F079-9840-4286-AB92-E65BE0885F95"
    "</ProtectionSystemId>"
    "</ProtectionSystems>"
    "<StreamProtectionRequests>"
    "<StreamInformation>"
    "<ProgramIdentifier>$0</ProgramIdentifier>"
    "<OffsetFromProgramStart>P0S</OffsetFromProgramStart>"
    "</StreamInformation>"
    "</StreamProtectionRequests>"
    "</challenge>"
    "</AcquirePackagingData>"
    "</soap:Body>"
    "</soap:Envelope>";

bool Base64StringToBytes(const std::string& base64_string,
                         std::vector<uint8_t>* bytes) {
  DCHECK(bytes);
  std::string str;
  if (!base::Base64Decode(base64_string, &str))
    return false;
  bytes->assign(str.begin(), str.end());
  return true;
}
}

PlayReadyKeySource::PlayReadyKeySource(const std::string& server_url,
                                       int protection_system_flags,
                                       FourCC protection_scheme)
    // PlayReady PSSH is retrived from PlayReady server response.
    : KeySource(protection_system_flags & ~PLAYREADY_PROTECTION_SYSTEM_FLAG,
                protection_scheme),
      generate_playready_protection_system_(
          // Generate PlayReady protection system if there are no other
          // protection system specified.
          protection_system_flags == NO_PROTECTION_SYSTEM_FLAG ||
          protection_system_flags & PLAYREADY_PROTECTION_SYSTEM_FLAG),
      encryption_key_(new EncryptionKey),
      server_url_(server_url) {}

PlayReadyKeySource::PlayReadyKeySource(
    const std::string& server_url,
    const std::string& client_cert_file,
    const std::string& client_cert_private_key_file,
    const std::string& client_cert_private_key_password,
    int protection_system_flags,
    FourCC protection_scheme)
    // PlayReady PSSH is retrived from PlayReady server response.
    : KeySource(protection_system_flags & ~PLAYREADY_PROTECTION_SYSTEM_FLAG,
                protection_scheme),
      encryption_key_(new EncryptionKey),
      server_url_(server_url),
      client_cert_file_(client_cert_file),
      client_cert_private_key_file_(client_cert_private_key_file),
      client_cert_private_key_password_(client_cert_private_key_password) {}

PlayReadyKeySource::~PlayReadyKeySource() = default;

Status RetrieveTextInXMLElement(const std::string& element,
                                const std::string& xml,
                                std::string* value) {
  std::string start_tag = "<" + element + ">";
  std::string end_tag = "</" + element + ">";
  std::size_t start_pos = xml.find(start_tag);
  if (start_pos == std::string::npos) {
    return Status(error::SERVER_ERROR,
                  "Unable to find tag: " + start_tag);
  }
  start_pos += start_tag.size();
  std::size_t end_pos = xml.find(end_tag);
  if (end_pos == std::string::npos) {
    return Status(error::SERVER_ERROR,
                  "Unable to find tag: " + end_tag);
  }
  if (start_pos > end_pos) {
    return Status(error::SERVER_ERROR, "Invalid positions");
  }
  std::size_t segment_len = end_pos - start_pos;
  *value = xml.substr(start_pos, segment_len);
  return Status::OK;
}

Status SetKeyInformationFromServerResponse(
    const std::string& response,
    bool generate_playready_protection_system,
    EncryptionKey* encryption_key) {
  // TODO(robinconnell): Currently all tracks are encrypted using the same
  // key_id and key.  Add the ability to retrieve multiple key_id/keys from
  // the packager response and encrypt multiple tracks using differnt
  // key_id/keys.
  std::string key_id_hex;
  RETURN_IF_ERROR(RetrieveTextInXMLElement("KeyId", response, &key_id_hex));
  key_id_hex.erase(
      std::remove(key_id_hex.begin(), key_id_hex.end(), '-'), key_id_hex.end());
  if (!base::HexStringToBytes(key_id_hex, &encryption_key->key_id)) {
    LOG(ERROR) << "Cannot parse key_id_hex, " << key_id_hex;
    return Status(error::SERVER_ERROR, "Cannot parse key_id_hex.");
  }

  std::string key_data_b64;
  RETURN_IF_ERROR(RetrieveTextInXMLElement("KeyData", response, &key_data_b64));
  if (!Base64StringToBytes(key_data_b64, &encryption_key->key)) {
    LOG(ERROR) << "Cannot parse key, " << key_data_b64;
    return Status(error::SERVER_ERROR, "Cannot parse key.");
  }

  if (generate_playready_protection_system) {
    std::string pssh_data_b64;
    RETURN_IF_ERROR(RetrieveTextInXMLElement("Data", response, &pssh_data_b64));
    std::vector<uint8_t> pssh_data;
    if (!Base64StringToBytes(pssh_data_b64, &pssh_data)) {
      LOG(ERROR) << "Cannot parse pssh data, " << pssh_data_b64;
      return Status(error::SERVER_ERROR, "Cannot parse pssh.");
    }

    PsshBoxBuilder pssh_builder;
    pssh_builder.add_key_id(encryption_key->key_id);
    pssh_builder.set_system_id(kPlayReadySystemId,
                               arraysize(kPlayReadySystemId));
    pssh_builder.set_pssh_data(pssh_data);
    encryption_key->key_system_info.push_back(
        {pssh_builder.system_id(), pssh_builder.CreateBox()});
  }
  return Status::OK;
}

Status PlayReadyKeySource::FetchKeysWithProgramIdentifier(
    const std::string& program_identifier) {
  std::unique_ptr<EncryptionKey> encryption_key(new EncryptionKey);
  HttpKeyFetcher key_fetcher(kHttpFetchTimeout);
  if (!client_cert_file_.empty() && !client_cert_private_key_file_.empty()) {
    key_fetcher.SetClientCertInfo(client_cert_file_,
                                  client_cert_private_key_file_,
                                  client_cert_private_key_password_);
  }
  if (!ca_file_.empty()) {
    key_fetcher.SetCaFile(ca_file_);
  }

  std::string acquire_license_request = kAcquireLicenseRequest;
  base::ReplaceFirstSubstringAfterOffset(
      &acquire_license_request, 0, "$0", program_identifier);
  std::string acquire_license_response;
  Status status = key_fetcher.FetchKeys(server_url_, acquire_license_request,
                                        &acquire_license_response);
  VLOG(1) << "Server response: " << acquire_license_response;
  RETURN_IF_ERROR(status);

  RETURN_IF_ERROR(SetKeyInformationFromServerResponse(
      acquire_license_response, generate_playready_protection_system_,
      encryption_key.get()));

  // PlayReady does not specify different streams.
  const char kEmptyDrmLabel[] = "";
  EncryptionKeyMap encryption_key_map;
  encryption_key_map[kEmptyDrmLabel] = std::move(encryption_key);
  RETURN_IF_ERROR(UpdateProtectionSystemInfo(&encryption_key_map));
  encryption_key_ = std::move(encryption_key_map[kEmptyDrmLabel]);
  return Status::OK;
}

Status PlayReadyKeySource::FetchKeys(EmeInitDataType init_data_type,
                                     const std::vector<uint8_t>& init_data) {
  // Do nothing for PlayReady encryption/decryption.
  return Status::OK;
}

Status PlayReadyKeySource::GetKey(const std::string& stream_label,
                                  EncryptionKey* key) {
  // TODO(robinconnell): Currently all tracks are encrypted using the same
  // key_id and key.  Add the ability to encrypt each stream_label using a
  // different key_id and key.
  DCHECK(key);
  DCHECK(encryption_key_);
  *key = *encryption_key_;
  return Status::OK;
}

Status PlayReadyKeySource::GetKey(const std::vector<uint8_t>& key_id,
                                  EncryptionKey* key) {
  // TODO(robinconnell): Currently all tracks are encrypted using the same
  // key_id and key.  Add the ability to encrypt using multiple key_id/keys.
  DCHECK(key);
  DCHECK(encryption_key_);
  *key = *encryption_key_;
  return Status::OK;
}

Status PlayReadyKeySource::GetCryptoPeriodKey(uint32_t crypto_period_index,
                                              const std::string& stream_label,
                                              EncryptionKey* key) {
  // TODO(robinconnell): Implement key rotation.
  *key = *encryption_key_;
  return Status::OK;
}

}  // namespace media
}  // namespace shaka
