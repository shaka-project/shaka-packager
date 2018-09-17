// Copyright 2017 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/base/playready_pssh_generator.h"

#include <openssl/aes.h>
#include <algorithm>
#include <memory>
#include <set>
#include <vector>

#include "packager/base/base64.h"
#include "packager/base/logging.h"
#include "packager/base/strings/string_number_conversions.h"
#include "packager/base/strings/string_util.h"
#include "packager/media/base/buffer_writer.h"
#include "packager/media/base/protection_system_ids.h"

namespace shaka {
namespace media {
namespace {

const uint8_t kPlayReadyPsshBoxVersion = 1;
const std::string kPlayHeaderObject_4_1 =
    "<WRMHEADER "
    "xmlns=\"http://schemas.microsoft.com/DRM/2007/03/PlayReadyHeader\" "
    "version=\"4.1.0.0\"><DATA><PROTECTINFO>"
    "<KID VALUE=\"$0\" ALGID=\"AESCTR\" CHECKSUM=\"$1\"></KID></PROTECTINFO>"
    "</DATA></WRMHEADER>";
const std::string kPlayHeaderObject_4_0 =
    "<WRMHEADER "
    "xmlns=\"http://schemas.microsoft.com/DRM/2007/03/PlayReadyHeader\" "
    "version=\"4.0.0.0\"><DATA><PROTECTINFO><KEYLEN>16</KEYLEN>"
    "<ALGID>AESCTR</ALGID></PROTECTINFO><KID>$0</KID><CHECKSUM>$1</CHECKSUM>"
    "</DATA></WRMHEADER>";

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
  std::unique_ptr<AES_KEY> aes_key(new AES_KEY);
  CHECK_EQ(AES_set_encrypt_key(key.data(), key.size() * 8, aes_key.get()), 0);
  AES_ecb_encrypt(key_id_converted.data(), encrypted_key_id.data(),
                  aes_key.get(), AES_ENCRYPT);
  std::string checksum =
      std::string(encrypted_key_id.begin(), encrypted_key_id.end())
          .substr(0, 8);
  std::string base64_checksum;
  base::Base64Encode(checksum, &base64_checksum);
  std::string base64_key_id;
  base::Base64Encode(
      std::string(key_id_converted.begin(), key_id_converted.end()),
      &base64_key_id);
  std::string playready_header = kPlayHeaderObject_4_0;
  base::ReplaceFirstSubstringAfterOffset(&playready_header, 0, "$0",
                                         base64_key_id);
  base::ReplaceFirstSubstringAfterOffset(&playready_header, 0, "$1",
                                         base64_checksum);

  // Create a PlayReady Record.
  // Outline in section '2.PlayReady Records' of
  // 'PlayReady Header Object' document.  Note data is in little endian format.
  std::vector<uint16_t> record_value =
      std::vector<uint16_t>(playready_header.begin(), playready_header.end());
  shaka::media::BufferWriter writer_pr_record;
  uint16_t record_type =
      1;  // Indicates that the record contains a rights management header.
  uint16_t record_length = record_value.size() * 2;
  writer_pr_record.AppendInt(static_cast<uint8_t>(record_type & 0xff));
  writer_pr_record.AppendInt(static_cast<uint8_t>((record_type >> 8) & 0xff));
  writer_pr_record.AppendInt(static_cast<uint8_t>(record_length & 0xff));
  writer_pr_record.AppendInt(static_cast<uint8_t>((record_length >> 8) & 0xff));
  for (auto record_item : record_value) {
    writer_pr_record.AppendInt(static_cast<uint8_t>(record_item & 0xff));
    writer_pr_record.AppendInt(static_cast<uint8_t>((record_item >> 8) & 0xff));
  }

  // Create the PlayReady Header object.
  // Outline in section '1.PlayReady Header Objects' of
  // 'PlayReady Header Object' document.   Note data is in little endian format.
  shaka::media::BufferWriter writer_pr_header_object;
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
  writer_pr_header_object.AppendInt(static_cast<uint8_t>(record_count & 0xff));
  writer_pr_header_object.AppendInt(
      static_cast<uint8_t>((record_count >> 8) & 0xff));
  writer_pr_header_object.AppendBuffer(writer_pr_record);
  *output = std::vector<uint8_t>(
      writer_pr_header_object.Buffer(),
      writer_pr_header_object.Buffer() + writer_pr_header_object.Size());
  return Status::OK;
}
}  // namespace

PlayReadyPsshGenerator::PlayReadyPsshGenerator()
    : PsshGenerator(std::vector<uint8_t>(std::begin(kPlayReadySystemId),
                                         std::end(kPlayReadySystemId)),
                    kPlayReadyPsshBoxVersion) {}

PlayReadyPsshGenerator::~PlayReadyPsshGenerator() {}

bool PlayReadyPsshGenerator::SupportMultipleKeys() {
  return false;
}

base::Optional<std::vector<uint8_t>>
PlayReadyPsshGenerator::GeneratePsshDataFromKeyIdAndKey(
    const std::vector<uint8_t>& key_id,
    const std::vector<uint8_t>& key) const {
  std::vector<uint8_t> pssh_data;
  Status status = GeneratePlayReadyPsshData(key_id, key, &pssh_data);
  if (!status.ok()) {
    LOG(ERROR) << status.ToString();
    return base::nullopt;
  }

  return pssh_data;
}

base::Optional<std::vector<uint8_t>>
PlayReadyPsshGenerator::GeneratePsshDataFromKeyIds(
    const std::vector<std::vector<uint8_t>>& key_ids) const {
  NOTIMPLEMENTED();
  return base::nullopt;
}

}  // namespace media
}  // namespace shaka
