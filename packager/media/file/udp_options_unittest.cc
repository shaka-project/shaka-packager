// Copyright 2016 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/file/udp_options.h"

#include <gflags/gflags.h>
#include <gtest/gtest.h>

DECLARE_string(udp_interface_address);

namespace shaka {
namespace media {

class UdpOptionsTest : public testing::Test {
 public:
  void SetUp() override { FLAGS_udp_interface_address = ""; }
};

TEST_F(UdpOptionsTest, AddressAndPort) {
  auto options = UdpOptions::ParseFromString("224.1.2.30:88");
  ASSERT_TRUE(options);
  EXPECT_EQ("224.1.2.30", options->address());
  EXPECT_EQ(88u, options->port());
  // The below fields are not set.
  EXPECT_FALSE(options->reuse());
  EXPECT_EQ("", options->interface_address());
  EXPECT_EQ(0u, options->timeout_us());
}

TEST_F(UdpOptionsTest, MissingPort) {
  ASSERT_FALSE(UdpOptions::ParseFromString("224.1.2.30"));
  ASSERT_FALSE(UdpOptions::ParseFromString("224.1.2.30:"));
}

TEST_F(UdpOptionsTest, InvalidPort) {
  ASSERT_FALSE(UdpOptions::ParseFromString("224.1.2.30:888888"));
  ASSERT_FALSE(UdpOptions::ParseFromString("224.1.2.30:abcd"));
}

TEST_F(UdpOptionsTest, MissingAddress) {
  ASSERT_FALSE(UdpOptions::ParseFromString(":888888"));
  ASSERT_FALSE(UdpOptions::ParseFromString("888888"));
}

TEST_F(UdpOptionsTest, UdpInterfaceAddressFlag) {
  FLAGS_udp_interface_address = "10.11.12.13";

  auto options = UdpOptions::ParseFromString("224.1.2.30:88");
  ASSERT_TRUE(options);
  EXPECT_EQ("224.1.2.30", options->address());
  EXPECT_EQ(88u, options->port());
  // The below fields are not set.
  EXPECT_FALSE(options->reuse());
  EXPECT_EQ("10.11.12.13", options->interface_address());
  EXPECT_EQ(0u, options->timeout_us());
}

TEST_F(UdpOptionsTest, Reuse) {
  auto options = UdpOptions::ParseFromString("224.1.2.30:88?reuse=1");
  EXPECT_EQ("224.1.2.30", options->address());
  EXPECT_EQ(88u, options->port());
  EXPECT_TRUE(options->reuse());
  EXPECT_EQ("", options->interface_address());
  EXPECT_EQ(0u, options->timeout_us());
}

TEST_F(UdpOptionsTest, InvalidReuse) {
  ASSERT_FALSE(UdpOptions::ParseFromString("224.1.2.30:88?reuse=7bd"));
}

TEST_F(UdpOptionsTest, InterfaceAddress) {
  auto options = UdpOptions::ParseFromString(
      "224.1.2.30:88?reuse=0&interface=10.11.12.13");
  EXPECT_EQ("224.1.2.30", options->address());
  EXPECT_EQ(88u, options->port());
  EXPECT_FALSE(options->reuse());
  EXPECT_EQ("10.11.12.13", options->interface_address());
  EXPECT_EQ(0u, options->timeout_us());
}

TEST_F(UdpOptionsTest, Timeout) {
  auto options = UdpOptions::ParseFromString(
      "224.1.2.30:88?source=10.11.12.13&timeout=88888888");
  EXPECT_EQ("224.1.2.30", options->address());
  EXPECT_EQ(88u, options->port());
  EXPECT_FALSE(options->reuse());
  EXPECT_EQ("10.11.12.13", options->interface_address());
  EXPECT_EQ(88888888u, options->timeout_us());
}

TEST_F(UdpOptionsTest, InvalidTimeout) {
  ASSERT_FALSE(UdpOptions::ParseFromString(
      "224.1.2.30:88?source=10.11.12.13&timeout=1a9"));
}

}  // namespace media
}  // namespace shaka
