// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef MEDIA_FORMATS_MP4_MULTI_SEGMENT_SEGMENTER_H_
#define MEDIA_FORMATS_MP4_MULTI_SEGMENT_SEGMENTER_H_

#include "media/formats/mp4/segmenter.h"

namespace edash_packager {
namespace media {
namespace mp4 {

struct SegmentType;

/// Segmenter for MP4 live, main and simple profiles. The generated media file
/// can contain one or many segments with segment duration defined by @b
/// MuxerOptions.segment_duration. A segment can contain one or many
/// subsegments defined by @b num_subsegments_per_sidx. A subsegment can
/// contain one or many fragments with fragment duration defined by @b
/// MuxerOptions.fragment_duration. The actual segment or fragment duration
/// may not match the requested duration exactly, but will be approximated.
/// That is, the Segmenter tries to end segment/fragment at the first sample
/// with overall segment/fragment duration not smaller than defined duration
/// and yet meet SAP requirements. The generated segments are written to files
/// defined by @b MuxerOptions.segment_template if specified; otherwise,
/// the segments are appended to the main output file specified by @b
/// MuxerOptions.output_file_name.
class MultiSegmentSegmenter : public Segmenter {
 public:
  MultiSegmentSegmenter(const MuxerOptions& options,
                        scoped_ptr<FileType> ftyp,
                        scoped_ptr<Movie> moov);
  virtual ~MultiSegmentSegmenter();

  /// @name Segmenter implementation overrides.
  /// @{
  virtual bool GetInitRange(size_t* offset, size_t* size) OVERRIDE;
  virtual bool GetIndexRange(size_t* offset, size_t* size) OVERRIDE;
  /// @}

 private:
  // Segmenter implementation overrides.
  virtual Status DoInitialize() OVERRIDE;
  virtual Status DoFinalize() OVERRIDE;
  virtual Status DoFinalizeSegment() OVERRIDE;

  // Write segment to file.
  Status WriteSegment();

  scoped_ptr<SegmentType> styp_;
  uint32 num_segments_;

  DISALLOW_COPY_AND_ASSIGN(MultiSegmentSegmenter);
};

}  // namespace mp4
}  // namespace media
}  // namespace edash_packager

#endif  // MEDIA_FORMATS_MP4_MULTI_SEGMENT_SEGMENTER_H_
