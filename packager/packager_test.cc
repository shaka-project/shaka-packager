// Copyright 2017 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "packager/base/files/file_util.h"
#include "packager/base/logging.h"
#include "packager/base/path_service.h"
#include "packager/base/strings/string_number_conversions.h"
#include "packager/packager.h"

using testing::_;
using testing::Invoke;
using testing::MockFunction;
using testing::Return;
using testing::ReturnArg;
using testing::StrEq;
using testing::WithArgs;

namespace shaka {
namespace {

const char kTestFile[] = "bear-640x360.mp4";
const char kOutputVideo[] = "output_video.mp4";
const char kOutputVideoTemplate[] = "output_video_$Number$.m4s";
const char kOutputAudio[] = "output_audio.mp4";
const char kOutputMpd[] = "output.mpd";

const double kSegmentDurationInSeconds = 1.0;
const char kKeyIdHex[] = "e5007e6e9dcd5ac095202ed3758382cd";
const char kKeyHex[] = "6fc96fe628a265b13aeddec0bc421f4d";
const double kClearLeadInSeconds = 1.0;

std::string GetTestDataFilePath(const std::string& name) {
  base::FilePath file_path;
  CHECK(PathService::Get(base::DIR_SOURCE_ROOT, &file_path));

  file_path = file_path.Append(FILE_PATH_LITERAL("packager"))
                  .Append(FILE_PATH_LITERAL("media"))
                  .Append(FILE_PATH_LITERAL("test"))
                  .Append(FILE_PATH_LITERAL("data"))
                  .AppendASCII(name);
  return file_path.AsUTF8Unsafe();
}

}  // namespace

class PackagerTest : public ::testing::Test {
 public:
  PackagerTest() {}

  void SetUp() override {
    // Create a test directory for testing, will be deleted after test.
    ASSERT_TRUE(base::CreateNewTempDirectory(
        base::FilePath::FromUTF8Unsafe("packager_").value(), &test_directory_));
  }

  void TearDown() override { base::DeleteFile(test_directory_, true); }

  std::string GetFullPath(const std::string& file_name) {
    return test_directory_.Append(base::FilePath::FromUTF8Unsafe(file_name))
        .AsUTF8Unsafe();
  }

  PackagingParams SetupPackagingParams() {
    PackagingParams packaging_params;
    packaging_params.temp_dir = test_directory_.AsUTF8Unsafe();
    packaging_params.chunking_params.segment_duration_in_seconds =
        kSegmentDurationInSeconds;
    packaging_params.mpd_params.mpd_output = GetFullPath(kOutputMpd);

    packaging_params.encryption_params.clear_lead_in_seconds =
        kClearLeadInSeconds;
    packaging_params.encryption_params.key_provider = KeyProvider::kRawKey;
    CHECK(base::HexStringToBytes(
        kKeyIdHex,
        &packaging_params.encryption_params.raw_key.key_map[""].key_id));
    CHECK(base::HexStringToBytes(
        kKeyHex, &packaging_params.encryption_params.raw_key.key_map[""].key));
    return packaging_params;
  }

  std::vector<StreamDescriptor> SetupStreamDescriptors() {
    std::vector<StreamDescriptor> stream_descriptors;
    StreamDescriptor stream_descriptor;

    stream_descriptor.input = GetTestDataFilePath(kTestFile);
    stream_descriptor.stream_selector = "video";
    stream_descriptor.output = GetFullPath(kOutputVideo);
    stream_descriptors.push_back(stream_descriptor);

    stream_descriptor.input = GetTestDataFilePath(kTestFile);
    stream_descriptor.stream_selector = "audio";
    stream_descriptor.output = GetFullPath(kOutputAudio);
    stream_descriptors.push_back(stream_descriptor);

    return stream_descriptors;
  }

 protected:
  base::FilePath test_directory_;
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

  stream_descriptor.input = GetTestDataFilePath(kTestFile);
  stream_descriptor.stream_selector = "video";
  stream_descriptor.output = GetFullPath(kOutputVideo);
  stream_descriptor.segment_template = GetFullPath(kOutputVideoTemplate);
  stream_descriptors.push_back(stream_descriptor);

  stream_descriptor.input = GetTestDataFilePath(kTestFile);
  stream_descriptor.stream_selector = "audio";
  stream_descriptor.output = GetFullPath(kOutputAudio);
  stream_descriptor.segment_template.clear();
  stream_descriptors.push_back(stream_descriptor);

  Packager packager;
  auto status = packager.Initialize(SetupPackagingParams(), stream_descriptors);
  ASSERT_EQ(error::INVALID_ARGUMENT, status.error_code());
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

  const std::string file_name = GetTestDataFilePath(kTestFile);
  FILE* file_ptr =
      base::OpenFile(base::FilePath::FromUTF8Unsafe(file_name), "rb");
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

  base::CloseFile(file_ptr);
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
