// Copyright 2014 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/base/http_key_fetcher.h"

#include <algorithm>

#include "glog/logging.h"
#include "packager/media/test/test_web_server.h"
#include "packager/status/status_test_util.h"

namespace {
// A completely arbitrary port number, unlikely to be in use.
const int kTestServerPort = 58405;

// Reflects back the method, body, and headers of the request as JSON.
const char kTestUrl[] = "http://localhost:58405/reflect";
// Returns the requested HTTP status code.
const char kTestUrl404[] = "http://localhost:58405/status?code=404";
// Returns after the requested delay.
const char kTestUrlDelayTwoSecs[] = "http://localhost:58405/delay?seconds=2";
}  // namespace

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
  void SetUp() override { ASSERT_TRUE(server_.Start(kTestServerPort)); }

 private:
  TestWebServer server_;
};

TEST_F(HttpKeyFetcherTest, HttpGet) {
  HttpKeyFetcher fetcher;
  std::string response;
  ASSERT_OK(fetcher.Get(kTestUrl, &response));
  EXPECT_NE(std::string::npos, response.find("\"method\":\"GET\""));
}

TEST_F(HttpKeyFetcherTest, HttpPost) {
  HttpKeyFetcher fetcher;
  std::string response;
  ASSERT_OK(fetcher.Post(kTestUrl, "", &response));
  EXPECT_NE(std::string::npos, response.find("\"method\":\"POST\""));
}

TEST_F(HttpKeyFetcherTest, HttpFetchKeys) {
  HttpKeyFetcher fetcher;
  std::string response;
  ASSERT_OK(fetcher.FetchKeys(kTestUrl, "foo=62&type=mp4", &response));
  EXPECT_NE(std::string::npos, response.find("\"foo=62&type=mp4\""));
}

TEST_F(HttpKeyFetcherTest, InvalidUrl) {
  HttpKeyFetcher fetcher;
  std::string response;
  Status status = fetcher.FetchKeys(kTestUrl404, "", &response);
  EXPECT_EQ(error::HTTP_FAILURE, status.error_code());
  EXPECT_NE(std::string::npos, status.error_message().find("404"));
}

TEST_F(HttpKeyFetcherTest, SmallTimeout) {
  const int32_t kTimeoutInSeconds = 1;
  HttpKeyFetcher fetcher(kTimeoutInSeconds);
  std::string response;
  Status status = fetcher.FetchKeys(kTestUrlDelayTwoSecs, "", &response);
  EXPECT_EQ(error::TIME_OUT, status.error_code());
}

TEST_F(HttpKeyFetcherTest, BigTimeout) {
  const int32_t kTimeoutInSeconds = 5;
  HttpKeyFetcher fetcher(kTimeoutInSeconds);
  std::string response;
  Status status = fetcher.FetchKeys(kTestUrlDelayTwoSecs, "", &response);
  EXPECT_OK(status);
}

}  // namespace media
}  // namespace shaka
