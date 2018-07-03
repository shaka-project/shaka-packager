// Copyright 2017 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/app/muxer_factory.h"

#include "packager/base/time/clock.h"
#include "packager/media/base/muxer.h"
#include "packager/media/base/muxer_options.h"
#include "packager/media/formats/mp2t/ts_muxer.h"
#include "packager/media/formats/mp4/mp4_muxer.h"
#include "packager/media/formats/packed_audio/packed_audio_writer.h"
#include "packager/media/formats/webm/webm_muxer.h"
#include "packager/packager.h"

namespace shaka {
namespace media {

MuxerFactory::MuxerFactory(const PackagingParams& packaging_params)
    : mp4_params_(packaging_params.mp4_output_params),
      transport_stream_timestamp_offset_ms_(
          packaging_params.transport_stream_timestamp_offset_ms),
      temp_dir_(packaging_params.temp_dir) {}

std::shared_ptr<Muxer> MuxerFactory::CreateMuxer(
    MediaContainerName output_format,
    const StreamDescriptor& stream) {
  MuxerOptions options;
  options.mp4_params = mp4_params_;
  options.transport_stream_timestamp_offset_ms =
      transport_stream_timestamp_offset_ms_;
  options.temp_dir = temp_dir_;
  options.output_file_name = stream.output;
  options.segment_template = stream.segment_template;
  options.bandwidth = stream.bandwidth;

  std::shared_ptr<Muxer> muxer;

  switch (output_format) {
    case CONTAINER_AAC:
    case CONTAINER_AC3:
    case CONTAINER_EAC3:
      muxer = std::make_shared<PackedAudioWriter>(options);
      break;
    case CONTAINER_WEBM:
      muxer = std::make_shared<webm::WebMMuxer>(options);
      break;
    case CONTAINER_MPEG2TS:
      muxer = std::make_shared<mp2t::TsMuxer>(options);
      break;
    case CONTAINER_MOV:
      muxer = std::make_shared<mp4::MP4Muxer>(options);
      break;
    default:
      LOG(ERROR) << "Cannot support muxing to " << output_format;
      break;
  }

  if (!muxer) {
    return nullptr;
  }

  // We successfully created a muxer, then there is a couple settings
  // we should set before returning it.
  if (clock_) {
    muxer->set_clock(clock_);
  }

  return muxer;
}

void MuxerFactory::OverrideClock(base::Clock* clock) {
  clock_ = clock;
}
}  // namespace media
}  // namespace shaka
