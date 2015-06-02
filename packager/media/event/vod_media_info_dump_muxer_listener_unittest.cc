// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <google/protobuf/text_format.h>
#include <gtest/gtest.h>

#include <vector>

#include "packager/base/file_util.h"
#include "packager/base/files/file_path.h"
#include "packager/base/stl_util.h"
#include "packager/media/event/vod_media_info_dump_muxer_listener.h"
#include "packager/media/file/file.h"
#include "packager/media/base/muxer_options.h"
#include "packager/media/base/video_stream_info.h"
#include "packager/mpd/base/media_info.pb.h"

namespace {
const bool kEnableEncryption = true;
}  // namespace

namespace edash_packager {
namespace media {

namespace {
struct VideoStreamInfoParameters {
  int track_id;
  uint32_t time_scale;
  uint64_t duration;
  VideoCodec codec;
  std::string codec_string;
  std::string language;
  uint16_t width;
  uint16_t height;
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
  const uint8_t kH264Profile = 1;
  const uint8_t kH264CompatibleProfile = 1;
  const uint8_t kH264Level = 1;
  const char* kLanuageUndefined = "und";
  const uint16_t kWidth = 720;
  const uint16_t kHeight = 480;
  const uint8_t kNaluLengthSize = 1;
  const std::vector<uint8_t> kExtraData;
  const bool kEncryptedFlag = false;

  VideoStreamInfoParameters param = {
      kTrackId, kTimeScale, kVideoStreamDuration, kH264Codec,
      VideoStreamInfo::GetCodecString(
          kCodecH264, kH264Profile, kH264CompatibleProfile, kH264Level),
      kLanuageUndefined, kWidth, kHeight, kNaluLengthSize, kExtraData,
      kEncryptedFlag};
  return param;
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
  // I found out here
  // https://groups.google.com/forum/#!msg/protobuf/5sOExQkB2eQ/ZSBNZI0K54YJ
  // that the best way to check equality is to serialize and check equality.
  std::string expect_serialized;
  std::string actual_serialized;
  ASSERT_TRUE(expect.SerializeToString(&expect_serialized));
  ASSERT_TRUE(actual.SerializeToString(&actual_serialized));
  ASSERT_EQ(expect_serialized, actual_serialized);
}

void ExpectTextFormatMediaInfoEqual(const std::string& expect,
                                    const std::string& actual) {
  MediaInfo expect_media_info;
  MediaInfo actual_media_info;
  typedef ::google::protobuf::TextFormat TextFormat;
  ASSERT_TRUE(TextFormat::ParseFromString(expect, &expect_media_info))
      << "Failed to parse " << std::endl << expect;
  ASSERT_TRUE(TextFormat::ParseFromString(actual, &actual_media_info))
      << "Failed to parse " << std::endl <<  actual;
  ASSERT_NO_FATAL_FAILURE(
      ExpectMediaInfoEqual(expect_media_info, actual_media_info))
      << "Expect:" << std::endl << expect << std::endl
      << "Actual:" << std::endl << actual;
}
}  // namespace

class VodMediaInfoDumpMuxerListenerTest : public ::testing::Test {
 public:
  VodMediaInfoDumpMuxerListenerTest() {}
  virtual ~VodMediaInfoDumpMuxerListenerTest() {}

  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(base::CreateTemporaryFile(&temp_file_path_));
    DLOG(INFO) << "Created temp file: " << temp_file_path_.value();

    listener_.reset(new VodMediaInfoDumpMuxerListener(temp_file_path_.value()));
  }

  virtual void TearDown() OVERRIDE {
    base::DeleteFile(temp_file_path_, false);
  }

  void FireOnMediaStartWithDefaultMuxerOptions(
      const std::vector<StreamInfo*> stream_infos,
      bool enable_encryption) {
    MuxerOptions muxer_options;
    SetDefaultMuxerOptionsValues(&muxer_options);
    const uint32_t kReferenceTimeScale = 1000;
    listener_->OnMediaStart(muxer_options,
                            stream_infos,
                            kReferenceTimeScale,
                            MuxerListener::kContainerMp4,
                            enable_encryption);
  }

