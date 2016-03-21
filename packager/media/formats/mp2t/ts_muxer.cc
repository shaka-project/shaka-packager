// Copyright 2016 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/formats/mp2t/ts_muxer.h"

namespace edash_packager {
namespace media {
namespace mp2t {

TsMuxer::TsMuxer(const MuxerOptions& muxer_options)
    : Muxer(muxer_options), segmenter_(options()) {}
TsMuxer::~TsMuxer() {}

Status TsMuxer::Initialize() {
  if (streams().size() > 1u)
    return Status(error::MUXER_FAILURE, "Cannot handle more than one streams.");
  return segmenter_.Initialize(*streams()[0]->info());
}

Status TsMuxer::Finalize() {
  return segmenter_.Finalize();
}

Status TsMuxer::DoAddSample(const MediaStream* stream,
                            scoped_refptr<MediaSample> sample) {
  return segmenter_.AddSample(sample);
}

}  // namespace mp2t
}  // namespace media
}  // namespace edash_packager
