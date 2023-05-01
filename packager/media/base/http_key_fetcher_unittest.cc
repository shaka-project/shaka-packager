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
void RetryTest(
    std::function<Status(HttpKeyFetcher&, std::string*)> make_request,
    std::function<void(Status, std::string&)> check_response,
    int32_t timeout_in_seconds = 0) {
  std::string response;
  Status status;

  for (int i = 0; i < 3; ++i) {
    HttpKeyFetcher fetcher(timeout_in_seconds);

    response.clear();
    status = make_request(fetcher, &response);
    if (testing::Test::HasFailure()) return;

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
  check_response(status, response);
}

}  // namespace

TEST(HttpKeyFetcherTest, HttpGet) {
  RetryTest(
      // make_request
      [](HttpKeyFetcher& fetcher, std::string* response) -> Status {
        return fetcher.Get(kTestUrl, response);
      },
      // check_response
      [](Status status, std::string& response) -> void {
        ASSERT_OK(status);
        EXPECT_NE(std::string::npos, response.find("\"method\": \"GET\""));
      });
}

TEST(HttpKeyFetcherTest, HttpPost) {
  RetryTest(
      // make_request
      [](HttpKeyFetcher& fetcher, std::string* response) -> Status {
        return fetcher.Post(kTestUrl, "", response);
      },
      // check_response
      [](Status status, std::string& response) -> void {
        ASSERT_OK(status);
        EXPECT_NE(std::string::npos, response.find("\"method\": \"POST\""));
      });
}

TEST(HttpKeyFetcherTest, HttpFetchKeys) {
  RetryTest(
      // make_request
      [](HttpKeyFetcher& fetcher, std::string* response) -> Status {
        return fetcher.FetchKeys(kTestUrl, "foo=62&type=mp4", response);
      },
      // check_response
      [](Status status, std::string& response) -> void {
        ASSERT_OK(status);
        EXPECT_NE(std::string::npos, response.find("\"foo=62&type=mp4\""));
      });
}

TEST(HttpKeyFetcherTest, InvalidUrl) {
  RetryTest(
      // make_request
      [](HttpKeyFetcher& fetcher, std::string* response) -> Status {
        return fetcher.FetchKeys(kTestUrl404, "", response);
      },
      // check_response
      [](Status status, std::string&) -> void {
        EXPECT_EQ(error::HTTP_FAILURE, status.error_code());
        EXPECT_NE(std::string::npos, status.error_message().find("404"));
      });
}

TEST(HttpKeyFetcherTest, UrlWithPort) {
  RetryTest(
      // make_request
      [](HttpKeyFetcher& fetcher, std::string* response) -> Status {
        return fetcher.FetchKeys(kTestUrlWithPort, "", response);
      },
      // check_response
      [](Status status, std::string&) -> void {
        ASSERT_OK(status);
      });
}

TEST(HttpKeyFetcherTest, SmallTimeout) {
  const int32_t kTimeoutInSeconds = 1;

  RetryTest(
      // make_request
      [](HttpKeyFetcher& fetcher, std::string* response) -> Status {
        return fetcher.FetchKeys(kTestUrlDelayTwoSecs, "", response);
      },
      // check_response
      [](Status status, std::string&) -> void {
        EXPECT_EQ(error::TIME_OUT, status.error_code());
      },
      // timeout_in_seconds
      kTimeoutInSeconds);
}

TEST(HttpKeyFetcherTest, BigTimeout) {
  const int32_t kTimeoutInSeconds = 5;

  RetryTest(
      // make_request
      [](HttpKeyFetcher& fetcher, std::string* response) -> Status {
        return fetcher.FetchKeys(kTestUrlDelayTwoSecs, "", response);
      },
      // check_response
      [](Status status, std::string&) -> void {
        ASSERT_OK(status);
      },
      // timeout_in_seconds
      kTimeoutInSeconds);
}

}  // namespace media
}  // namespace shaka
