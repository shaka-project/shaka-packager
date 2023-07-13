// Copyright 2014 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/base/http_key_fetcher.h"

#include <algorithm>

#include "glog/logging.h"
#include "packager/status/status_test_util.h"

namespace {
const char kTestUrl[] = "https://httpbin.org/anything";
const char kTestUrl404[] = "https://httpbin.org/status/404";
const char kTestUrlWithPort[] = "https://httpbin.org:443/anything";
const char kTestUrlDelayTwoSecs[] = "https://httpbin.org/delay/2";
}  // namespace

namespace shaka {
namespace media {

TEST(HttpFetcherTest, HttpGet) {
  HttpKeyFetcher fetcher;
  std::string response;
  ASSERT_OK(fetcher.Get(kTestUrl, &response));
  EXPECT_NE(std::string::npos, response.find("\"method\": \"GET\""));
}

TEST(HttpFetcherTest, HttpPost) {
  HttpKeyFetcher fetcher;
  std::string response;
  ASSERT_OK(fetcher.Post(kTestUrl, "", &response));
  EXPECT_NE(std::string::npos, response.find("\"method\": \"POST\""));
}

TEST(HttpKeyFetcherTest, HttpFetchKeys) {
  HttpKeyFetcher fetcher;
  std::string response;
  ASSERT_OK(fetcher.FetchKeys(kTestUrl, "foo=62&type=mp4", &response));
  EXPECT_NE(std::string::npos, response.find("\"foo=62&type=mp4\""));
}

TEST(HttpKeyFetcherTest, InvalidUrl) {
  HttpKeyFetcher fetcher;
  std::string response;
  Status status = fetcher.FetchKeys(kTestUrl404, "", &response);
  EXPECT_EQ(error::HTTP_FAILURE, status.error_code());
  EXPECT_NE(std::string::npos, status.error_message().find("404"));
}

TEST(HttpKeyFetcherTest, UrlWithPort) {
  HttpKeyFetcher fetcher;
  std::string response;
  ASSERT_OK(fetcher.FetchKeys(kTestUrlWithPort, "", &response));
}

TEST(HttpKeyFetcherTest, SmallTimeout) {
  const int32_t kTimeoutInSeconds = 1;
  HttpKeyFetcher fetcher(kTimeoutInSeconds);
  std::string response;
  Status status = fetcher.FetchKeys(kTestUrlDelayTwoSecs, "", &response);
  EXPECT_EQ(error::TIME_OUT, status.error_code());
}

TEST(HttpKeyFetcherTest, BigTimeout) {
  const int32_t kTimeoutInSeconds = 5;
  HttpKeyFetcher fetcher(kTimeoutInSeconds);
  std::string response;
  Status status = fetcher.FetchKeys(kTestUrlDelayTwoSecs, "", &response);
  EXPECT_OK(status);
}

}  // namespace media
}  // namespace shaka

