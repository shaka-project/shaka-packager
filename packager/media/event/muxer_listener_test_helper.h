// Copyright 2015 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef MEDIA_EVENT_MUXER_LISTENER_TEST_HELPER_H_
#define MEDIA_EVENT_MUXER_LISTENER_TEST_HELPER_H_

#include <stdint.h>
#include <vector>

#include "packager/base/memory/ref_counted.h"
#include "packager/media/base/key_source.h"
#include "packager/media/base/muxer_options.h"
#include "packager/media/base/stream_info.h"
#include "packager/media/base/video_stream_info.h"
#include "packager/mpd/base/media_info.pb.h"

namespace edash_packager {

namespace media {

// A string containing the escaped PSSH box (for use with a MediaInfo proto).
// This is a full v0 PSSH box with the Widevine system ID and the PSSH data
// 'pssh'
const char kExpectedDefaultPsshBox[] =
    "\\000\\000\\000$pssh\\000\\000\\000\\000\\355\\357\\213\\251y\\326J\\316"
    "\\243\\310\\'\\334\\325\\035!\\355\\000\\000\\000\\4pssh";
const char kExpectedDefaultMediaInfo[] =
    "bandwidth: 7620\n"
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
  VideoCodec codec;
  std::string codec_string;
  std::string language;
  uint16_t width;
  uint16_t height;
  uint32_t pixel_width;
  uint32_t pixel_height;
  uint8_t nalu_length_size;
  std::vector<uint8_t> extra_data;
  bool is_encrypted;
};

// Note that this does not have vector of StreamInfo pointer.
struct OnMediaEndParameters {
  bool has_init_range;
  uint64_t init_range_start;
  uint64_t init_range_end;
  bool has_index_range;
  uint64_t index_range_start;
  uint64_t index_range_end;
  float duration_seconds;
  uint64_t file_size;
};

// Creates StreamInfo instance from VideoStreamInfoParameters.
scoped_refptr<StreamInfo> CreateVideoStreamInfo(
    const VideoStreamInfoParameters& param);

// Returns the "default" VideoStreamInfoParameters for testing.
VideoStreamInfoParameters GetDefaultVideoStreamInfoParams();

// Returns the "default" values for OnMediaEnd().
OnMediaEndParameters GetDefaultOnMediaEndParams();

// Returns the "default" ProtectionSystemSpecificInfo for testing.
std::vector<ProtectionSystemSpecificInfo> GetDefaultKeySystemInfo();

// Sets "default" values for muxer_options for testing.
void SetDefaultMuxerOptionsValues(MuxerOptions* muxer_options);

// Expect that expect and actual are equal.
void ExpectMediaInfoEqual(const MediaInfo& expect, const MediaInfo& actual);

// Returns true if expect and actual are equal.
bool MediaInfoEqual(const MediaInfo& expect, const MediaInfo& actual);

}  // namespace media

}  // namespace edash_packager

#endif  // MEDIA_EVENT_MUXER_LISTENER_TEST_HELPER_H_
