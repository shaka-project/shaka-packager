// Copyright 2017 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/media/base/playready_pssh_generator.h>

#include <algorithm>
#include <memory>
#include <set>
#include <vector>

#include <absl/log/check.h>
#include <absl/log/log.h>
#include <absl/strings/escaping.h>
#include <mbedtls/cipher.h>

#include <packager/macros/compiler.h>
#include <packager/macros/crypto.h>
#include <packager/macros/logging.h>
#include <packager/media/base/buffer_writer.h>
#include <packager/media/base/protection_system_ids.h>

namespace shaka {
namespace media {
namespace {

const uint8_t kPlayReadyPsshBoxVersion = 0;

// For PlayReady clients 1.0+ that support CTR keys.
const std::string kPlayHeaderObject_4_0 =
    "<WRMHEADER "
    "xmlns=\"http://schemas.microsoft.com/DRM/2007/03/PlayReadyHeader\" "
    "version=\"4.0.0.0\"><DATA>"
    "<PROTECTINFO><KEYLEN>16</KEYLEN><ALGID>AESCTR</ALGID></PROTECTINFO>"
    "<KID>$0</KID><CHECKSUM>$1</CHECKSUM>"
    "$2</DATA></WRMHEADER>";

// For PlayReady clients 4.0+ that support CBC keys.
const std::string kPlayHeaderObject_4_3 =
    "<WRMHEADER "
    "xmlns=\"http://schemas.microsoft.com/DRM/2007/03/PlayReadyHeader\" "
    "version=\"4.3.0.0\"><DATA><PROTECTINFO><KIDS>"
    "<KID ALGID=\"AESCBC\" VALUE=\"$0\"></KID>"
    "</KIDS></PROTECTINFO>$1</DATA></WRMHEADER>";

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

void ReplaceString(std::string* str,
                   const std::string& from,
                   const std::string& to) {
  size_t location = str->find(from);
  if (location != std::string::npos) {
    str->replace(location, from.size(), to);
  }
}

void AesEcbEncrypt(const std::vector<uint8_t>& key,
                   const std::vector<uint8_t>& plaintext,
                   std::vector<uint8_t>* ciphertext) {
  CHECK_EQ(plaintext.size() % AES_BLOCK_SIZE, 0u);
  // mbedtls requires an extra block worth of output buffer.
  ciphertext->resize(plaintext.size() + AES_BLOCK_SIZE);

  mbedtls_cipher_context_t ctx;
  mbedtls_cipher_init(&ctx);

  const mbedtls_cipher_info_t* cipher_info =
      mbedtls_cipher_info_from_type(MBEDTLS_CIPHER_AES_128_ECB);
  CHECK(cipher_info);

  CHECK_EQ(mbedtls_cipher_setup(&ctx, cipher_info), 0) << "Cipher setup failed";

  CHECK_EQ(key.size(), 16u);
  CHECK_EQ(
      mbedtls_cipher_setkey(&ctx, key.data(), 8 * key.size(), MBEDTLS_ENCRYPT),
      0)
      << "Failed to set encryption key";

  size_t output_size = 0;
  CHECK_EQ(mbedtls_cipher_crypt(&ctx, /* iv= */ NULL, /* iv_len= */ 0,
                                plaintext.data(), plaintext.size(),
                                ciphertext->data(), &output_size),
           0);
  // Truncate the output to the correct size.
  ciphertext->resize(plaintext.size());

  mbedtls_cipher_free(&ctx);
}

// Generates the data section of a PlayReady PSSH.
// PlayReady PSSH Data is a PlayReady Header Object, which is described at
// https://docs.microsoft.com/en-us/playready/specifications/playready-header-specification
Status GeneratePlayReadyPsshData(const std::vector<uint8_t>& key_id,
                                 const std::vector<uint8_t>& key,
                                 const std::string& extra_header_data,
                                 const FourCC protection_scheme,
                                 std::vector<uint8_t>* output) {
  CHECK(output);
  std::vector<uint8_t> key_id_converted = ConvertGuidEndianness(key_id);

  std::vector<uint8_t> encrypted_key_id;
  AesEcbEncrypt(key, key_id_converted, &encrypted_key_id);

  std::string checksum =
      std::string(encrypted_key_id.begin(), encrypted_key_id.end())
          .substr(0, 8);
  std::string base64_checksum;
  absl::Base64Escape(checksum, &base64_checksum);
  std::string base64_key_id;
  absl::Base64Escape(
      std::string(key_id_converted.begin(), key_id_converted.end()),
      &base64_key_id);

  std::string playready_header;

  switch (protection_scheme) {
    case kAppleSampleAesProtectionScheme:
    case FOURCC_cbc1:
    case FOURCC_cbcs:
      playready_header = kPlayHeaderObject_4_3;
      ReplaceString(&playready_header, "$0", base64_key_id);
      ReplaceString(&playready_header, "$1", extra_header_data);
      break;

    case FOURCC_cenc:
    case FOURCC_cens:
      playready_header = kPlayHeaderObject_4_0;
      ReplaceString(&playready_header, "$0", base64_key_id);
      ReplaceString(&playready_header, "$1", base64_checksum);
      ReplaceString(&playready_header, "$2", extra_header_data);
      break;

    default:
      return Status(error::INVALID_ARGUMENT,
                    "The provided protection scheme is not supported.");
  }

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

PlayReadyPsshGenerator::PlayReadyPsshGenerator(
    const std::string& extra_header_data,
    FourCC protection_scheme)
    : PsshGenerator(std::vector<uint8_t>(std::begin(kPlayReadySystemId),
                                         std::end(kPlayReadySystemId)),
                    kPlayReadyPsshBoxVersion),
      extra_header_data_(extra_header_data),
      protection_scheme_(protection_scheme) {}

PlayReadyPsshGenerator::~PlayReadyPsshGenerator() {}

bool PlayReadyPsshGenerator::SupportMultipleKeys() {
  return false;
}

std::optional<std::vector<uint8_t>>
PlayReadyPsshGenerator::GeneratePsshDataFromKeyIdAndKey(
    const std::vector<uint8_t>& key_id,
    const std::vector<uint8_t>& key) const {
  std::vector<uint8_t> pssh_data;
  Status status = GeneratePlayReadyPsshData(key_id, key, extra_header_data_,
                                            protection_scheme_, &pssh_data);
  if (!status.ok()) {
    LOG(ERROR) << status.ToString();
    return std::nullopt;
  }

  return pssh_data;
}

std::optional<std::vector<uint8_t>>
PlayReadyPsshGenerator::GeneratePsshDataFromKeyIds(
    const std::vector<std::vector<uint8_t>>& key_ids) const {
  UNUSED(key_ids);
  NOTIMPLEMENTED();
  return std::nullopt;
}

}  // namespace media
}  // namespace shaka
