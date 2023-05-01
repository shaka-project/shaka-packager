// Copyright 2014 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/base/http_key_fetcher.h"

#include <algorithm>
#include <chrono>
#include <thread>

#include "glog/logging.h"
#include "packager/status/status_test_util.h"

namespace shaka {
namespace media {

namespace {

const char kTestUrl[] = "https://httpbin.org/anything";
const char kTestUrl404[] = "https://httpbin.org/status/404";
const char kTestUrlWithPort[] = "https://httpbin.org:443/anything";
const char kTestUrlDelayTwoSecs[] = "https://httpbin.org/delay/2";

// Tests using httpbin can sometimes be flaky.  We get HTTP 502 errors when it
// is overloaded.  This will retry a test with delays, up to a limit, if the
// HTTP status code is 502.
void RetryTest(std::function<void(HttpKeyFetcher&, std::string*)> make_request,
               std::function<void(std::string&)> check_response,
               int32_t timeout_in_seconds = 0) {
  std::string response;

  for (int i = 0; i < 3; ++i) {
    HttpKeyFetcher fetcher(timeout_in_seconds);

    response.clear();
    make_request(fetcher, &response);

    if (fetcher.http_status_code() != 502) {
      // Not a 502 error, so take this result.
      break;
    }

    // Delay with exponential increase (1s, 2s, 4s), then loop try again.
    int delay = 1 << i;
    LOG(WARNING) << "httpbin failure (" << fetcher.http_status_code() << "): "
                 << "Delaying " << delay << " seconds and retrying.";
    std::this_thread::sleep_for(std::chrono::seconds(delay));
  }

  // Out of retries?  Check what we have.
  check_response(response);
}

}  // namespace

TEST(HttpFetcherTest, HttpGet) {
  RetryTest(
      // make_request
      [](HttpKeyFetcher& fetcher, std::string* response) -> void {
        ASSERT_OK(fetcher.Get(kTestUrl, response));
      },
      // check_response
      [](std::string& response) -> void {
        EXPECT_NE(std::string::npos, response.find("\"method\": \"GET\""));
      });
}

TEST(HttpFetcherTest, HttpPost) {
  RetryTest(
      // make_request
      [](HttpKeyFetcher& fetcher, std::string* response) -> void {
        ASSERT_OK(fetcher.Post(kTestUrl, "", response));
      },
      // check_response
      [](std::string& response) -> void {
        EXPECT_NE(std::string::npos, response.find("\"method\": \"POST\""));
      });
}

TEST(HttpKeyFetcherTest, HttpFetchKeys) {
  RetryTest(
      // make_request
      [](HttpKeyFetcher& fetcher, std::string* response) -> void {
        ASSERT_OK(fetcher.FetchKeys(kTestUrl, "foo=62&type=mp4", response));
      },
      // check_response
      [](std::string& response) -> void {
        EXPECT_NE(std::string::npos, response.find("\"foo=62&type=mp4\""));
      });
}

TEST(HttpKeyFetcherTest, InvalidUrl) {
  RetryTest(
      // make_request
      [](HttpKeyFetcher& fetcher, std::string* response) -> void {
        Status status = fetcher.FetchKeys(kTestUrl404, "", response);
        EXPECT_EQ(error::HTTP_FAILURE, status.error_code());
        EXPECT_NE(std::string::npos, status.error_message().find("404"));
      },
      // check_response
      [](std::string&) -> void {});
}

TEST(HttpKeyFetcherTest, UrlWithPort) {
  RetryTest(
      // make_request
      [](HttpKeyFetcher& fetcher, std::string* response) -> void {
        ASSERT_OK(fetcher.FetchKeys(kTestUrlWithPort, "", response));
      },
      // check_response
      [](std::string&) -> void {});
}

TEST(HttpKeyFetcherTest, SmallTimeout) {
  const int32_t kTimeoutInSeconds = 1;

  RetryTest(
      // make_request
      [](HttpKeyFetcher& fetcher, std::string* response) -> void {
        Status status = fetcher.FetchKeys(kTestUrlDelayTwoSecs, "", response);
        EXPECT_EQ(error::TIME_OUT, status.error_code());
      },
      // check_response
      [](std::string&) -> void {},
      // timeout_in_seconds
      kTimeoutInSeconds);
}

TEST(HttpKeyFetcherTest, BigTimeout) {
  const int32_t kTimeoutInSeconds = 5;

  RetryTest(
      // make_request
      [](HttpKeyFetcher& fetcher, std::string* response) -> void {
        Status status = fetcher.FetchKeys(kTestUrlDelayTwoSecs, "", response);
        EXPECT_OK(status);
      },
      // check_response
      [](std::string&) -> void {},
      // timeout_in_seconds
      kTimeoutInSeconds);
}

}  // namespace media
}  // namespace shaka
