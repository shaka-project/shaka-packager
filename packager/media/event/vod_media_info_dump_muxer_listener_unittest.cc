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
#include "packager/media/base/muxer_options.h"
#include "packager/media/base/video_stream_info.h"
#include "packager/media/event/muxer_listener_test_helper.h"
#include "packager/media/event/vod_media_info_dump_muxer_listener.h"
#include "packager/media/file/file.h"
#include "packager/mpd/base/media_info.pb.h"

namespace {
const bool kEnableEncryption = true;
// '_default_key_id_' (length 16).
const uint8_t kBogusDefaultKeyId[] = {0x5f, 0x64, 0x65, 0x66, 0x61, 0x75,
                                      0x6c, 0x74, 0x5f, 0x6b, 0x65, 0x79,
                                      0x5f, 0x69, 0x64, 0x5f};
// 'pssh'. Not a valid pssh box.
const uint8_t kInvalidPssh[] = {
  0x70, 0x73, 0x73, 0x68
};

// This should be in the uuid field of the protobuf. This is not a valid UUID
// format but the protobof generation shouldn't care.
const char kTestUUID[] = "myuuid";
const char kTestContentProtectionName[] = "MyContentProtection version 1";
const bool kInitialEncryptionInfo = true;
}  // namespace

namespace edash_packager {
namespace media {

namespace {

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
      const StreamInfo& stream_info,
      bool enable_encryption) {
    MuxerOptions muxer_options;
    SetDefaultMuxerOptionsValues(&muxer_options);
    const uint32_t kReferenceTimeScale = 1000;
    if (enable_encryption) {
      std::vector<uint8_t> bogus_default_key_id(
          kBogusDefaultKeyId,
          kBogusDefaultKeyId + arraysize(kBogusDefaultKeyId));

      // This isn't a valid pssh box but the MediaInfo protobuf creator
      // shouldn't worry about it.
      std::vector<uint8_t> invalid_pssh(kInvalidPssh,
                                        kInvalidPssh + arraysize(kInvalidPssh));

      listener_->OnEncryptionInfoReady(kInitialEncryptionInfo, kTestUUID,
                                       kTestContentProtectionName,
                                       bogus_default_key_id, invalid_pssh);
    }
    listener_->OnMediaStart(muxer_options, stream_info, kReferenceTimeScale,
                            MuxerListener::kContainerMp4);
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

  FireOnMediaStartWithDefaultMuxerOptions(*stream_info, !kEnableEncryption);
  OnMediaEndParameters media_end_param = GetDefaultOnMediaEndParams();
  FireOnMediaEndWithParams(media_end_param);

  const char kExpectedProtobufOutput[] =
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
  ASSERT_NO_FATAL_FAILURE(ExpectTempFileToEqual(kExpectedProtobufOutput));
}

TEST_F(VodMediaInfoDumpMuxerListenerTest, EncryptedStream_Normal) {
  listener_->SetContentProtectionSchemeIdUri("http://foo.com/bar");

  scoped_refptr<StreamInfo> stream_info =
      CreateVideoStreamInfo(GetDefaultVideoStreamInfoParams());
  FireOnMediaStartWithDefaultMuxerOptions(*stream_info, kEnableEncryption);
  OnMediaEndParameters media_end_param = GetDefaultOnMediaEndParams();
  FireOnMediaEndWithParams(media_end_param);

  const char kExpectedProtobufOutput[] =
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
      "media_duration_seconds: 10.5\n"
      "protected_content {\n"
      "  content_protection_entry {\n"
      "    uuid: 'myuuid'\n"
      "    name_version: 'MyContentProtection version 1'\n"
      "    pssh: 'pssh'\n"
      "  }\n"
      "  default_key_id: '_default_key_id_'\n"
      "}\n";

  ASSERT_NO_FATAL_FAILURE(ExpectTempFileToEqual(kExpectedProtobufOutput));
}

// Verify that VideoStreamInfo with non-0 pixel_{width,height} is set in the
// generated MediaInfo.
TEST_F(VodMediaInfoDumpMuxerListenerTest, CheckPixelWidthAndHeightSet) {
  VideoStreamInfoParameters params = GetDefaultVideoStreamInfoParams();
  params.pixel_width = 8;
  params.pixel_height = 9;

  scoped_refptr<StreamInfo> stream_info = CreateVideoStreamInfo(params);
  FireOnMediaStartWithDefaultMuxerOptions(*stream_info, !kEnableEncryption);
  OnMediaEndParameters media_end_param = GetDefaultOnMediaEndParams();
  FireOnMediaEndWithParams(media_end_param);

  const char kExpectedProtobufOutput[] =
      "bandwidth: 7620\n"
      "video_info {\n"
      "  codec: 'avc1.010101'\n"
      "  width: 720\n"
      "  height: 480\n"
      "  time_scale: 10\n"
      "  pixel_width: 8\n"
      "  pixel_height: 9\n"
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
  ASSERT_NO_FATAL_FAILURE(ExpectTempFileToEqual(kExpectedProtobufOutput));
}

}  // namespace media
}  // namespace edash_packager
