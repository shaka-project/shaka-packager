// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef MEDIA_FORMATS_MP4_SINGLE_SEGMENT_SEGMENTER_H_
#define MEDIA_FORMATS_MP4_SINGLE_SEGMENT_SEGMENTER_H_

#include "media/file/file_closer.h"
#include "media/formats/mp4/segmenter.h"

namespace edash_packager {
namespace media {
namespace mp4 {

/// Segmenter for MP4 Dash Video-On-Demand profile. A single MP4 file with a
/// single segment is created, i.e. with only one SIDX box. The generated media
/// file can contain one or many subsegments with subsegment duration
/// defined by @b MuxerOptions.segment_duration. A subsegment can contain one
/// or many fragments with fragment duration defined by @b
/// MuxerOptions.fragment_duration. The actual subsegment or fragment duration
/// may not match the requested duration exactly, but will be approximated. That
/// is, the Segmenter tries to end subsegment/fragment at the first sample with
/// overall subsegment/fragment duration not smaller than defined duration and
/// yet meet SAP requirements. SingleSegmentSegmenter ignores @b
/// MuxerOptions.num_subsegments_per_sidx.
class SingleSegmentSegmenter : public Segmenter {
 public:
  SingleSegmentSegmenter(const MuxerOptions& options,
                         scoped_ptr<FileType> ftyp,
                         scoped_ptr<Movie> moov);
  virtual ~SingleSegmentSegmenter();

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

  scoped_ptr<SegmentIndex> vod_sidx_;
  std::string temp_file_name_;
  scoped_ptr<File, FileCloser> temp_file_;

  DISALLOW_COPY_AND_ASSIGN(SingleSegmentSegmenter);
};

}  // namespace mp4
}  // namespace media
}  // namespace edash_packager

#endif  // MEDIA_FORMATS_MP4_SINGLE_SEGMENT_SEGMENTER_H_
