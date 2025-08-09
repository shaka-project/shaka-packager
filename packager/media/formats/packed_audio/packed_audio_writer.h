// Copyright 2018 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_FORMATS_PACKED_AUDIO_PACKED_AUDIO_WRITER_H_
#define PACKAGER_MEDIA_FORMATS_PACKED_AUDIO_PACKED_AUDIO_WRITER_H_

#include <cstdint>

#include <packager/file/file_closer.h>
#include <packager/media/base/muxer.h>

namespace shaka {
namespace media {

class BufferWriter;
class PackedAudioSegmenter;

/// Implements packed audio writer.
/// https://tools.ietf.org/html/draft-pantos-http-live-streaming-23#section-3.4
/// A Packed Audio Segment contains encoded audio samples and ID3 tags that are
/// simply packed together with minimal framing and no per-sample timestamps.
class PackedAudioWriter : public Muxer {
 public:
  /// Create a MP4Muxer object from MuxerOptions.
  explicit PackedAudioWriter(const MuxerOptions& muxer_options);
  ~PackedAudioWriter() override;

 private:
  friend class PackedAudioWriterTest;

  PackedAudioWriter(const PackedAudioWriter&) = delete;
  PackedAudioWriter& operator=(const PackedAudioWriter&) = delete;

  // Muxer implementations.
  Status InitializeMuxer() override;
  Status Finalize() override;
  Status AddMediaSample(size_t stream_id, const MediaSample& sample) override;
  Status FinalizeSegment(size_t stream_id, const SegmentInfo& sample) override;

  Status WriteSegment(const std::string& segment_path,
                      BufferWriter* segment_buffer);

  Status CloseFile(std::unique_ptr<File, FileCloser> file);

  const int32_t transport_stream_timestamp_offset_ = 0;
  std::unique_ptr<PackedAudioSegmenter> segmenter_;

  // Used in single segment mode.
  std::unique_ptr<File, FileCloser> output_file_;
  // Keeps track of segment ranges in single segment mode.
  MuxerListener::MediaRanges media_ranges_;
  int64_t total_duration_ = 0;

  // Used in multi-segment mode for segment template.
  uint64_t segment_number_ = 0;
};

}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_FORMATS_PACKED_AUDIO_PACKED_AUDIO_WRITER_H_
