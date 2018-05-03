// Copyright 2015 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_EVENT_MUXER_LISTENER_TEST_HELPER_H_
#define PACKAGER_MEDIA_EVENT_MUXER_LISTENER_TEST_HELPER_H_

#include <stdint.h>
#include <vector>

#include "packager/media/base/key_source.h"
#include "packager/media/base/muxer_options.h"
#include "packager/media/base/stream_info.h"
#include "packager/media/base/video_stream_info.h"
#include "packager/media/event/muxer_listener.h"
#include "packager/mpd/base/media_info.pb.h"

namespace shaka {

namespace media {

const char kExpectedDefaultPsshBox[] = "expected_pssh_box";
const char kExpectedDefaultMediaInfo[] =
    "video_info {\n"
    "  codec: 'avc1.010101'\n"
    "  width: 720\n"
    "  height: 480\n"
    "  time_scale: 10\n"
    "  pixel_width: 1\n"
    "  pixel_height: 1\n"
    "}\n"
    "init_range {\n"
    "  begin: 0\n"
    "  end: 120\n"
    "}\n"
    "index_range {\n"
    "  begin: 121\n"
    "  end: 221\n"
    "}\n"
    "reference_time_scale: 1000\n"
    "container_type: 1\n"
    "media_file_name: 'test_output_file_name.mp4'\n"
    "media_duration_seconds: 10.5\n";
const uint32_t kDefaultReferenceTimeScale = 1000u;

// Struct that gets passed for to CreateVideoStreamInfo() to create a
// StreamInfo instance. Useful for generating multiple VideoStreamInfo with
// slightly different parameters.
struct VideoStreamInfoParameters {
  VideoStreamInfoParameters();
  ~VideoStreamInfoParameters();
  int track_id;
  uint32_t time_scale;
  uint64_t duration;
  Codec codec;
  std::string codec_string;
  std::string language;
  uint16_t width;
  uint16_t height;
  uint32_t pixel_width;
  uint32_t pixel_height;
  uint8_t nalu_length_size;
  std::vector<uint8_t> codec_config;
  bool is_encrypted;
};

struct OnNewSegmentParameters {
  std::string file_name;
  uint64_t start_time;
  uint64_t duration;
  uint64_t segment_file_size;
};

// Note that this does not have vector of StreamInfo pointer.
struct OnMediaEndParameters {
  MuxerListener::MediaRanges media_ranges;
  float duration_seconds;
};

// Creates StreamInfo instance from VideoStreamInfoParameters.
std::shared_ptr<StreamInfo> CreateVideoStreamInfo(
    const VideoStreamInfoParameters& param);

// Returns the "default" VideoStreamInfoParameters for testing.
VideoStreamInfoParameters GetDefaultVideoStreamInfoParams();

// Returns the "default" values for OnMediaEnd().
OnMediaEndParameters GetDefaultOnMediaEndParams();

// Returns the "default" ProtectionSystemSpecificInfo for testing.
std::vector<ProtectionSystemSpecificInfo> GetDefaultKeySystemInfo();

// Sets "default" values for muxer_options for testing.
void SetDefaultMuxerOptions(MuxerOptions* muxer_options);

}  // namespace media

}  // namespace shaka

#endif  // PACKAGER_MEDIA_EVENT_MUXER_LISTENER_TEST_HELPER_H_
