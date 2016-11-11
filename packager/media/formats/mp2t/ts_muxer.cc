// Copyright 2016 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/formats/mp2t/ts_muxer.h"

namespace shaka {
namespace media {
namespace mp2t {

namespace {
const uint32_t kTsTimescale = 90000;
}  // namespace

TsMuxer::TsMuxer(const MuxerOptions& muxer_options) : Muxer(muxer_options) {}
TsMuxer::~TsMuxer() {}

Status TsMuxer::Initialize() {
  if (streams().size() > 1u)
    return Status(error::MUXER_FAILURE, "Cannot handle more than one streams.");

  segmenter_.reset(new TsSegmenter(options(), muxer_listener()));
  Status status =
      segmenter_->Initialize(*streams()[0]->info(), encryption_key_source(),
                             max_sd_pixels(), max_hd_pixels(),
                             max_uhd1_pixels(), clear_lead_in_seconds());
  FireOnMediaStartEvent();
  return status;
}

Status TsMuxer::Finalize() {
  FireOnMediaEndEvent();
  return segmenter_->Finalize();
}

Status TsMuxer::DoAddSample(const MediaStream* stream,
                            scoped_refptr<MediaSample> sample) {
  return segmenter_->AddSample(sample);
}

void TsMuxer::FireOnMediaStartEvent() {
  if (!muxer_listener())
    return;
  muxer_listener()->OnMediaStart(options(), *streams().front()->info(),
                                 kTsTimescale, MuxerListener::kContainerWebM);
}

void TsMuxer::FireOnMediaEndEvent() {
  if (!muxer_listener())
    return;

  // For now, there is no single file TS segmenter. So all the values passed
  // here are false and 0. Called just to notify the MuxerListener.
  const bool kHasInitRange = true;
  const bool kHasIndexRange = true;
  muxer_listener()->OnMediaEnd(!kHasInitRange, 0, 0, !kHasIndexRange, 0, 0, 0,
                               0);
}

}  // namespace mp2t
}  // namespace media
}  // namespace shaka
