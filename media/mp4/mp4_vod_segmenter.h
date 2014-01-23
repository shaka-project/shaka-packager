// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef MEDIA_MP4_MP4_VOD_SEGMENTER_H_
#define MEDIA_MP4_MP4_VOD_SEGMENTER_H_

#include "media/file/file_closer.h"
#include "media/mp4/mp4_segmenter.h"

namespace media {
namespace mp4 {

/// Segmenter for MP4 Dash Video-On-Demand profile. A single MP4 file with a
/// single segment is created, i.e. with only one SIDX box. The generated media
/// file can contain one or many subsegments with subsegment duration
/// defined by @b MuxerOptions.segment_duration. A subsegment can contain one
/// or many fragments with fragment duration defined by @b
/// MuxerOptions.fragment_duration. The actual subsegment or fragment duration
/// may not match the requested duration exactly, but will be approximated. That
/// is, the segmenter tries to end subsegment/fragment at the first sample with
/// overall subsegment/fragment duration not smaller than defined duration and
/// yet meet SAP requirements. VOD segmenter ignores @b
/// MuxerOptions.num_subsegments_per_sidx.
class MP4VODSegmenter : public MP4Segmenter {
 public:
  MP4VODSegmenter(const MuxerOptions& options,
                  scoped_ptr<FileType> ftyp,
                  scoped_ptr<Movie> moov);
  virtual ~MP4VODSegmenter();

  /// @name MP4Segmenter implementation overrides.
  /// @{
  virtual Status Initialize(EncryptorSource* encryptor_source,
                            double clear_lead_in_seconds,
                            const std::vector<MediaStream*>& streams) OVERRIDE;
  virtual Status Finalize() OVERRIDE;

  virtual bool GetInitRange(size_t* offset, size_t* size) OVERRIDE;
  virtual bool GetIndexRange(size_t* offset, size_t* size) OVERRIDE;
  /// @}

 protected:
  virtual Status FinalizeSegment() OVERRIDE;

 private:
  scoped_ptr<SegmentIndex> vod_sidx_;
  scoped_ptr<File, FileCloser> temp_file_;

  DISALLOW_COPY_AND_ASSIGN(MP4VODSegmenter);
};

}  // namespace mp4
}  // namespace media

#endif  // MEDIA_MP4_MP4_VOD_SEGMENTER_H_
