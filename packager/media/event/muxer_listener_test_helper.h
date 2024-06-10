// Copyright 2015 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_EVENT_MUXER_LISTENER_TEST_HELPER_H_
#define PACKAGER_MEDIA_EVENT_MUXER_LISTENER_TEST_HELPER_H_

#include <cstdint>
#include <vector>

#include <packager/media/base/audio_stream_info.h>
#include <packager/media/base/key_source.h>
#include <packager/media/base/muxer_options.h>
#include <packager/media/base/stream_info.h>
#include <packager/media/base/video_stream_info.h>
#include <packager/media/event/muxer_listener.h>
#include <packager/mpd/base/media_info.pb.h>

namespace shaka {

namespace media {

const char kExpectedDefaultPsshBox[] = "expected_pssh_box";
const char kExpectedDefaultMediaInfo[] =
    "video_info {\n"
    "  codec: 'avc1.010101'\n"
    "  supplemental_codec: ''\n"
    "  compatible_brand: 0\n"
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
    "media_duration_seconds: 10.5\n"
    "index: 0\n";

const char kExpectedDefaultMediaInfoSubsegmentRange[] =
    "video_info {\n"
    "  codec: 'avc1.010101'\n"
    "  width: 720\n"
    "  height: 480\n"
    "  time_scale: 10\n"
    "  pixel_width: 1\n"
    "  pixel_height: 1\n"
    "  supplemental_codec: ''\n"
    "  compatible_brand: 0\n"
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
    "media_duration_seconds: 10.5\n"
    "index: 0\n"
    "subsegment_ranges {\n"
    "  begin: 222\n"
    "  end: 9999\n"
    "}\n";

const int32_t kDefaultReferenceTimeScale = 1000;

// Struct that gets passed for to CreateVideoStreamInfo() to create a
// StreamInfo instance. Useful for generating multiple VideoStreamInfo with
// slightly different parameters.
struct VideoStreamInfoParameters {
  VideoStreamInfoParameters();
  ~VideoStreamInfoParameters();
  int track_id;
  int32_t time_scale;
  int64_t duration;
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

// Struct that gets passed for to CreateAudioStreamInfo() to create a
// StreamInfo instance. Useful for generating multiple AudioStreamInfo with
// slightly different parameters.
struct AudioStreamInfoParameters {
  AudioStreamInfoParameters();
  ~AudioStreamInfoParameters();
  int track_id;
  int32_t time_scale;
  int64_t duration;
  Codec codec;
  std::string codec_string;
  std::vector<uint8_t> codec_config;
  uint8_t sample_bits;
  uint8_t num_channels;
  uint32_t sampling_frequency;
  uint64_t seek_preroll_ns;
  uint64_t codec_delay_ns;
  uint32_t max_bitrate;
  uint32_t avg_bitrate;
  std::string language;
  bool is_encrypted;
};

struct OnNewSegmentParameters {
  std::string file_name;
  int64_t start_time;
  int64_t duration;
  uint64_t segment_file_size;
};

// Note that this does not have vector of StreamInfo pointer.
struct OnMediaEndParameters {
  MuxerListener::MediaRanges media_ranges;
  float duration_seconds;
};

// Creates StreamInfo instance from VideoStreamInfoParameters.
std::shared_ptr<VideoStreamInfo> CreateVideoStreamInfo(
    const VideoStreamInfoParameters& param);

// Returns the "default" VideoStreamInfoParameters for testing.
VideoStreamInfoParameters GetDefaultVideoStreamInfoParams();

// Creates StreamInfo instance from AudioStreamInfoParameters.
std::shared_ptr<AudioStreamInfo> CreateAudioStreamInfo(
    const AudioStreamInfoParameters& param);

// Returns the "default" configuration for testing given codec and parameters.
AudioStreamInfoParameters GetAudioStreamInfoParams(
    Codec codec,
    const char* codec_string,
    const std::vector<uint8_t>& codec_config);

// Returns the "default" values for OnMediaEnd().
OnMediaEndParameters GetDefaultOnMediaEndParams();

// Returns the "default" ProtectionSystemSpecificInfo for testing.
std::vector<ProtectionSystemSpecificInfo> GetDefaultKeySystemInfo();

// Sets "default" values for muxer_options for testing.
void SetDefaultMuxerOptions(MuxerOptions* muxer_options);

}  // namespace media

}  // namespace shaka

#endif  // PACKAGER_MEDIA_EVENT_MUXER_LISTENER_TEST_HELPER_H_
