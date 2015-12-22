// Copyright 2015 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef MEDIA_FILE_FILE_TEST_UTIL_H_
#define MEDIA_FILE_FILE_TEST_UTIL_H_

#include <string>

#include "packager/media/file/file.h"

namespace edash_packager {
namespace media {

#define ASSERT_FILE_EQ(file_name, array)                                \
  do {                                                                  \
    std::string temp_data;                                              \
    ASSERT_TRUE(File::ReadFileToString((file_name), &temp_data));       \
    const char* array_ptr = reinterpret_cast<const char*>(array);       \
    ASSERT_EQ(std::string(array_ptr, arraysize(array)), temp_data);     \
  } while (false)

}  // namespace media
}  // namespace edash_packager

#endif  // MEDIA_FILE_FILE_TEST_UTIL_H_

