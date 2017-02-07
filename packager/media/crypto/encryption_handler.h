// Copyright 2017 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_CRYPTO_ENCRYPTION_HANDLER_H_
#define PACKAGER_MEDIA_CRYPTO_ENCRYPTION_HANDLER_H_

#include "packager/media/base/key_source.h"
#include "packager/media/base/media_handler.h"

namespace shaka {
namespace media {

class AesCryptor;
class VideoSliceHeaderParser;
class VPxParser;
struct EncryptionKey;
struct VPxFrameInfo;

/// This structure defines encryption options.
struct EncryptionOptions {
  /// Clear lead duration in seconds.
  double clear_lead_in_seconds = 0;
  /// The protection scheme: 'cenc', 'cens', 'cbc1', 'cbcs'.
  FourCC protection_scheme = FOURCC_cenc;
  /// The threshold to determine whether a video track should be considered as
  /// SD. If the max pixels per frame is no higher than max_sd_pixels, i.e.
  /// [0, max_sd_pixels], it is SD.
  uint32_t max_sd_pixels = 0;
  /// The threshold to determine whether a video track should be considered as
  /// HD. If the max pixels per frame is higher than max_sd_pixels, but no
  /// higher than max_hd_pixels, i.e. (max_sd_pixels, max_hd_pixels], it is HD.
  uint32_t max_hd_pixels = 0;
  /// The threshold to determine whether a video track should be considered as
  /// UHD1. If the max pixels per frame is higher than max_hd_pixels, but no
  /// higher than max_uhd1_pixels, i.e. (max_hd_pixels, max_uhd1_pixels], it is
  /// UHD1. Otherwise it is UHD2.
  uint32_t max_uhd1_pixels = 0;
  /// Crypto period duration in seconds. A positive value means key rotation is
  /// enabled, the key source must support key rotation in this case.
  double crypto_period_duration_in_seconds = 0;
};

class EncryptionHandler : public MediaHandler {
 public:
  EncryptionHandler(const EncryptionOptions& encryption_options,
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
  Status ProcessStreamInfo(StreamInfo* stream_info);
  // Processes media sample and encrypts it if needed.
  Status ProcessMediaSample(MediaSample* sample);

  bool CreateEncryptor(EncryptionKey* encryption_key);
  bool EncryptVpxFrame(const std::vector<VPxFrameInfo>& vpx_frames,
                       MediaSample* sample,
                       DecryptConfig* decrypt_config);
  bool EncryptNalFrame(MediaSample* sample, DecryptConfig* decrypt_config);
  void EncryptBytes(uint8_t* data, size_t size);

  // Testing injections.
  void InjectVpxParserForTesting(std::unique_ptr<VPxParser> vpx_parser);
  void InjectVideoSliceHeaderParserForTesting(
      std::unique_ptr<VideoSliceHeaderParser> header_parser);

  const EncryptionOptions encryption_options_;
  KeySource* key_source_ = nullptr;
  KeySource::TrackType track_type_ = KeySource::TRACK_TYPE_UNKNOWN;
  std::unique_ptr<AesCryptor> encryptor_;
  // Specifies the size of NAL unit length in bytes. Can be 1, 2 or 4 bytes. 0
  // if it is not a NAL structured video.
  uint8_t nalu_length_size_ = 0;
  Codec video_codec_ = kUnknownCodec;
  // Remaining clear lead in the stream's time scale.
  int64_t remaining_clear_lead_ = 0;
  // Crypto period duration in the stream's time scale.
  uint64_t crypto_period_duration_ = 0;
  // Previous crypto period index if key rotation is enabled.
  int64_t prev_crypto_period_index_ = -1;
  bool new_segment_ = true;

  // Number of encrypted blocks (16-byte-block) in pattern based encryption.
  uint8_t crypt_byte_block_ = 0;
  /// Number of unencrypted blocks (16-byte-block) in pattern based encryption.
  uint8_t skip_byte_block_ = 0;

  // Current key id.
  std::vector<uint8_t> key_id_;
  // VPx parser for VPx streams.
  std::unique_ptr<VPxParser> vpx_parser_;
  // Video slice header parser for NAL strucutred streams.
  std::unique_ptr<VideoSliceHeaderParser> header_parser_;
};

}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_CRYPTO_ENCRYPTION_HANDLER_H_
