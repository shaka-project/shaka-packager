// Copyright 2017 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "packager/packager.h"

using testing::_;
using testing::HasSubstr;
using testing::Invoke;
using testing::MockFunction;
using testing::Return;
using testing::ReturnArg;
using testing::StrEq;
using testing::UnitTest;
using testing::WithArgs;

namespace shaka {
namespace {

const char kTestFile[] = "packager/media/test/data/bear-640x360.mp4";
const char kOutputVideo[] = "output_video.mp4";
const char kOutputVideoTemplate[] = "output_video_$Number$.m4s";
const char kOutputAudio[] = "output_audio.mp4";
const char kOutputAudioTemplate[] = "output_audio_$Number$.m4s";
const char kOutputMpd[] = "output.mpd";

const double kSegmentDurationInSeconds = 1.0;
const uint8_t kKeyId[] = {
    0xe5, 0x00, 0x7e, 0x6e, 0x9d, 0xcd, 0x5a, 0xc0,
    0x95, 0x20, 0x2e, 0xd3, 0x75, 0x83, 0x82, 0xcd,
};
const uint8_t kKey[]{
    0x6f, 0xc9, 0x6f, 0xe6, 0x28, 0xa2, 0x65, 0xb1,
    0x3a, 0xed, 0xde, 0xc0, 0xbc, 0x42, 0x1f, 0x4d,
};
const double kClearLeadInSeconds = 1.0;

}  // namespace

class PackagerTest : public ::testing::Test {
 public:
  void SetUp() override {
    FILE* f = fopen(kTestFile, "rb");
    if (!f) {
      FAIL() << "The test is expected to run from packager repository root.";
      return;
    }
    fclose(f);

    // Use memory file for testing and generate different test directories for
    // different tests.
    test_directory_ = std::string("memory://test/") +
                      UnitTest::GetInstance()->current_test_info()->name() +
                      "/";
  }

  std::string GetFullPath(const std::string& file_name) {
    return test_directory_ + file_name;
  }

  PackagingParams SetupPackagingParams() {
    PackagingParams packaging_params;
    packaging_params.temp_dir = test_directory_;
    packaging_params.chunking_params.segment_duration_in_seconds =
        kSegmentDurationInSeconds;
    packaging_params.mpd_params.mpd_output = GetFullPath(kOutputMpd);

    packaging_params.encryption_params.clear_lead_in_seconds =
        kClearLeadInSeconds;
    packaging_params.encryption_params.key_provider = KeyProvider::kRawKey;
    packaging_params.encryption_params.raw_key.key_map[""].key_id.assign(
        std::begin(kKeyId), std::end(kKeyId));
    packaging_params.encryption_params.raw_key.key_map[""].key.assign(
        std::begin(kKey), std::end(kKey));
    return packaging_params;
  }

  std::vector<StreamDescriptor> SetupStreamDescriptors() {
    std::vector<StreamDescriptor> stream_descriptors;
    StreamDescriptor stream_descriptor;

    stream_descriptor.input = kTestFile;
    stream_descriptor.stream_selector = "video";
    stream_descriptor.output = GetFullPath(kOutputVideo);
    stream_descriptors.push_back(stream_descriptor);

    stream_descriptor.input = kTestFile;
    stream_descriptor.stream_selector = "audio";
    stream_descriptor.output = GetFullPath(kOutputAudio);
    stream_descriptors.push_back(stream_descriptor);

    return stream_descriptors;
  }