  void FireOnMediaEndWithParams(const OnMediaEndParameters& params) {
    // On success, this writes the result to |temp_file_path_|.
    listener_->OnMediaEnd(params.has_init_range,
                          params.init_range_start,
                          params.init_range_end,
                          params.has_index_range,
                          params.index_range_start,
                          params.index_range_end,
                          params.duration_seconds,
                          params.file_size);
  }

  void ExpectTempFileToEqual(const std::string& expected_protobuf) {
    std::string temp_file_media_info_str;
    ASSERT_TRUE(File::ReadFileToString(temp_file_path_.value().c_str(),
                                       &temp_file_media_info_str));
    ASSERT_TRUE(!temp_file_media_info_str.empty());

    ASSERT_NO_FATAL_FAILURE((ExpectTextFormatMediaInfoEqual(
        expected_protobuf, temp_file_media_info_str)));
  }

 protected:
  base::FilePath temp_file_path_;
  scoped_ptr<VodMediaInfoDumpMuxerListener> listener_;

 private:
  DISALLOW_COPY_AND_ASSIGN(VodMediaInfoDumpMuxerListenerTest);
};

TEST_F(VodMediaInfoDumpMuxerListenerTest, UnencryptedStream_Normal) {
  scoped_refptr<StreamInfo> stream_info =
      CreateVideoStreamInfo(GetDefaultVideoStreamInfoParams());
  std::vector<StreamInfo*> stream_infos;
  stream_infos.push_back(stream_info.get());

  FireOnMediaStartWithDefaultMuxerOptions(stream_infos, !kEnableEncryption);
  OnMediaEndParameters media_end_param = GetDefaultOnMediaEndParams();
  FireOnMediaEndWithParams(media_end_param);

  const char kExpectedProtobufOutput[] =
      "bandwidth: 7620\n"
      "video_info {\n"
      "  codec: \"avc1.010101\"\n"
      "  width: 720\n"
      "  height: 480\n"
      "  time_scale: 10\n"
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
      "media_file_name: \"test_output_file_name.mp4\"\n"
      "media_duration_seconds: 10.5\n";
  ASSERT_NO_FATAL_FAILURE(ExpectTempFileToEqual(kExpectedProtobufOutput));
}

TEST_F(VodMediaInfoDumpMuxerListenerTest, EncryptedStream_Normal) {
  listener_->SetContentProtectionSchemeIdUri("http://foo.com/bar");

  scoped_refptr<StreamInfo> stream_info =
      CreateVideoStreamInfo(GetDefaultVideoStreamInfoParams());
  std::vector<StreamInfo*> stream_infos;
  stream_infos.push_back(stream_info.get());

  FireOnMediaStartWithDefaultMuxerOptions(stream_infos, kEnableEncryption);

  OnMediaEndParameters media_end_param = GetDefaultOnMediaEndParams();
  FireOnMediaEndWithParams(media_end_param);

  const char kExpectedProtobufOutput[] =
      "bandwidth: 7620\n"
      "video_info {\n"
      "  codec: \"avc1.010101\"\n"
      "  width: 720\n"
      "  height: 480\n"
      "  time_scale: 10\n"
      "}\n"
      "content_protections {\n"
      "  scheme_id_uri: \"urn:mpeg:dash:mp4protection:2011\"\n"
      "  value: \"cenc\"\n"
      "}\n"
      "content_protections {\n"
      "  scheme_id_uri: \"http://foo.com/bar\"\n"
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
      "media_file_name: \"test_output_file_name.mp4\"\n"
      "media_duration_seconds: 10.5\n";
  ASSERT_NO_FATAL_FAILURE(ExpectTempFileToEqual(kExpectedProtobufOutput));
}

}  // namespace media
}  // namespace edash_packager
