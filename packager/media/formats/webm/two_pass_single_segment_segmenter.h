// Copyright 2015 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef MEDIA_FORMATS_WEBM_TWO_PASS_SINGLE_SEGMENT_SEGMENTER_H_
#define MEDIA_FORMATS_WEBM_TWO_PASS_SINGLE_SEGMENT_SEGMENTER_H_

#include "packager/media/formats/webm/single_segment_segmenter.h"

#include "packager/base/memory/scoped_ptr.h"
#include "packager/media/base/status.h"
#include "packager/media/formats/webm/mkv_writer.h"

namespace edash_packager {
namespace media {

struct MuxerOptions;

namespace webm {

/// An implementation of a Segmenter for a single-segment that performs two
/// passes.  This does not use seeking and is used for non-seekable files.
class TwoPassSingleSegmentSegmenter : public SingleSegmentSegmenter {
 public:
  explicit TwoPassSingleSegmentSegmenter(const MuxerOptions& options);
  ~TwoPassSingleSegmentSegmenter() override;

  // Segmenter implementation overrides.
  Status DoInitialize(scoped_ptr<MkvWriter> writer) override;
  Status DoFinalize() override;

 private:
  /// Copies the data from source to destination while rewriting the Cluster
  /// sizes to the correct values.  This assumes that both @a source and
  /// @a dest are at the same position and that the headers have already
  /// been copied.
  bool CopyFileWithClusterRewrite(File* source,
                                  MkvWriter* dest,
                                  uint64_t last_size);

  scoped_ptr<MkvWriter> real_writer_;

  DISALLOW_COPY_AND_ASSIGN(TwoPassSingleSegmentSegmenter);
};

}  // namespace webm
}  // namespace media
}  // namespace edash_packager

#endif  // MEDIA_FORMATS_WEBM_TWO_PASS_SINGLE_SEGMENT_SEGMENTER_H_
