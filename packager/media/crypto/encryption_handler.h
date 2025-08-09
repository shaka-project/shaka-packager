// Copyright 2017 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_CRYPTO_ENCRYPTION_HANDLER_H_
#define PACKAGER_MEDIA_CRYPTO_ENCRYPTION_HANDLER_H_

#include <cstdint>

#include <packager/crypto_params.h>
#include <packager/media/base/key_source.h>
#include <packager/media/base/media_handler.h>

namespace shaka {
namespace media {

class AesCryptor;
class AesEncryptorFactory;
class SubsampleGenerator;
struct EncryptionKey;

class EncryptionHandler : public MediaHandler {
 public:
  EncryptionHandler(const EncryptionParams& encryption_params,
                    KeySource* key_source);

  ~EncryptionHandler() override;

 protected:
  /// @name MediaHandler implementation overrides.
  /// @{
  Status InitializeInternal() override;
  Status Process(std::unique_ptr<StreamData> stream_data) override;
  /// @}

 private:
  friend class EncryptionHandlerTest;

  EncryptionHandler(const EncryptionHandler&) = delete;
  EncryptionHandler& operator=(const EncryptionHandler&) = delete;

  // Processes |stream_info| and sets up stream specific variables.
  Status ProcessStreamInfo(const StreamInfo& stream_info);
  // Processes media sample and encrypts it if needed.
  Status ProcessMediaSample(std::shared_ptr<const MediaSample> clear_sample);

  void SetupProtectionPattern(StreamType stream_type);
  bool CreateEncryptor(const EncryptionKey& encryption_key);
  // Encrypt an E-AC3 frame with size |source_size| according to SAMPLE-AES
  // specification. |dest| should have at least |source_size| bytes.
  bool SampleAesEncryptEac3Frame(const uint8_t* source,
                                 size_t source_size,
                                 uint8_t* dest);
  // Encrypt an array with size |source_size|. |dest| should have at
  // least |source_size| bytes.
  void EncryptBytes(const uint8_t* source,
                    size_t source_size,
                    uint8_t* dest,
                    size_t dest_size);

  // An E-AC3 frame comprises of one or more syncframes. This function extracts
  // the syncframe sizes from the source bytes.
  // Returns false if the frame is not well formed.
  bool ExtractEac3SyncframeSizes(const uint8_t* source,
                                 size_t source_size,
                                 std::vector<size_t>* syncframe_sizes);

  // Testing injections.
  void InjectSubsampleGeneratorForTesting(
      std::unique_ptr<SubsampleGenerator> generator);
  void InjectEncryptorFactoryForTesting(
      std::unique_ptr<AesEncryptorFactory> encryptor_factory);

  const EncryptionParams encryption_params_;
  const FourCC protection_scheme_ = FOURCC_NULL;
  KeySource* key_source_ = nullptr;
  std::string stream_label_;
  // Current encryption config and encryptor.
  std::shared_ptr<EncryptionConfig> encryption_config_;
  std::unique_ptr<AesCryptor> encryptor_;
  Codec codec_ = kUnknownCodec;
  // Remaining clear lead in the stream's time scale.
  int64_t remaining_clear_lead_ = 0;
  // Crypto period duration in the stream's time scale.
  int64_t crypto_period_duration_ = 0;
  // Previous crypto period index if key rotation is enabled.
  int64_t prev_crypto_period_index_ = -1;
  bool check_new_crypto_period_ = false;

  std::unique_ptr<SubsampleGenerator> subsample_generator_;
  std::unique_ptr<AesEncryptorFactory> encryptor_factory_;
  // Number of encrypted blocks (16-byte-block) in pattern based encryption.
  uint8_t crypt_byte_block_ = 0;
  /// Number of unencrypted blocks (16-byte-block) in pattern based encryption.
  uint8_t skip_byte_block_ = 0;
};

}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_CRYPTO_ENCRYPTION_HANDLER_H_
