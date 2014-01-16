// Copyright (c) 2013 Google Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Segmenter for MP4 live, main and simple profiles. The generated media file
// could contain one to many segments with segment duration defined by
// |MuxerOptions.segment_duration|. A segment could contain one to many
// subsegments defined by |num_subsegments_per_sidx|. A subsegment could
// contain one to many fragments with fragment duration defined by
// |MuxerOptions.fragment_duration|. The actual segment or fragment duration
// may not match the defined duration exactly but in a best effort basic, i.e.
// the segmenter tries to end segment/fragment at the first sample with
// overall segment/fragment duration not smaller than defined duration and
// yet meet SAP requirements. The generated segments are written into files
// defined by |MuxerOptions.segment_template| if it is defined; otherwise,
// the segments are appended to the main output file defined by
// |MuxerOptions.output_file_name|.

#ifndef MEDIA_MP4_MP4_GENERAL_SEGMENTER_H_
#define MEDIA_MP4_MP4_GENERAL_SEGMENTER_H_

#include "media/mp4/mp4_segmenter.h"

namespace media {
namespace mp4 {

struct SegmentType;

class MP4GeneralSegmenter : public MP4Segmenter {
 public:
  // Caller transfers the ownership of |ftyp| and |moov| to this class.
  MP4GeneralSegmenter(const MuxerOptions& options,
                      scoped_ptr<FileType> ftyp,
                      scoped_ptr<Movie> moov);
  virtual ~MP4GeneralSegmenter();

  // MP4Segmenter implementations.
  virtual Status Initialize(EncryptorSource* encryptor_source,
                            double clear_lead_in_seconds,
                            const std::vector<MediaStream*>& streams) OVERRIDE;

  virtual bool GetInitRange(size_t* offset, size_t* size) OVERRIDE;
  virtual bool GetIndexRange(size_t* offset, size_t* size) OVERRIDE;

 protected:
  virtual Status FinalizeSegment() OVERRIDE;

 private:
  // Write segment to file.
  Status WriteSegment();

  scoped_ptr<SegmentType> styp_;
  uint32 num_segments_;

  DISALLOW_COPY_AND_ASSIGN(MP4GeneralSegmenter);
};

}  // namespace mp4
}  // namespace media

#endif  // MEDIA_MP4_MP4_GENERAL_SEGMENTER_H_
