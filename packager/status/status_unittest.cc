// Copyright 2014 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/status.h>

#include <absl/strings/str_format.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace shaka {

static void CheckStatus(const Status& s,
                        error::Code code,
                        const std::string& message) {
  EXPECT_EQ(code, s.error_code());
  EXPECT_EQ(message, s.error_message());

  if (code == error::OK) {
    EXPECT_TRUE(s.ok());
    EXPECT_EQ("OK", s.ToString());
  } else {
    EXPECT_TRUE(!s.ok());
    EXPECT_THAT(s.ToString(), testing::HasSubstr(message));
    EXPECT_THAT(s.ToString(), testing::HasSubstr(absl::StrFormat("%d", code)));
  }
}

TEST(Status, Empty) {
  CheckStatus(Status(), error::OK, "");
}

TEST(Status, OK) {
  CheckStatus(Status::OK, error::OK, "");
}

TEST(Status, ConstructorOK) {
  CheckStatus(Status(error::OK, "msg"), error::OK, "");
}

TEST(Status, Unknown) {
  CheckStatus(Status::UNKNOWN, error::UNKNOWN, "");
}

TEST(Status, Filled) {
  CheckStatus(Status(error::CANCELLED, "message"), error::CANCELLED, "message");
}

TEST(Status, Copy) {
  Status a(error::CANCELLED, "message");
  Status b(a);
  ASSERT_EQ(a, b);
}

TEST(Status, Assign) {
  Status a(error::CANCELLED, "message");
  Status b;
  b = a;
  ASSERT_EQ(a, b);
}

TEST(Status, AssignEmpty) {
  Status a(error::CANCELLED, "message");
  Status b;
  a = b;
  ASSERT_EQ("OK", a.ToString());
  ASSERT_TRUE(b.ok());
  ASSERT_TRUE(a.ok());
}

TEST(Status, Update) {
  Status s;
  s.Update(Status::OK);
  ASSERT_TRUE(s.ok());
  const Status a(error::CANCELLED, "message");
  s.Update(a);
  ASSERT_EQ(s, a);
  Status b(error::UNIMPLEMENTED, "other message");
  s.Update(b);
  ASSERT_EQ(s, a);
  s.Update(Status::OK);
  ASSERT_EQ(s, a);
  ASSERT_FALSE(s.ok());
}

// Will trigger copy ellision.
TEST(Status, Update2) {
  Status s;
  ASSERT_TRUE(s.ok());
  s.Update(Status(error::INVALID_ARGUMENT, "some message"));
  ASSERT_EQ(error::INVALID_ARGUMENT, s.error_code());
}

TEST(Status, EqualsOK) {
  ASSERT_EQ(Status::OK, Status());
}

TEST(Status, EqualsSame) {
  ASSERT_EQ(Status(error::UNKNOWN, "message"),
            Status(error::UNKNOWN, "message"));
}

TEST(Status, EqualsCopy) {
  Status a(error::UNKNOWN, "message");
  Status b = a;
  ASSERT_EQ(a, b);
}

TEST(Status, EqualsDifferentCode) {
  ASSERT_NE(Status(error::UNKNOWN, "message"),
            Status(error::CANCELLED, "message"));
}

TEST(Status, EqualsDifferentMessage) {
  ASSERT_NE(Status(error::UNKNOWN, "message"),
            Status(error::UNKNOWN, "another"));
}

}  // namespace shaka
