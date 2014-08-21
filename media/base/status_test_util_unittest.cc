// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <gtest/gtest-spi.h>

#include "media/base/status_test_util.h"

TEST(StatusTestUtil, ExpectOkSuccess) {
  EXPECT_OK(media::Status::OK);
}

TEST(StatusTestUtil, AssertOkSuccess) {
  ASSERT_OK(media::Status::OK);
}

TEST(StatusTestUtil, ExpectOkFailure) {
  media::Status status(media::error::UNKNOWN, "Status Unknown");
  EXPECT_NONFATAL_FAILURE(EXPECT_OK(status), "Status Unknown");
}

TEST(StatusTestUtil, AssertOkFailure) {
  EXPECT_FATAL_FAILURE(
      ASSERT_OK(media::Status(media::error::UNKNOWN, "Status Unknown")),
      "Status Unknown");
}
