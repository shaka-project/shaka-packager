// Copyright 2014 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/media/formats/mp4/mp4_init_muxer.h>

#include <packager/macros/logging.h>
#include <packager/macros/status.h>

namespace shaka {
namespace media {
namespace mp4 {

MP4InitMuxer::MP4InitMuxer(const MuxerOptions& options) : MP4Muxer(options) {}

MP4InitMuxer::~MP4InitMuxer() {}

Status MP4InitMuxer::Finalize() {
  LOG(INFO) << "Packaging init segment '" << options().output_file_name;
  RETURN_IF_ERROR(DelayInitializeMuxer());
  return Status::OK;
}

}  // namespace mp4
}  // namespace media
}  // namespace shaka
