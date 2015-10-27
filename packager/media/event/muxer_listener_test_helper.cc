// Copyright 2015 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/event/muxer_listener_test_helper.h"

#include <gtest/gtest.h>

#include "packager/base/stl_util.h"

namespace edash_packager {
namespace media {

VideoStreamInfoParameters::VideoStreamInfoParameters() {}
VideoStreamInfoParameters::~VideoStreamInfoParameters() {}

scoped_refptr<StreamInfo> CreateVideoStreamInfo(
    const VideoStreamInfoParameters& param) {
  return scoped_refptr<StreamInfo>(
      new VideoStreamInfo(param.track_id,
                          param.time_scale,
                          param.duration,
                          param.codec,
                          param.codec_string,
                          param.language,
                          param.width,
                          param.height,
                          param.pixel_width,
                          param.pixel_height,
                          0,  // trick_play_rate
                          param.nalu_length_size,
                          vector_as_array(&param.extra_data),
                          param.extra_data.size(),
                          param.is_encrypted));
}

VideoStreamInfoParameters GetDefaultVideoStreamInfoParams() {
  const int kTrackId = 0;
  const uint32_t kTimeScale = 10;
  const uint64_t kVideoStreamDuration = 200;
  const VideoCodec kH264Codec = kCodecH264;
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
  params.extra_data = kExtraData;
  params.is_encrypted = kEncryptedFlag;
  return params;
}

OnMediaEndParameters GetDefaultOnMediaEndParams() {
  // Values for {init, index} range {start, end} are arbitrary, but makes sure
  // that it is monotonically increasing and contiguous.
  const bool kHasInitRange = true;
  const uint64_t kInitRangeStart = 0;
  const uint64_t kInitRangeEnd = kInitRangeStart + 120;
  const uint64_t kHasIndexRange = true;
  const uint64_t kIndexRangeStart = kInitRangeEnd + 1;
  const uint64_t kIndexRangeEnd = kIndexRangeStart + 100;
  const float kMediaDuration = 10.5f;
  const uint64_t kFileSize = 10000;
  OnMediaEndParameters param = {
      kHasInitRange,    kInitRangeStart, kInitRangeEnd,  kHasIndexRange,
      kIndexRangeStart, kIndexRangeEnd,  kMediaDuration, kFileSize};
  return param;
}

void SetDefaultMuxerOptionsValues(MuxerOptions* muxer_options) {
  muxer_options->single_segment = true;
  muxer_options->segment_duration = 10.0;
  muxer_options->fragment_duration = 10.0;
  muxer_options->segment_sap_aligned = true;
  muxer_options->fragment_sap_aligned = true;
  muxer_options->num_subsegments_per_sidx = 0;
  muxer_options->output_file_name = "test_output_file_name.mp4";
  muxer_options->segment_template.clear();
  muxer_options->temp_dir.clear();
}

void ExpectMediaInfoEqual(const MediaInfo& expect, const MediaInfo& actual) {
  ASSERT_TRUE(MediaInfoEqual(expect, actual));
}

bool MediaInfoEqual(const MediaInfo& expect, const MediaInfo& actual) {
  // I found out here
  // https://groups.google.com/forum/#!msg/protobuf/5sOExQkB2eQ/ZSBNZI0K54YJ
  // that the best way to check equality is to serialize and check equality.
  std::string expect_serialized;
  std::string actual_serialized;
  EXPECT_TRUE(expect.SerializeToString(&expect_serialized));
  EXPECT_TRUE(actual.SerializeToString(&actual_serialized));
  EXPECT_EQ(expect_serialized, actual_serialized);
  return expect_serialized == actual_serialized;
}

}  // namespace media
}  // namespace edash_packager
