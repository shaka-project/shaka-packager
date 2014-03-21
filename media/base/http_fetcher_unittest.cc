// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "media/base/http_fetcher.h"

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "media/base/status_test_util.h"

namespace {
const int kHttpNotFound = 404;

const char kTestUrl[] = "http://packager-test.appspot.com/http_test";
const char kTestUrlWithPort[] = "http://packager-test.appspot.com:80/http_test";
const char kExpectedGetResponse[] =
    "<html><head><title>http_test</title></head><body><pre>"
    "Arguments()</pre></body></html>";
const char kPostData[] = "foo=62&type=mp4";
const char kExpectedPostResponse[] =
    "<html><head><title>http_test</title></head><body><pre>"
    "Arguments([foo]=>62[type]=>mp4)</pre></body></html>";
}  // namespace

namespace media {

static void CheckHttpGet(const std::string& url,
                         const std::string& expected_response) {
  SimpleHttpFetcher fetcher;
  std::string response;
  ASSERT_OK(fetcher.Get(url, &response));
  base::RemoveChars(response, "\r\n\t ", &response);
  EXPECT_EQ(expected_response, response);
}

static void CheckHttpPost(const std::string& url, const std::string& data,
                          const std::string& expected_response) {
  SimpleHttpFetcher fetcher;
  std::string response;
  ASSERT_OK(fetcher.Post(url, data, &response));
  base::RemoveChars(response, "\r\n\t ", &response);
  EXPECT_EQ(expected_response, response);
}


TEST(HttpFetcherTest, HttpGet) {
  CheckHttpGet(kTestUrl, kExpectedGetResponse);
}

TEST(HttpFetcherTest, HttpPost) {
  CheckHttpPost(kTestUrl, kPostData, kExpectedPostResponse);
}

TEST(HttpFetcherTest, InvalidUrl) {
  SimpleHttpFetcher fetcher;
  std::string response;
  const std::string invalid_url(kTestUrl, sizeof(kTestUrl) - 2);
  Status status = fetcher.Get(invalid_url, &response);
  EXPECT_EQ(error::HTTP_FAILURE, status.error_code());
  EXPECT_TRUE(
      EndsWith(status.error_message(), base::IntToString(kHttpNotFound), true));
}

TEST(HttpFetcherTest, UrlWithPort) {
  CheckHttpGet(kTestUrlWithPort, kExpectedGetResponse);
}

}  // namespace media

