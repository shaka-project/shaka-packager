// Copyright 2014 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/media/base/http_key_fetcher.h>

#include <algorithm>

#include <absl/log/log.h>

#include <packager/media/test/test_web_server.h>
#include <packager/status/status_test_util.h>

namespace shaka {
namespace media {

// Quoting gtest docs:
//   "For each TEST_F, GoogleTest will create a fresh test fixture object,
//   immediately call SetUp(), run the test body, call TearDown(), and then
//   delete the test fixture object."
// So we don't need a TearDown method.  The destructor on TestWebServer is good
// enough.
class HttpKeyFetcherTest : public testing::Test {
 protected:
  void SetUp() override { ASSERT_TRUE(server_.Start()); }

  TestWebServer server_;
};

TEST_F(HttpKeyFetcherTest, HttpGet) {
  HttpKeyFetcher fetcher;
  std::string response;
  ASSERT_OK(fetcher.Get(server_.ReflectUrl(), &response));
  EXPECT_NE(std::string::npos, response.find("\"method\":\"GET\""));
}

TEST_F(HttpKeyFetcherTest, HttpPost) {
  HttpKeyFetcher fetcher;
  std::string response;
  ASSERT_OK(fetcher.Post(server_.ReflectUrl(), "", &response));
  EXPECT_NE(std::string::npos, response.find("\"method\":\"POST\""));
}

TEST_F(HttpKeyFetcherTest, HttpFetchKeys) {
  HttpKeyFetcher fetcher;
  std::string response;
  ASSERT_OK(
      fetcher.FetchKeys(server_.ReflectUrl(), "foo=62&type=mp4", &response));
  EXPECT_NE(std::string::npos, response.find("\"foo=62&type=mp4\""));
}

TEST_F(HttpKeyFetcherTest, InvalidUrl) {
  HttpKeyFetcher fetcher;
  std::string response;
  Status status = fetcher.FetchKeys(server_.StatusCodeUrl(404), "", &response);
  EXPECT_EQ(error::HTTP_FAILURE, status.error_code());
  EXPECT_NE(std::string::npos, status.error_message().find("404"));
}

TEST_F(HttpKeyFetcherTest, SmallTimeout) {
  const int32_t kTimeoutInSeconds = 1;
  HttpKeyFetcher fetcher(kTimeoutInSeconds);
  std::string response;
  Status status = fetcher.FetchKeys(server_.DelayUrl(2), "", &response);
  EXPECT_EQ(error::TIME_OUT, status.error_code());
}

TEST_F(HttpKeyFetcherTest, BigTimeout) {
  const int32_t kTimeoutInSeconds = 5;
  HttpKeyFetcher fetcher(kTimeoutInSeconds);
  std::string response;
  Status status = fetcher.FetchKeys(server_.DelayUrl(2), "", &response);
  EXPECT_OK(status);
}

}  // namespace media
}  // namespace shaka
