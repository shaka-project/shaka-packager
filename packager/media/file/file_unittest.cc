// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <gtest/gtest.h>

#include "packager/base/file_util.h"
#include "packager/media/file/file.h"

namespace {
const int kDataSize = 1024;
const char* kTestLocalFileName = "/tmp/local_file_test";
}

namespace edash_packager {
namespace media {

class LocalFileTest : public testing::Test {
 protected:
  virtual void SetUp() {
    data_.resize(kDataSize);
    for (int i = 0; i < kDataSize; ++i)
      data_[i] = i % 256;

    // Test file path for file_util API.
    test_file_path_ = base::FilePath(kTestLocalFileName);

    // Local file name with prefix for File API.
    local_file_name_ = kLocalFilePrefix;
    local_file_name_ += kTestLocalFileName;
  }

  virtual void TearDown() {
    // Remove test file if created.
    base::DeleteFile(base::FilePath(kTestLocalFileName), false);
  }

  std::string data_;
  base::FilePath test_file_path_;
  std::string local_file_name_;
};

TEST_F(LocalFileTest, ReadNotExist) {
  // Remove test file if it exists.
  base::DeleteFile(base::FilePath(kTestLocalFileName), false);
  ASSERT_TRUE(File::Open(local_file_name_.c_str(), "r") == NULL);
}

TEST_F(LocalFileTest, Size) {
  ASSERT_EQ(kDataSize,
            file_util::WriteFile(test_file_path_, data_.data(), kDataSize));
  ASSERT_EQ(kDataSize, File::GetFileSize(local_file_name_.c_str()));
}

TEST_F(LocalFileTest, Write) {
  // Write file using File API.
  File* file = File::Open(local_file_name_.c_str(), "w");
  ASSERT_TRUE(file != NULL);
  EXPECT_EQ(kDataSize, file->Write(&data_[0], kDataSize));
  EXPECT_EQ(kDataSize, file->Size());
  EXPECT_TRUE(file->Close());

  // Read file using file_util API.
  std::string read_data(kDataSize, 0);
  ASSERT_EQ(kDataSize,
            base::ReadFile(test_file_path_, &read_data[0], kDataSize));

  // Compare data written and read.
  EXPECT_EQ(data_, read_data);
}

TEST_F(LocalFileTest, Read_And_Eof) {
  // Write file using file_util API.
  ASSERT_EQ(kDataSize,
            file_util::WriteFile(test_file_path_, data_.data(), kDataSize));

  // Read file using File API.
  File* file = File::Open(local_file_name_.c_str(), "r");
  ASSERT_TRUE(file != NULL);

  // Read half of the file.
  const int kFirstReadBytes = kDataSize / 2;
  std::string read_data(kFirstReadBytes + kDataSize, 0);
  EXPECT_EQ(kFirstReadBytes, file->Read(&read_data[0], kFirstReadBytes));

  // Read the remaining half of the file and verify EOF.
  EXPECT_EQ(kDataSize - kFirstReadBytes,
            file->Read(&read_data[kFirstReadBytes], kDataSize));
  uint8_t single_byte;
  EXPECT_EQ(0, file->Read(&single_byte, sizeof(single_byte)));
  EXPECT_TRUE(file->Close());

  // Compare data written and read.
  read_data.resize(kDataSize);
  EXPECT_EQ(data_, read_data);
}

TEST_F(LocalFileTest, WriteRead) {
  // Write file using File API, using file name directly (without prefix).
  File* file = File::Open(kTestLocalFileName, "w");
  ASSERT_TRUE(file != NULL);
  EXPECT_EQ(kDataSize, file->Write(&data_[0], kDataSize));
  EXPECT_EQ(kDataSize, file->Size());
  EXPECT_TRUE(file->Close());

  // Read file using File API, using local file prefix + file name.
  file = File::Open(local_file_name_.c_str(), "r");
  ASSERT_TRUE(file != NULL);

  // Read half of the file and verify that Eof is not true.
  std::string read_data(kDataSize, 0);
  EXPECT_EQ(kDataSize, file->Read(&read_data[0], kDataSize));
  EXPECT_TRUE(file->Close());

  // Compare data written and read.
  EXPECT_EQ(data_, read_data);
}

}  // namespace media
}  // namespace edash_packager
