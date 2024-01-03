// Copyright 2016 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/file/file_util.h>

#include <absl/log/log.h>
#include <gtest/gtest.h>

namespace shaka {

TEST(FileUtilTest, TempFilePathInDesignatedDirectory) {
  std::string temp_file_path;
  EXPECT_TRUE(TempFilePath("test", &temp_file_path));
  EXPECT_EQ(temp_file_path.find("test"), 0u);
  LOG(INFO) << "temp file path: " << temp_file_path;
}

TEST(FileUtilTest, TempFilePathInSystemTempDirectory) {
  std::string temp_file_path;
  EXPECT_TRUE(TempFilePath("", &temp_file_path));
  // temp_file_path should be created in a system specific temp directory.
  LOG(INFO) << "temp file path: " << temp_file_path;
}

TEST(FileUtilTest, TempFilePathCalledTwice) {
  const char kTempDir[] = "/test/";
  std::string temp_file_path1;
  std::string temp_file_path2;
  ASSERT_TRUE(TempFilePath(kTempDir, &temp_file_path1));
  ASSERT_TRUE(TempFilePath(kTempDir, &temp_file_path2));
  ASSERT_NE(temp_file_path1, temp_file_path2);
  LOG(INFO) << "temp file path1: " << temp_file_path1;
  LOG(INFO) << "temp file path2: " << temp_file_path2;
}

}  // namespace shaka
