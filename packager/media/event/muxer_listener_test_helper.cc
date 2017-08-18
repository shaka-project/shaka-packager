// Copyright 2015 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/event/muxer_listener_test_helper.h"
#include "packager/media/event/muxer_listener.h"

#include <gtest/gtest.h>

namespace shaka {
namespace media {

VideoStreamInfoParameters::VideoStreamInfoParameters() {}
VideoStreamInfoParameters::~VideoStreamInfoParameters() {}

std::shared_ptr<StreamInfo> CreateVideoStreamInfo(
    const VideoStreamInfoParameters& param) {
  return std::make_shared<VideoStreamInfo>(
      param.track_id, param.time_scale, param.duration, param.codec,
      H26xStreamFormat::kUnSpecified, param.codec_string,
      param.codec_config.data(), param.codec_config.size(), param.width,
      param.height, param.pixel_width, param.pixel_height,
      0,  // trick_play_factor
      param.nalu_length_size, param.language, param.is_encrypted);
}

VideoStreamInfoParameters GetDefaultVideoStreamInfoParams() {
  const int kTrackId = 0;
  const uint32_t kTimeScale = 10;
  const uint64_t kVideoStreamDuration = 200;
  const Codec kH264Codec = kCodecH264;
  const char* kCodecString = "avc1.010101";
  const char* kLanuageUndefined = "und";
  const uint16_t kWidth = 720;
  const uint16_t kHeight = 480;
  const uint32_t kPixelWidth = 1;
  const uint32_t kPixelHeight = 1;
  const uint8_t kNaluLengthSize = 1;
  const std::vector<uint8_t> kExtraData;
  const bool kEncryptedFlag = false;
  VideoStreamInfoParameters params;
  params.track_id = kTrackId;
  params.time_scale = kTimeScale;
  params.duration = kVideoStreamDuration;
  params.codec = kH264Codec;
  params.codec_string = kCodecString;
  params.language = kLanuageUndefined;
  params.width = kWidth;
  params.height = kHeight;
  params.pixel_width = kPixelWidth;
  params.pixel_height = kPixelHeight;
  params.nalu_length_size = kNaluLengthSize;
  params.codec_config = kExtraData;
  params.is_encrypted = kEncryptedFlag;
  return params;
}

OnMediaEndParameters GetDefaultOnMediaEndParams() {
  // Values for {init, index} range {start, end} are arbitrary, but makes sure
  // that it is monotonically increasing and contiguous.
  const uint64_t kInitRangeStart = 0;
  const uint64_t kInitRangeEnd = kInitRangeStart + 120;
  const uint64_t kIndexRangeStart = kInitRangeEnd + 1;
  const uint64_t kIndexRangeEnd = kIndexRangeStart + 100;
  const uint64_t kMediaSegmentRangeStart = kIndexRangeEnd + 1;
  const uint64_t kMediaSegmentRangeEnd = 9999;
  const float kMediaDuration = 10.5f;
  MuxerListener::MediaRanges media_ranges;
  Range init_range;
  init_range.start = kInitRangeStart;
  init_range.end = kInitRangeEnd;
  media_ranges.init_range = init_range;
  Range index_range;
  index_range.start = kIndexRangeStart;
  index_range.end = kIndexRangeEnd;
  media_ranges.index_range =index_range;

  Range media_segment_range;
  media_segment_range.start = kMediaSegmentRangeStart;
  media_segment_range.end = kMediaSegmentRangeEnd;
  media_ranges.subsegment_ranges.push_back(media_segment_range);

  OnMediaEndParameters param = {media_ranges, kMediaDuration};
  return param;
}

void SetDefaultMuxerOptions(MuxerOptions* muxer_options) {
  muxer_options->mp4_params.num_subsegments_per_sidx = 0;
  muxer_options->output_file_name = "test_output_file_name.mp4";
  muxer_options->segment_template.clear();
  muxer_options->temp_dir.clear();
}

std::vector<ProtectionSystemSpecificInfo> GetDefaultKeySystemInfo() {
  const uint8_t kPsshData[] = {'p', 's', 's', 'h'};
  const uint8_t kTestSystemId[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05,
                                   0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b,
                                   0x0c, 0x0d, 0x0e, 0x0f};

  ProtectionSystemSpecificInfo info;
  info.set_system_id(kTestSystemId, arraysize(kTestSystemId));
  info.set_pssh_data(
      std::vector<uint8_t>(kPsshData, kPsshData + arraysize(kPsshData)));
  info.set_pssh_box_version(0);

  std::vector<ProtectionSystemSpecificInfo> key_system_info;
  key_system_info.push_back(info);
  return key_system_info;
}

void ExpectMediaInfoEqual(const MediaInfo& expect, const MediaInfo& actual) {
  ASSERT_TRUE(MediaInfoEqual(expect, actual));
}

bool MediaInfoEqual(const MediaInfo& expect, const MediaInfo& actual) {
  // I found out here
  // https://groups.google.com/forum/#!msg/protobuf/5sOExQkB2eQ/ZSBNZI0K54YJ
  // that the best way to check equality is to serialize and check equality.
  std::string expect_serialized = expect.SerializeAsString();
  std::string actual_serialized = actual.SerializeAsString();
  EXPECT_EQ(expect_serialized, actual_serialized);
  return expect_serialized == actual_serialized;
}

}  // namespace media
}  // namespace shaka
