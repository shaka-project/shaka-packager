// Copyright (c) 2013 Google Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_STATUS_TEST_UTIL_H_
#define MEDIA_BASE_STATUS_TEST_UTIL_H_

#include "media/base/status.h"
#include "testing/gtest/include/gtest/gtest.h"

#define EXPECT_OK(val) EXPECT_EQ(media::Status::OK, (val))
#define ASSERT_OK(val) ASSERT_EQ(media::Status::OK, (val))

#endif  // MEDIA_BASE_STATUS_TEST_UTIL_H_