 protected:
  std::string test_directory_;
};

TEST_F(PackagerTest, Version) {
  EXPECT_FALSE(Packager::GetLibraryVersion().empty());
}

TEST_F(PackagerTest, Success) {
  Packager packager;
  ASSERT_EQ(Status::OK, packager.Initialize(SetupPackagingParams(),
                                            SetupStreamDescriptors()));
  ASSERT_EQ(Status::OK, packager.Run());
}

TEST_F(PackagerTest, MissingStreamDescriptors) {
  std::vector<StreamDescriptor> stream_descriptors;
  Packager packager;
  auto status = packager.Initialize(SetupPackagingParams(), stream_descriptors);
  ASSERT_EQ(error::INVALID_ARGUMENT, status.error_code());
}

TEST_F(PackagerTest, MixingSegmentTemplateAndSingleSegment) {
  std::vector<StreamDescriptor> stream_descriptors;
  StreamDescriptor stream_descriptor;

  stream_descriptor.input = kTestFile;
  stream_descriptor.stream_selector = "video";
  stream_descriptor.output = GetFullPath(kOutputVideo);
  stream_descriptor.segment_template = GetFullPath(kOutputVideoTemplate);
  stream_descriptors.push_back(stream_descriptor);

  stream_descriptor.input = kTestFile;
  stream_descriptor.stream_selector = "audio";
  stream_descriptor.output = GetFullPath(kOutputAudio);
  stream_descriptor.segment_template.clear();
  stream_descriptors.push_back(stream_descriptor);

  Packager packager;
  auto status = packager.Initialize(SetupPackagingParams(), stream_descriptors);
  ASSERT_EQ(error::INVALID_ARGUMENT, status.error_code());
}

TEST_F(PackagerTest, DuplicatedOutputs) {
  std::vector<StreamDescriptor> stream_descriptors;
  StreamDescriptor stream_descriptor;

  stream_descriptor.input = kTestFile;
  stream_descriptor.stream_selector = "video";
  stream_descriptor.output = GetFullPath(kOutputVideo);
  stream_descriptor.segment_template = GetFullPath(kOutputVideoTemplate);
  stream_descriptors.push_back(stream_descriptor);

  stream_descriptor.input = kTestFile;
  stream_descriptor.stream_selector = "audio";
  stream_descriptor.output = GetFullPath(kOutputVideo);
  stream_descriptor.segment_template = GetFullPath(kOutputAudioTemplate);
  stream_descriptors.push_back(stream_descriptor);

  Packager packager;
  auto status = packager.Initialize(SetupPackagingParams(), stream_descriptors);
  ASSERT_EQ(error::INVALID_ARGUMENT, status.error_code());
  EXPECT_THAT(status.error_message(), HasSubstr("duplicated outputs"));
}

TEST_F(PackagerTest, DuplicatedSegmentTemplates) {
  std::vector<StreamDescriptor> stream_descriptors;
  StreamDescriptor stream_descriptor;

  stream_descriptor.input = kTestFile;
  stream_descriptor.stream_selector = "video";
  stream_descriptor.output = GetFullPath(kOutputVideo);
  stream_descriptor.segment_template = GetFullPath(kOutputVideoTemplate);
  stream_descriptors.push_back(stream_descriptor);

  stream_descriptor.input = kTestFile;
  stream_descriptor.stream_selector = "audio";
  stream_descriptor.output = GetFullPath(kOutputAudio);
  stream_descriptor.segment_template = GetFullPath(kOutputVideoTemplate);
  stream_descriptors.push_back(stream_descriptor);

  Packager packager;
  auto status = packager.Initialize(SetupPackagingParams(), stream_descriptors);
  ASSERT_EQ(error::INVALID_ARGUMENT, status.error_code());
  EXPECT_THAT(status.error_message(),
              HasSubstr("duplicated segment templates"));
}

TEST_F(PackagerTest, SegmentAlignedAndSubsegmentNotAligned) {
  auto packaging_params = SetupPackagingParams();
  packaging_params.chunking_params.segment_sap_aligned = true;
  packaging_params.chunking_params.subsegment_sap_aligned = false;
  Packager packager;
  ASSERT_EQ(Status::OK,
            packager.Initialize(packaging_params, SetupStreamDescriptors()));
  ASSERT_EQ(Status::OK, packager.Run());
}

TEST_F(PackagerTest, SegmentNotAlignedButSubsegmentAligned) {
  auto packaging_params = SetupPackagingParams();
  packaging_params.chunking_params.segment_sap_aligned = false;
  packaging_params.chunking_params.subsegment_sap_aligned = true;
  Packager packager;
  auto status = packager.Initialize(packaging_params, SetupStreamDescriptors());
  ASSERT_EQ(error::INVALID_ARGUMENT, status.error_code());
}

TEST_F(PackagerTest, WriteOutputToBuffer) {
  auto packaging_params = SetupPackagingParams();

  MockFunction<int64_t(const std::string& name, const void* buffer,
                       uint64_t length)>
      mock_write_func;
  packaging_params.buffer_callback_params.write_func =
      mock_write_func.AsStdFunction();
  EXPECT_CALL(mock_write_func, Call(StrEq(GetFullPath(kOutputVideo)), _, _))
      .WillRepeatedly(ReturnArg<2>());
  EXPECT_CALL(mock_write_func, Call(StrEq(GetFullPath(kOutputAudio)), _, _))
      .WillRepeatedly(ReturnArg<2>());
  EXPECT_CALL(mock_write_func, Call(StrEq(GetFullPath(kOutputMpd)), _, _))
      .WillRepeatedly(ReturnArg<2>());

  Packager packager;
  ASSERT_EQ(Status::OK,
            packager.Initialize(packaging_params, SetupStreamDescriptors()));
  ASSERT_EQ(Status::OK, packager.Run());
}

TEST_F(PackagerTest, ReadFromBuffer) {
  auto packaging_params = SetupPackagingParams();

  MockFunction<int64_t(const std::string& name, void* buffer, uint64_t length)>
      mock_read_func;
  packaging_params.buffer_callback_params.read_func =
      mock_read_func.AsStdFunction();

  const std::string file_name = kTestFile;
  FILE* file_ptr = fopen(file_name.c_str(), "rb");
  ASSERT_TRUE(file_ptr);
  EXPECT_CALL(mock_read_func, Call(StrEq(file_name), _, _))
      .WillRepeatedly(
          WithArgs<1, 2>(Invoke([file_ptr](void* buffer, uint64_t size) {
            return fread(buffer, sizeof(char), size, file_ptr);
          })));

  Packager packager;
  ASSERT_EQ(Status::OK,
            packager.Initialize(packaging_params, SetupStreamDescriptors()));
  ASSERT_EQ(Status::OK, packager.Run());

  fclose(file_ptr);
}

TEST_F(PackagerTest, ReadFromBufferFailed) {
  auto packaging_params = SetupPackagingParams();

  MockFunction<int64_t(const std::string& name, void* buffer, uint64_t length)>
      mock_read_func;
  packaging_params.buffer_callback_params.read_func =
      mock_read_func.AsStdFunction();

  EXPECT_CALL(mock_read_func, Call(_, _, _)).WillOnce(Return(-1));

  Packager packager;
  ASSERT_EQ(Status::OK,
            packager.Initialize(packaging_params, SetupStreamDescriptors()));
  ASSERT_EQ(error::FILE_FAILURE, packager.Run().error_code());
}

// TODO(kqyang): Add more tests.

}  // namespace shaka
