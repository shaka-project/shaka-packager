// Copyright 2014 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_FORMATS_MP4_MULTI_SEGMENT_SEGMENTER_H_
#define PACKAGER_MEDIA_FORMATS_MP4_MULTI_SEGMENT_SEGMENTER_H_

#include <packager/macros/classes.h>
#include <packager/media/formats/mp4/segmenter.h>

namespace shaka {
namespace media {
namespace mp4 {

struct SegmentType;

/// Segmenter for MP4 live, main and simple profiles. There can be multiple
/// media segments, which can contain multiple fragments. The generated segments
/// are written to files defined by @b MuxerOptions.segment_template if
/// specified; otherwise, the segments are appended to the main output file
/// specified by @b MuxerOptions.output_file_name.
class MultiSegmentSegmenter : public Segmenter {
 public:
  MultiSegmentSegmenter(const MuxerOptions& options,
                        std::unique_ptr<FileType> ftyp,
                        std::unique_ptr<Movie> moov);
  ~MultiSegmentSegmenter() override;

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

  // Write segment to file.
  Status WriteInitSegment();
  Status WriteSegment();

  std::unique_ptr<SegmentType> styp_;
  uint32_t num_segments_;

  DISALLOW_COPY_AND_ASSIGN(MultiSegmentSegmenter);
};

}  // namespace mp4
}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_FORMATS_MP4_MULTI_SEGMENT_SEGMENTER_H_
