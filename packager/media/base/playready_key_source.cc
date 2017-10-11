// Copyright 2017 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/base/playready_key_source.h"

#include <openssl/aes.h>
#include <algorithm>
#include "packager/base/base64.h"
#include "packager/base/logging.h"
#include "packager/base/strings/string_number_conversions.h"
#include "packager/base/strings/string_util.h"
#include "packager/media/base/buffer_writer.h"
#include "packager/media/base/http_key_fetcher.h"

namespace shaka {
namespace media {

namespace {

const uint32_t kHttpFetchTimeout = 60;  // In seconds
const std::string kPlayHeaderObject_4_1 = "<WRMHEADER "
    "xmlns=\"http://schemas.microsoft.com/DRM/2007/03/PlayReadyHeader\" "
    "version=\"4.1.0.0\"><DATA><PROTECTINFO>"
    "<KID VALUE=\"$0\" ALGID=\"AESCTR\" CHECKSUM=\"$1\"></KID></PROTECTINFO>"
    "</DATA></WRMHEADER>";
const std::string kPlayHeaderObject_4_0 = "<WRMHEADER "
    "xmlns=\"http://schemas.microsoft.com/DRM/2007/03/PlayReadyHeader\" "
    "version=\"4.0.0.0\"><DATA><PROTECTINFO><KEYLEN>16</KEYLEN>"
    "<ALGID>AESCTR</ALGID></PROTECTINFO><KID>$0</KID><CHECKSUM>$1</CHECKSUM>"
    "</DATA></WRMHEADER>";
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

// Converts the key_id's endianness.
std::vector<uint8_t> ConvertGuidEndianness(const std::vector<uint8_t>& input) {
  std::vector<uint8_t> output = input;
  if (output.size() > 7) {  // Defensive check.
    output[0] = input[3];
    output[1] = input[2];
    output[2] = input[1];
    output[3] = input[0];
    output[4] = input[5];
    output[5] = input[4];
    output[6] = input[7];
    output[7] = input[6];
    // 8-15 are an array of bytes with no endianness.
  }
  return output;
}

// Generates the data section of a PlayReady PSSH.
// PlayReady PSSH Data is a PlayReady Header Object.
// Format is outlined in the following document.
// http://download.microsoft.com/download/2/3/8/238F67D9-1B8B-48D3-AB83-9C00112268B2/PlayReady%20Header%20Object%202015-08-13-FINAL-CL.PDF
Status GeneratePlayReadyPsshData(const std::vector<uint8_t>& key_id,
                                 const std::vector<uint8_t>& key,
                                 std::vector<uint8_t>* output) {
  CHECK(output);
  std::vector<uint8_t> key_id_converted = ConvertGuidEndianness(key_id);
  std::vector<uint8_t> encrypted_key_id(key_id_converted.size());
  std::unique_ptr<AES_KEY> aes_key (new AES_KEY);
  CHECK_EQ(AES_set_encrypt_key(key.data(), key.size() * 8, aes_key.get()), 0);
  AES_ecb_encrypt(key_id_converted.data(), encrypted_key_id.data(),
                  aes_key.get(), AES_ENCRYPT);
  std::string checksum = std::string(encrypted_key_id.begin(),
                                     encrypted_key_id.end()).substr(0, 8);
  std::string base64_checksum;
  base::Base64Encode(checksum, &base64_checksum);
  std::string base64_key_id;
  base::Base64Encode(std::string(key_id_converted.begin(),
                                 key_id_converted.end()),
                     &base64_key_id);
  std::string playready_header = kPlayHeaderObject_4_0;
  base::ReplaceFirstSubstringAfterOffset(
      &playready_header, 0, "$0", base64_key_id);
  base::ReplaceFirstSubstringAfterOffset(
      &playready_header, 0, "$1", base64_checksum);

  // Create a PlayReady Record.
  // Outline in section '2.PlayReady Records' of
  // 'PlayReady Header Object' document.  Note data is in little endian format.
  std::vector<uint16_t> record_value =
      std::vector<uint16_t>(playready_header.begin(), playready_header.end());
  BufferWriter writer_pr_record;
  uint16_t record_type = 1; // Indicates that the record contains a rights management header.
  uint16_t record_length = record_value.size() * 2;
  writer_pr_record.AppendInt(static_cast<uint8_t>(record_type & 0xff));
  writer_pr_record.AppendInt(static_cast<uint8_t>((record_type >> 8) & 0xff));
  writer_pr_record.AppendInt(static_cast<uint8_t>(record_length & 0xff));
  writer_pr_record.AppendInt(static_cast<uint8_t>((record_length >> 8) & 0xff));
  for (auto record_item: record_value) {
    writer_pr_record.AppendInt(static_cast<uint8_t>(record_item & 0xff));
    writer_pr_record.AppendInt(static_cast<uint8_t>((record_item >> 8) & 0xff));
  }

  // Create the PlayReady Header object.
  // Outline in section '1.PlayReady Header Objects' of
  // 'PlayReady Header Object' document.   Note data is in little endian format.
  BufferWriter writer_pr_header_object;
  uint32_t playready_header_length = writer_pr_record.Size() + 4 + 2;
  uint16_t record_count = 1;
  writer_pr_header_object.AppendInt(
      static_cast<uint8_t>(playready_header_length & 0xff));
  writer_pr_header_object.AppendInt(
      static_cast<uint8_t>((playready_header_length >> 8) & 0xff));
  writer_pr_header_object.AppendInt(
      static_cast<uint8_t>((playready_header_length >> 16) & 0xff));
  writer_pr_header_object.AppendInt(
      static_cast<uint8_t>((playready_header_length >> 24) & 0xff));
  writer_pr_header_object.AppendInt(
      static_cast<uint8_t>(record_count & 0xff));
  writer_pr_header_object.AppendInt(
      static_cast<uint8_t>((record_count >> 8) & 0xff));
  writer_pr_header_object.AppendBuffer(writer_pr_record);
  *output = std::vector<uint8_t>(writer_pr_header_object.Buffer(),
                                 writer_pr_header_object.Buffer() +
                                 writer_pr_header_object.Size());
  return Status::OK;
}

}  // namespace

PlayReadyKeySource::PlayReadyKeySource(
    const std::string& server_url)
    : encryption_key_(new EncryptionKey),
      server_url_(server_url) {
}

PlayReadyKeySource::PlayReadyKeySource(const std::string& server_url,
    const std::string& client_cert_file,
    const std::string& client_cert_private_key_file,
    const std::string& client_cert_private_key_password)
    : encryption_key_(new EncryptionKey),
      server_url_(server_url),
      client_cert_file_(client_cert_file),
      client_cert_private_key_file_(client_cert_private_key_file),
      client_cert_private_key_password_(client_cert_private_key_password) {
}

PlayReadyKeySource::PlayReadyKeySource(
    std::unique_ptr<EncryptionKey> encryption_key)
    : encryption_key_(std::move(encryption_key)) {
}

PlayReadyKeySource::~PlayReadyKeySource() {}

std::unique_ptr<PlayReadyKeySource> PlayReadyKeySource::CreateFromKeyAndKeyId(
    const std::vector<uint8_t>& key_id,
    const std::vector<uint8_t>& key) {
  std::unique_ptr<EncryptionKey> encryption_key(new EncryptionKey);
  encryption_key->key_id = key_id;
  encryption_key->key = key;
  std::vector<uint8_t> pssh_data;
  Status status = GeneratePlayReadyPsshData(
      encryption_key->key_id, encryption_key->key, &pssh_data);
  if (!status.ok()) {
    LOG(ERROR) << status.ToString();
    return std::unique_ptr<PlayReadyKeySource>();
  }
  ProtectionSystemSpecificInfo info;
  info.add_key_id(encryption_key->key_id);
  info.set_system_id(kPlayReadySystemId, arraysize(kPlayReadySystemId));
  info.set_pssh_data(pssh_data);

  encryption_key->key_system_info.push_back(info);
  return std::unique_ptr<PlayReadyKeySource>(
      new PlayReadyKeySource(std::move(encryption_key)));
}

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

Status SetKeyInformationFromServerResponse(const std::string& response,
                                           EncryptionKey* encryption_key) {
  // TODO(robinconnell): Currently all tracks are encrypted using the same
  // key_id and key.  Add the ability to retrieve multiple key_id/keys from
  // the packager response and encrypt multiple tracks using differnt
  // key_id/keys.
  std::string key_id_hex;
  Status status = RetrieveTextInXMLElement("KeyId", response, &key_id_hex);
  if (!status.ok()) {
    return status;
  }
  key_id_hex.erase(
      std::remove(key_id_hex.begin(), key_id_hex.end(), '-'), key_id_hex.end());
  std::string key_data_b64;
  status = RetrieveTextInXMLElement("KeyData", response, &key_data_b64);
  if (!status.ok()) {
    LOG(ERROR) << "Key retreiving KeyData";
    return status;
  }
  std::string pssh_data_b64;
  status = RetrieveTextInXMLElement("Data", response, &pssh_data_b64);
  if (!status.ok()) {
    LOG(ERROR) << "Key retreiving Data";
    return status;
  }
  if (!base::HexStringToBytes(key_id_hex, &encryption_key->key_id)) {
    LOG(ERROR) << "Cannot parse key_id_hex, " << key_id_hex;
    return Status(error::SERVER_ERROR, "Cannot parse key_id_hex.");
  }

  if (!Base64StringToBytes(key_data_b64, &encryption_key->key)) {
    LOG(ERROR) << "Cannot parse key, " << key_data_b64;
    return Status(error::SERVER_ERROR, "Cannot parse key.");
  }
  std::vector<uint8_t> pssh_data;
  if (!Base64StringToBytes(pssh_data_b64, &pssh_data)) {
    LOG(ERROR) << "Cannot parse pssh data, " << pssh_data_b64;
    return Status(error::SERVER_ERROR, "Cannot parse pssh.");
  }
  ProtectionSystemSpecificInfo info;
  info.add_key_id(encryption_key->key_id);
  info.set_system_id(kPlayReadySystemId, arraysize(kPlayReadySystemId));
  info.set_pssh_data(pssh_data);
  encryption_key->key_system_info.push_back(info);
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
  if (!status.ok()) {
    LOG(ERROR) << "Server response: " << acquire_license_response;
    return status;
  }
  status = SetKeyInformationFromServerResponse(acquire_license_response,
                                               encryption_key.get());
  encryption_key_ = std::move(encryption_key);
  return Status::OK;
}

Status PlayReadyKeySource::FetchKeys(EmeInitDataType init_data_type,
                                     const std::vector<uint8_t>& init_data) {
  // Do nothing for playready encryption/decryption.
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
