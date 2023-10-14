// Copyright 2018 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_FORMATS_PACKED_AUDIO_PACKED_AUDIO_SEGMENTER_H_
#define PACKAGER_MEDIA_FORMATS_PACKED_AUDIO_PACKED_AUDIO_SEGMENTER_H_

#include <memory>

#include <packager/media/base/buffer_writer.h>
#include <packager/media/base/stream_info.h>
#include <packager/status.h>

namespace shaka {
namespace media {

class AACAudioSpecificConfig;
class Id3Tag;
class MediaSample;

/// PackedAudio uses transport stream timescale.
constexpr double kPackedAudioTimescale = 90000;

/// https://tools.ietf.org/html/draft-pantos-http-live-streaming-23#section-3.4
/// Timestamp is carried inside an ID3 PRIV tag with identifier:
constexpr char kTimestampOwnerIdentifier[] =
    "com.apple.streaming.transportStreamTimestamp";

/// http://goo.gl/FPhLma 2.4.3.4 Elementary Stream Setup for FairPlay streaming
/// Audio setup information is carried inside an ID3 PRIV tag with identifier:
constexpr char kAudioDescriptionOwnerIdentifier[] =
    "com.apple.streaming.audioDescription";

/// Implements packed audio segment writer.
/// https://tools.ietf.org/html/draft-pantos-http-live-streaming-23#section-3.4
/// A Packed Audio Segment contains encoded audio samples and ID3 tags that are
/// simply packed together with minimal framing and no per-sample timestamps.
class PackedAudioSegmenter {
 public:
  /// @param transport_stream_timestamp_offset is the offset to be applied to
  ///        sample timestamps to compensate for possible negative timestamps in
  ///        the input.
  explicit PackedAudioSegmenter(int32_t transport_stream_timestamp_offset);
  virtual ~PackedAudioSegmenter();

  /// Initialize the object.
  /// @param stream_info is the stream info for the segmenter.
  /// @return OK on success.
  // This function is made virtual for testing.
  virtual Status Initialize(const StreamInfo& stream_info);

  /// @param sample gets added to this object.
  /// @return OK on success.
  // This function is made virtual for testing.
  virtual Status AddSample(const MediaSample& sample);

  /// Flush all the samples that are (possibly) buffered and write them to the
  /// current segment.
  /// @return OK on success.
  // This function is made virtual for testing.
  virtual Status FinalizeSegment();

  /// @return The scale for converting timestamp in input stream's scale to
  ///         output stream's scale.
  // This function is made virtual for testing.
  virtual double TimescaleScale() const;

  /// @return A pointer to the buffer for the current segment.
  BufferWriter* segment_buffer() { return &segment_buffer_; }

 private:
  PackedAudioSegmenter(const PackedAudioSegmenter&) = delete;
  PackedAudioSegmenter& operator=(const PackedAudioSegmenter&) = delete;

  // These functions is made virtual for testing.
  virtual std::unique_ptr<AACAudioSpecificConfig> CreateAdtsConverter();
  virtual std::unique_ptr<Id3Tag> CreateId3Tag();

  Status EncryptionAudioSetup(const MediaSample& sample);
  Status StartNewSegment(const MediaSample& first_sample);

  const int32_t transport_stream_timestamp_offset_ = 0;
  // Codec for the stream.
  Codec codec_ = kUnknownCodec;
  std::vector<uint8_t> audio_codec_config_;
  // Calculated by output stream's timescale / input stream's timescale. This is
  // used to scale the timestamps.
  double timescale_scale_ = 0.0;
  // Whether it is the start of a new segment.
  bool start_of_new_segment_ = true;

  // Audio setup information for encrypted segment.
  std::string audio_setup_information_;
  // AAC is carried in ADTS.
  std::unique_ptr<AACAudioSpecificConfig> adts_converter_;

  BufferWriter segment_buffer_;
};

}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_FORMATS_PACKED_AUDIO_PACKED_AUDIO_SEGMENTER_H_
