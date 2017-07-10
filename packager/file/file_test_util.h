// Copyright 2015 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef MEDIA_FILE_FILE_TEST_UTIL_H_
#define MEDIA_FILE_FILE_TEST_UTIL_H_

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <string>

#include "packager/file/file.h"

namespace shaka {

#define ASSERT_FILE_EQ(file_name, array)                            \
  do {                                                              \
    std::string temp_data;                                          \
    ASSERT_TRUE(File::ReadFileToString((file_name), &temp_data));   \
    const char* array_ptr = reinterpret_cast<const char*>(array);   \
    ASSERT_EQ(std::string(array_ptr, arraysize(array)), temp_data); \
  } while (false)

#define ASSERT_FILE_STREQ(file_name, str)                         \
  do {                                                            \
    std::string temp_data;                                        \
    ASSERT_TRUE(File::ReadFileToString((file_name), &temp_data)); \
    ASSERT_EQ(str, temp_data);                                    \
  } while (false)

#define ASSERT_FILE_ENDS_WITH(file_name, array)                             \
  do {                                                                      \
    std::string temp_data;                                                  \
    ASSERT_TRUE(File::ReadFileToString((file_name), &temp_data));           \
    EXPECT_THAT(temp_data,                                                  \
                ::testing::EndsWith(std::string(                            \
                    reinterpret_cast<const char*>(array), sizeof(array)))); \
  } while (false)

}  // namespace shaka

#endif  // MEDIA_FILE_FILE_TEST_UTIL_H_
