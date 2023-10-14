// Copyright 2014 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_STATUS_TEST_UTIL_H_
#define PACKAGER_STATUS_TEST_UTIL_H_

#include <gtest/gtest.h>

#include <packager/status.h>

#define EXPECT_OK(val) EXPECT_EQ(shaka::Status::OK, (val))
#define ASSERT_OK(val) ASSERT_EQ(shaka::Status::OK, (val))
#define EXPECT_NOT_OK(val) EXPECT_NE(shaka::Status::OK, (val))
#define ASSERT_NOT_OK(val) ASSERT_NE(shaka::Status::OK, (val))

#endif  // PACKAGER_STATUS_TEST_UTIL_H_
