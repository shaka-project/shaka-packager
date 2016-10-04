// Copyright 2016 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/file/file_util.h"

#include <gtest/gtest.h>

namespace shaka {

TEST(FileUtilTest, Basic) {
  std::string temp_file_path;
  EXPECT_TRUE(TempFilePath("test", &temp_file_path));
  EXPECT_EQ(temp_file_path.find("test"), 0u);

  EXPECT_TRUE(TempFilePath("", &temp_file_path));
  // temp_file_path should be created in a system specific temp directory.
}

}  // namespace shaka
