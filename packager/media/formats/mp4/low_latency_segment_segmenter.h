// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_FORMATS_MP4_LOW_LATENCY_SEGMENT_SEGMENTER_H_
#define PACKAGER_MEDIA_FORMATS_MP4_LOW_LATENCY_SEGMENT_SEGMENTER_H_

#include "packager/media/formats/mp4/segmenter.h"

namespace shaka {
namespace media {
namespace mp4 {

struct SegmentType;

/// TODO(Caitlin): Write description
/// Segmenter for low latency DASH profiles. There will be multiple
/// media segments, which will contain multiple fragments. The generated segments
/// are written as they are created to files defined by 
/// @b MuxerOptions.segment_template if specified; otherwise, the segments are 
/// appended to the main output file specified by @b MuxerOptions.output_file_name.
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
  Status DoFinalizeSubSegment() override;

  // Write segment to file.
  Status WriteInitSegment();
  Status WriteSegment();
  Status WriteSubSegment();

  std::unique_ptr<SegmentType> styp_;
  uint32_t num_segments_;
  bool is_first_subsegment_ = true;

  DISALLOW_COPY_AND_ASSIGN(LowLatencySegmentSegmenter);
};

}  // namespace mp4
}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_FORMATS_MP4_LOW_LATENCY_SEGMENT_SEGMENTER_H_
