// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/base/http_key_fetcher.h"

#include "packager/base/logging.h"
#include "packager/base/strings/string_number_conversions.h"
#include "packager/base/strings/string_util.h"
#include "packager/status_test_util.h"

namespace {
const char kTestUrl[] = "http://packager-test.appspot.com/http_test";
const char kTestUrlWithPort[] = "http://packager-test.appspot.com:80/http_test";
const char kExpectedGetResponse[] =
    "<html><head><title>http_test</title></head><body><pre>"
    "Arguments()</pre></body></html>";
const char kPostData[] = "foo=62&type=mp4";
const char kExpectedPostResponse[] =
    "<html><head><title>http_test</title></head><body><pre>"
    "Arguments([foo]=>62[type]=>mp4)</pre></body></html>";
const char kDelayTwoSecs[] = "delay=2";  // This causes host to delay 2 seconds.
}  // namespace

namespace shaka {
namespace media {

static void CheckHttpGet(const std::string& url,
                         const std::string& expected_response) {
  HttpKeyFetcher fetcher;
  std::string response;
  ASSERT_OK(fetcher.Get(url, &response));
  base::RemoveChars(response, "\r\n\t ", &response);
  EXPECT_EQ(expected_response, response);
}

static void CheckHttpPost(const std::string& url, const std::string& data,
                          const std::string& expected_response) {
  HttpKeyFetcher fetcher;
  std::string response;
  ASSERT_OK(fetcher.Post(url, data, &response));
  base::RemoveChars(response, "\r\n\t ", &response);
  EXPECT_EQ(expected_response, response);
}

TEST(DISABLED_HttpFetcherTest, HttpGet) {
  CheckHttpGet(kTestUrl, kExpectedGetResponse);
}

TEST(DISABLED_HttpFetcherTest, HttpPost) {
  CheckHttpPost(kTestUrl, kPostData, kExpectedPostResponse);
}

TEST(DISABLED_HttpKeyFetcherTest, HttpFetchKeys) {
  HttpKeyFetcher fetcher;
  std::string response;
  ASSERT_OK(fetcher.FetchKeys(kTestUrl, kPostData, &response));
  base::RemoveChars(response, "\r\n\t ", &response);
  EXPECT_EQ(kExpectedPostResponse, response);
}

TEST(DISABLED_HttpKeyFetcherTest, InvalidUrl) {
  const char kHttpNotFound[] = "404";
  HttpKeyFetcher fetcher;
  std::string response;
  const std::string invalid_url(kTestUrl, sizeof(kTestUrl) - 2);
  Status status = fetcher.FetchKeys(invalid_url, kPostData, &response);
  EXPECT_EQ(error::HTTP_FAILURE, status.error_code());
  EXPECT_NE(std::string::npos, status.error_message().find(kHttpNotFound));
}

TEST(DISABLED_HttpKeyFetcherTest, UrlWithPort) {
  HttpKeyFetcher fetcher;
  std::string response;
  ASSERT_OK(fetcher.FetchKeys(kTestUrlWithPort, kPostData, &response));
  base::RemoveChars(response, "\r\n\t ", &response);
  EXPECT_EQ(kExpectedPostResponse, response);
}

TEST(DISABLED_HttpKeyFetcherTest, SmallTimeout) {
  const uint32_t kTimeoutInSeconds = 1;
  HttpKeyFetcher fetcher(kTimeoutInSeconds);
  std::string response;
  Status status = fetcher.FetchKeys(kTestUrl, kDelayTwoSecs, &response);
  EXPECT_EQ(error::TIME_OUT, status.error_code());
}

TEST(DISABLED_HttpKeyFetcherTest, BigTimeout) {
  const uint32_t kTimeoutInSeconds = 5;
  HttpKeyFetcher fetcher(kTimeoutInSeconds);
  std::string response;
  Status status = fetcher.FetchKeys(kTestUrl, kDelayTwoSecs, &response);
  EXPECT_OK(status);
}

}  // namespace media
}  // namespace shaka

