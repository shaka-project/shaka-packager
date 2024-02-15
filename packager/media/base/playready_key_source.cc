// Copyright 2017 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/media/base/playready_key_source.h>

#include <algorithm>
#include <iterator>

#include <absl/log/check.h>
#include <absl/log/log.h>
#include <absl/strings/escaping.h>

#include <packager/macros/compiler.h>
#include <packager/macros/logging.h>
#include <packager/macros/status.h>
#include <packager/media/base/buffer_writer.h>
#include <packager/media/base/http_key_fetcher.h>
#include <packager/media/base/key_source.h>
#include <packager/media/base/protection_system_ids.h>
#include <packager/utils/hex_parser.h>

namespace shaka {
namespace media {

namespace {

const int32_t kHttpFetchTimeout = 60;  // In seconds
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
  if (!absl::Base64Unescape(base64_string, &str))
    return false;
  bytes->assign(str.begin(), str.end());
  return true;
}
}

PlayReadyKeySource::PlayReadyKeySource(const std::string& server_url,
                                       ProtectionSystem protection_systems)
    // PlayReady PSSH is retrived from PlayReady server response.
    : generate_playready_protection_system_(
          // Generate PlayReady protection system if there are no other
          // protection system specified.
          protection_systems == ProtectionSystem::kNone ||
          has_flag(protection_systems, ProtectionSystem::kPlayReady)),
      encryption_key_(new EncryptionKey),
      server_url_(server_url) {}

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

  std::string key_id_raw;
  if (!ValidHexStringToBytes(key_id_hex, &key_id_raw)) {
    LOG(ERROR) << "Cannot parse key_id_hex, " << key_id_hex;
    return Status(error::SERVER_ERROR, "Cannot parse key_id_hex.");
  }
  encryption_key->key_id.assign(key_id_raw.begin(), key_id_raw.end());

  std::string key_data_b64;
  RETURN_IF_ERROR(RetrieveTextInXMLElement("KeyData", response, &key_data_b64));
  if (!Base64StringToBytes(key_data_b64, &encryption_key->key)) {
    LOG(ERROR) << "Cannot parse key, " << key_data_b64;
    return Status(error::SERVER_ERROR, "Cannot parse key.");
  }
  encryption_key->key_ids.emplace_back(encryption_key->key_id);

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
                               std::size(kPlayReadySystemId));
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

  std::string acquire_license_request = kAcquireLicenseRequest;

  // Replace "$0" with |program_identifier|
  size_t dollar_zero_location = acquire_license_request.find("$0");
  if (dollar_zero_location != std::string::npos) {
    acquire_license_request.replace(dollar_zero_location, /* len= */ 2,
                                    program_identifier);
  }

  std::string acquire_license_response;
  Status status = key_fetcher.FetchKeys(server_url_, acquire_license_request,
                                        &acquire_license_response);
  VLOG(1) << "Server response: " << acquire_license_response;
  RETURN_IF_ERROR(status);

  RETURN_IF_ERROR(SetKeyInformationFromServerResponse(
      acquire_license_response, generate_playready_protection_system_,
      encryption_key.get()));

  // PlayReady does not specify different streams.
  encryption_key_ = std::move(encryption_key);
  return Status::OK;
}

Status PlayReadyKeySource::FetchKeys(EmeInitDataType init_data_type,
                                     const std::vector<uint8_t>& init_data) {
  UNUSED(init_data_type);
  UNUSED(init_data);
  // Do nothing for PlayReady encryption/decryption.
  return Status::OK;
}

Status PlayReadyKeySource::GetKey(const std::string& stream_label,
                                  EncryptionKey* key) {
  UNUSED(stream_label);
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
  UNUSED(key_id);
  // TODO(robinconnell): Currently all tracks are encrypted using the same
  // key_id and key.  Add the ability to encrypt using multiple key_id/keys.
  DCHECK(key);
  DCHECK(encryption_key_);
  *key = *encryption_key_;
  return Status::OK;
}

Status PlayReadyKeySource::GetCryptoPeriodKey(
    uint32_t crypto_period_index,
    int32_t crypto_period_duration_in_seconds,
    const std::string& stream_label,
    EncryptionKey* key) {
  UNUSED(crypto_period_index);
  UNUSED(crypto_period_duration_in_seconds);
  UNUSED(stream_label);
  // TODO(robinconnell): Implement key rotation.
  *key = *encryption_key_;
  return Status::OK;
}

}  // namespace media
}  // namespace shaka
