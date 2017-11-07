// Copyright 2017 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_CRYPTO_ENCRYPTION_HANDLER_H_
#define PACKAGER_MEDIA_CRYPTO_ENCRYPTION_HANDLER_H_

#include "packager/media/base/key_source.h"
#include "packager/media/base/media_handler.h"
#include "packager/media/public/crypto_params.h"

namespace shaka {
namespace media {

class AesCryptor;
class VideoSliceHeaderParser;
class VPxParser;
struct EncryptionKey;
struct VPxFrameInfo;

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

  Status SetupProtectionPattern(StreamType stream_type);
  bool CreateEncryptor(const EncryptionKey& encryption_key);
  // Encrypt a VPx frame with size |source_size|. |dest| should have at least
  // |source_size| bytes.
  bool EncryptVpxFrame(const std::vector<VPxFrameInfo>& vpx_frames,
                       const uint8_t* source,
                       size_t source_size,
                       uint8_t* dest,
                       DecryptConfig* decrypt_config);
  // Encrypt a NAL unit frame with size |source_size|. |dest| should have at
  // least |source_size| bytes.
  bool EncryptNalFrame(const uint8_t* source,
                       size_t source_size,
                       uint8_t* dest,
                       DecryptConfig* decrypt_config);
  // Encrypt an E-AC3 frame with size |source_size| according to SAMPLE-AES
  // specification. |dest| should have at least |source_size| bytes.
  bool SampleAesEncryptEac3Frame(const uint8_t* source,
                                 size_t source_size,
                                 uint8_t* dest);
  // Encrypt an array with size |source_size|. |dest| should have at
  // least |source_size| bytes.
  void EncryptBytes(const uint8_t* source, size_t source_size, uint8_t* dest);

  // An E-AC3 frame comprises of one or more syncframes. This function extracts
  // the syncframe sizes from the source bytes.
  // Returns false if the frame is not well formed.
  bool ExtractEac3SyncframeSizes(const uint8_t* source,
                                 size_t source_size,
                                 std::vector<size_t>* syncframe_sizes);

  // Testing injections.
  void InjectVpxParserForTesting(std::unique_ptr<VPxParser> vpx_parser);
  void InjectVideoSliceHeaderParserForTesting(
      std::unique_ptr<VideoSliceHeaderParser> header_parser);

  const EncryptionParams encryption_params_;
  const FourCC protection_scheme_ = FOURCC_NULL;
  KeySource* key_source_ = nullptr;
  std::string stream_label_;
  // Current encryption config and encryptor.
  std::shared_ptr<EncryptionConfig> encryption_config_;
  std::unique_ptr<AesCryptor> encryptor_;
  Codec codec_ = kUnknownCodec;
  // Specifies the size of NAL unit length in bytes. Can be 1, 2 or 4 bytes. 0
  // if it is not a NAL structured video.
  uint8_t nalu_length_size_ = 0;
  // For Sample AES, 32 bytes for Video and 16 bytes for audio.
  size_t leading_clear_bytes_size_ = 0;
  // For Sample AES, if the data size is less than this value, none of the bytes
  // are encrypted. The size is 48+1 bytes for video NAL and 16+15 bytes for
  // audio according to MPEG-2 Stream Encryption Format for HTTP Live Streaming.
  size_t min_protected_data_size_ = 0;
  // Remaining clear lead in the stream's time scale.
  int64_t remaining_clear_lead_ = 0;
  // Crypto period duration in the stream's time scale.
  uint64_t crypto_period_duration_ = 0;
  // Previous crypto period index if key rotation is enabled.
  int64_t prev_crypto_period_index_ = -1;
  bool check_new_crypto_period_ = false;

  // Number of encrypted blocks (16-byte-block) in pattern based encryption.
  uint8_t crypt_byte_block_ = 0;
  /// Number of unencrypted blocks (16-byte-block) in pattern based encryption.
  uint8_t skip_byte_block_ = 0;

  // VPx parser for VPx streams.
  std::unique_ptr<VPxParser> vpx_parser_;
  // Video slice header parser for NAL strucutred streams.
  std::unique_ptr<VideoSliceHeaderParser> header_parser_;
};

}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_CRYPTO_ENCRYPTION_HANDLER_H_
