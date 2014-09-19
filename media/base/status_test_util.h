// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef MEDIA_BASE_STATUS_TEST_UTIL_H_
#define MEDIA_BASE_STATUS_TEST_UTIL_H_

#include <gtest/gtest.h>

#include "media/base/status.h"

namespace edash_packager {
namespace media {

#define EXPECT_OK(val) EXPECT_EQ(Status::OK, (val))
#define ASSERT_OK(val) ASSERT_EQ(Status::OK, (val))

}  // namespace media
}  // namespace edash_packager

#endif  // MEDIA_BASE_STATUS_TEST_UTIL_H_
