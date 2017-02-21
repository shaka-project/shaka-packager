// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/base/muxer.h"

#include <algorithm>

#include "packager/media/base/fourccs.h"
#include "packager/media/base/media_sample.h"

namespace shaka {
namespace media {

Muxer::Muxer(const MuxerOptions& options)
    : options_(options),
      encryption_key_source_(NULL),
      max_sd_pixels_(0),
      max_hd_pixels_(0),
      max_uhd1_pixels_(0),
      clear_lead_in_seconds_(0),
      crypto_period_duration_in_seconds_(0),
      protection_scheme_(FOURCC_NULL),
      cancelled_(false),
      clock_(NULL) {}

Muxer::~Muxer() {}

void Muxer::SetKeySource(KeySource* encryption_key_source,
                         uint32_t max_sd_pixels,
                         uint32_t max_hd_pixels,
                         uint32_t max_uhd1_pixels,
                         double clear_lead_in_seconds,
                         double crypto_period_duration_in_seconds,
                         FourCC protection_scheme) {
  DCHECK(encryption_key_source);
  encryption_key_source_ = encryption_key_source;
  max_sd_pixels_ = max_sd_pixels;
  max_hd_pixels_ = max_hd_pixels;
  max_uhd1_pixels_ = max_uhd1_pixels;
  clear_lead_in_seconds_ = clear_lead_in_seconds;
  crypto_period_duration_in_seconds_ = crypto_period_duration_in_seconds;
  protection_scheme_ = protection_scheme;
}

void Muxer::Cancel() {
  cancelled_ = true;
}

void Muxer::SetMuxerListener(std::unique_ptr<MuxerListener> muxer_listener) {
  muxer_listener_ = std::move(muxer_listener);
}

void Muxer::SetProgressListener(
    std::unique_ptr<ProgressListener> progress_listener) {
  progress_listener_ = std::move(progress_listener);
}

Status Muxer::Process(std::unique_ptr<StreamData> stream_data) {
  Status status;
  switch (stream_data->stream_data_type) {
    case StreamDataType::kStreamInfo:
      streams_.push_back(std::move(stream_data->stream_info));
      return InitializeMuxer();
    case StreamDataType::kMediaSample:
      return DoAddSample(stream_data->media_sample);
    default:
      VLOG(3) << "Stream data type "
              << static_cast<int>(stream_data->stream_data_type) << " ignored.";
      break;
  }
  // No dispatch for muxer.
  return Status::OK;
}

}  // namespace media
}  // namespace shaka
