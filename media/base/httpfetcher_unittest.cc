// Copyright (c) 2013 Google Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/httpfetcher.h"

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "media/base/status_test_util.h"

namespace {
const int kHttpOK = 200;
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
  HTTPFetcher fetcher;
  std::string response;
  ASSERT_OK(fetcher.Get(url, &response));
  RemoveChars(response, "\r\n\t ", &response);
  EXPECT_EQ(expected_response, response);
}

static void CheckHttpPost(const std::string& url, const std::string& data,
                          const std::string& expected_response) {
  HTTPFetcher fetcher;
  std::string response;
  ASSERT_OK(fetcher.Post(url, data, &response));
  RemoveChars(response, "\r\n\t ", &response);
  EXPECT_EQ(expected_response, response);
}


TEST(HTTPFetcherTest, HttpGet) {
  CheckHttpGet(kTestUrl, kExpectedGetResponse);
}

TEST(HTTPFetcherTest, HttpPost) {
  CheckHttpPost(kTestUrl, kPostData, kExpectedPostResponse);
}

TEST(HTTPFetcherTest, InvalidUrl) {
  HTTPFetcher fetcher;
  std::string response;
  const std::string invalid_url(kTestUrl, sizeof(kTestUrl) - 2);
  Status status = fetcher.Get(invalid_url, &response);
  EXPECT_EQ(error::HTTP_FAILURE, status.error_code());
  EXPECT_TRUE(
      EndsWith(status.error_message(), base::IntToString(kHttpNotFound), true));
}

TEST(HTTPFetcherTest, UrlWithPort) {
  CheckHttpGet(kTestUrlWithPort, kExpectedGetResponse);
}

}  // namespace media

