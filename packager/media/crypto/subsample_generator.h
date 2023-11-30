// Copyright 2018 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_CRYPTO_SUBSAMPLE_GENERATOR_H_
#define PACKAGER_MEDIA_CRYPTO_SUBSAMPLE_GENERATOR_H_

#include <memory>
#include <vector>

#include <packager/media/base/fourccs.h>
#include <packager/media/base/stream_info.h>
#include <packager/status.h>

namespace shaka {
namespace media {

class AV1Parser;
class VideoSliceHeaderParser;
class VPxParser;
struct SubsampleEntry;

/// Parsing and generating encryption subsamples from bitstreams. Note that the
/// class can be used to generate subsamples from both audio and video
/// bitstreams according to relevant specifications. For example, for video
/// streams, the most notable specifications are Common Encryption v3 spec [1]
/// and Apple SAMPLE AES spec [2]; for audio streams, they are full sample
/// encrypted except for Apple SAMPLE AES spec, which usually contains leading
/// clear bytes.
/// [1] https://www.iso.org/standard/68042.html
/// [2] https://apple.co/2Noi43q
class SubsampleGenerator {
 public:
  /// @param vp9_subsample_encryption determines if subsample encryption or full
  ///        sample encryption is used for VP9. Only relevant for VP9 codec.
  explicit SubsampleGenerator(bool vp9_subsample_encryption);

  virtual ~SubsampleGenerator();

  /// Initialize the generator.
  /// @param protection_scheme is the protection scheme to be used for the
  ///        encryption. It is used to determine if the protected data should be
  ///        block aligned.
  /// @param stream_info contains stream information.
  /// @returns OK on success, an error status otherwise.
  virtual Status Initialize(FourCC protection_scheme,
                            const StreamInfo& stream_info);

  /// Generates subsamples from the bitstream. Note that all frames should be
  /// processed by this function even if it is not encrypted as the next
  /// (encrypted) frame may be dependent on the previous clear frames.
  /// @param frame points to the start of the frame.
  /// @param frame_size is the size of the frame.
  /// @param[out] subsamples will contain the output subsamples on success. It
  ///             will be empty if the frame should be full sample encrypted.
  /// @returns OK on success, an error status otherwise.
  virtual Status GenerateSubsamples(const uint8_t* frame,
                                    size_t frame_size,
                                    std::vector<SubsampleEntry>* subsamples);

  // Testing injections.
  void InjectVpxParserForTesting(std::unique_ptr<VPxParser> vpx_parser);
  void InjectVideoSliceHeaderParserForTesting(
      std::unique_ptr<VideoSliceHeaderParser> header_parser);
  void InjectAV1ParserForTesting(std::unique_ptr<AV1Parser> av1_parser);

 private:
  SubsampleGenerator(const SubsampleGenerator&) = delete;
  SubsampleGenerator& operator=(const SubsampleGenerator&) = delete;

  Status GenerateSubsamplesFromVPxFrame(
      const uint8_t* frame,
      size_t frame_size,
      std::vector<SubsampleEntry>* subsamples);
  Status GenerateSubsamplesFromH26xFrame(
      const uint8_t* frame,
      size_t frame_size,
      std::vector<SubsampleEntry>* subsamples);
  Status GenerateSubsamplesFromAV1Frame(
      const uint8_t* frame,
      size_t frame_size,
      std::vector<SubsampleEntry>* subsamples);

  const bool vp9_subsample_encryption_ = false;
  // Whether the protected portion should be AES block (16 bytes) aligned.
  bool align_protected_data_ = false;
  Codec codec_ = kUnknownCodec;
  // For NAL structured video only, the size of NAL unit length in bytes. Can be
  // 1, 2 or 4 bytes.
  uint8_t nalu_length_size_ = 0;
  // For SAMPLE AES only, 32 bytes for Video and 16 bytes for audio.
  size_t leading_clear_bytes_size_ = 0;
  // For SAMPLE AES only, if the data size is less than this value, none of the
  // bytes are encrypted. The size is 48+1 bytes for video NAL and 32 bytes for
  // audio according to MPEG-2 Stream Encryption Format for HTTP Live Streaming.
  size_t min_protected_data_size_ = 0;

  // VPx parser for VPx streams.
  std::unique_ptr<VPxParser> vpx_parser_;
  // Video slice header parser for NAL strucutred streams.
  std::unique_ptr<VideoSliceHeaderParser> header_parser_;
  // AV1 parser for AV1 streams.
  std::unique_ptr<AV1Parser> av1_parser_;
};

}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_CRYPTO_SUBSAMPLE_GENERATOR_H_
