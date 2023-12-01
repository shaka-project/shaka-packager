// Copyright 2014 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_FORMATS_MP4_LOW_LATENCY_SEGMENT_SEGMENTER_H_
#define PACKAGER_MEDIA_FORMATS_MP4_LOW_LATENCY_SEGMENT_SEGMENTER_H_

#include <packager/file.h>
#include <packager/file/file_closer.h>
#include <packager/macros/classes.h>
#include <packager/media/formats/mp4/segmenter.h>

namespace shaka {
namespace media {
namespace mp4 {

struct SegmentType;

/// Segmenter for LL-DASH profiles.
/// Each segment constist of many fragments, and each fragment contains one
/// chunk. A chunk is the smallest unit and is constructed of a single moof and
/// mdat atom. A chunk is be generated for each recieved @b MediaSample. The
/// generated chunks are written as they are created to files defined by
/// @b MuxerOptions.segment_template if specified; otherwise, the chunks are
/// appended to the main output file specified by @b
/// MuxerOptions.output_file_name.
class LowLatencySegmentSegmenter : public Segmenter {
 public:
  LowLatencySegmentSegmenter(const MuxerOptions& options,
                             std::unique_ptr<FileType> ftyp,
                             std::unique_ptr<Movie> moov);
  ~LowLatencySegmentSegmenter() override;

  /// @name Segmenter implementation overrides.
  /// @{
  bool GetInitRange(size_t* offset, size_t* size) override;
  bool GetIndexRange(size_t* offset, size_t* size) override;
  std::vector<Range> GetSegmentRanges() override;
  /// @}

 private:
  // Segmenter implementation overrides.
  Status DoInitialize() override;
  Status DoFinalize() override;
  Status DoFinalizeSegment() override;
  Status DoFinalizeChunk() override;

  // Write segment to file.
  Status WriteInitSegment();
  Status WriteChunk();
  Status WriteInitialChunk();
  Status FinalizeSegment();

  uint64_t GetSegmentDuration();

  std::unique_ptr<SegmentType> styp_;
  uint32_t num_segments_;
  bool is_initial_chunk_in_seg_ = true;
  bool ll_dash_mpd_values_initialized_ = false;
  std::unique_ptr<File, FileCloser> segment_file_;
  std::string file_name_;
  size_t segment_size_ = 0u;

  DISALLOW_COPY_AND_ASSIGN(LowLatencySegmentSegmenter);
};

}  // namespace mp4
}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_FORMATS_MP4_LOW_LATENCY_SEGMENT_SEGMENTER_H_
