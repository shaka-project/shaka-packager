// Copyright 2020 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/file/http_file.h"

#include <gtest/gtest.h>

#include <chrono>
#include <memory>
#include <vector>
#include <thread>

#include "absl/strings/str_split.h"
#include "nlohmann/json.hpp"
#include "packager/file/file.h"
#include "packager/file/file_closer.h"

#define ASSERT_JSON_STRING(json, key, value) \
  ASSERT_EQ(GetJsonString((json), (key)), (value)) << "JSON is " << (json)

namespace shaka {

namespace {

using FilePtr = std::unique_ptr<HttpFile, FileCloser>;

// Handles keys with dots, indicating a nested field.
std::string GetJsonString(const nlohmann::json& json,
                          const std::string& combined_key) {
  std::vector<std::string> keys = absl::StrSplit(combined_key, '.');
  nlohmann::json current = json;

  for (const std::string& key : keys) {
    if (!current.contains(key)) {
      return "";
    }
    current = current[key];
  }

  if (current.is_string()) {
    return current.get<std::string>();
  }

  return "";
}

nlohmann::json HandleResponse(const FilePtr& file) {
  std::string result;
  while (true) {
    char buffer[64 * 1024];
    auto ret = file->Read(buffer, sizeof(buffer));
    if (ret < 0)
      return nullptr;
    if (ret == 0)
      break;
    result.append(buffer, buffer + ret);
  }
  VLOG(1) << "Response:\n" << result;

  nlohmann::json value = nlohmann::json::parse(result,
                                               /* parser callback */ nullptr,
                                               /* allow exceptions */ false);
  return value;
}

// Tests using httpbin can sometimes be flaky.  We get HTTP 502 errors when it
// is overloaded.  This will retry a test with delays, up to a limit, if the
// HTTP status code is 502.
void RetryTest(std::function<HttpFile*()> setup,
               std::function<void(FilePtr&)> pre_read,
               std::function<void(FilePtr&, nlohmann::json)> post_read) {
  nlohmann::json json;
  FilePtr file;

  for (int i = 0; i < 3; ++i) {
    file.reset(setup());

    ASSERT_TRUE(file->Open());

    pre_read(file);

    json = HandleResponse(file);

    if (file->HttpStatusCode() != 502) {
      // Not a 502 error, so take this result.
      break;
    }

    // Delay with exponential increase (1s, 2s, 4s), then loop try again.
    int delay = 1 << i;
    LOG(WARNING) << "httpbin failure (" << file->HttpStatusCode() << "): "
                 << "Delaying " << delay << " seconds and retrying.";
    std::this_thread::sleep_for(std::chrono::seconds(delay));
  }

  // Out of retries?  Check what we have.
  post_read(file, json);
}

}  // namespace

TEST(HttpFileTest, BasicGet) {
  RetryTest(
      []() -> HttpFile* {
        return new HttpFile(HttpMethod::kGet, "https://httpbin.org/anything");
      },
      [](FilePtr&) -> void {},
      [](FilePtr& file, nlohmann::json json) -> void {
        ASSERT_TRUE(json.is_object());
        ASSERT_TRUE(file.release()->Close());
        ASSERT_JSON_STRING(json, "method", "GET");
      });
}

TEST(HttpFileTest, CustomHeaders) {
  RetryTest(
      []() -> HttpFile* {
        std::vector<std::string> headers{"Host: foo", "X-My-Header: Something"};
        return new HttpFile(HttpMethod::kGet, "https://httpbin.org/anything",
                            "", headers, 0);
      },
      [](FilePtr&) -> void {},
      [](FilePtr& file, nlohmann::json json) -> void {
        ASSERT_TRUE(json.is_object());
        ASSERT_TRUE(file.release()->Close());

        ASSERT_JSON_STRING(json, "method", "GET");
        ASSERT_JSON_STRING(json, "headers.Host", "foo");
        ASSERT_JSON_STRING(json, "headers.X-My-Header", "Something");
      });
}

TEST(HttpFileTest, BasicPost) {
  const std::string data = "abcd";

  RetryTest(
      []() -> HttpFile* {
        return new HttpFile(HttpMethod::kPost, "https://httpbin.org/anything");
      },
      [&data](FilePtr& file) -> void {
        ASSERT_EQ(file->Write(data.data(), data.size()),
                  static_cast<int64_t>(data.size()));
        ASSERT_TRUE(file->Flush());
      },
      [&data](FilePtr& file, nlohmann::json json) -> void {
        ASSERT_TRUE(json.is_object());
        ASSERT_TRUE(file.release()->Close());

        ASSERT_JSON_STRING(json, "method", "POST");
        ASSERT_JSON_STRING(json, "data", data);
        ASSERT_JSON_STRING(json, "headers.Content-Type",
                           "application/octet-stream");

        // Curl may choose to send chunked or not based on the data.  We request
        // chunked encoding, but don't control if it is actually used.  If we
        // get chunked transfer, there is no Content-Length header reflected
        // back to us.
        if (!GetJsonString(json, "headers.Content-Length").empty()) {
          ASSERT_JSON_STRING(json, "headers.Content-Length",
                             std::to_string(data.size()));
        } else {
          ASSERT_JSON_STRING(json, "headers.Transfer-Encoding", "chunked");
        }
      });
}

TEST(HttpFileTest, BasicPut) {
  const std::string data = "abcd";

  RetryTest(
      []() -> HttpFile* {
        return new HttpFile(HttpMethod::kPut, "https://httpbin.org/anything");
      },
      [&data](FilePtr& file) -> void {
        ASSERT_EQ(file->Write(data.data(), data.size()),
                  static_cast<int64_t>(data.size()));
        ASSERT_TRUE(file->Flush());
      },
      [&data](FilePtr& file, nlohmann::json json) -> void {
        ASSERT_TRUE(json.is_object());
        ASSERT_TRUE(file.release()->Close());

        ASSERT_JSON_STRING(json, "method", "PUT");
        ASSERT_JSON_STRING(json, "data", data);
        ASSERT_JSON_STRING(json, "headers.Content-Type",
                           "application/octet-stream");

        // Curl may choose to send chunked or not based on the data.  We request
        // chunked encoding, but don't control if it is actually used.  If we
        // get chunked transfer, there is no Content-Length header reflected
        // back to us.
        if (!GetJsonString(json, "headers.Content-Length").empty()) {
          ASSERT_JSON_STRING(json, "headers.Content-Length",
                             std::to_string(data.size()));
        } else {
          ASSERT_JSON_STRING(json, "headers.Transfer-Encoding", "chunked");
        }
      });
}

TEST(HttpFileTest, MultipleWrites) {
  const std::string data1 = "abcd";
  const std::string data2 = "efgh";
  const std::string data3 = "ijkl";
  const std::string data4 = "mnop";

  RetryTest(
      []() -> HttpFile* {
        return new HttpFile(HttpMethod::kPut, "https://httpbin.org/anything");
      },
      [&data1, &data2, &data3, &data4](FilePtr& file) -> void {
        ASSERT_EQ(file->Write(data1.data(), data1.size()),
                  static_cast<int64_t>(data1.size()));
        ASSERT_EQ(file->Write(data2.data(), data2.size()),
                  static_cast<int64_t>(data2.size()));
        ASSERT_EQ(file->Write(data3.data(), data3.size()),
                  static_cast<int64_t>(data3.size()));
        ASSERT_EQ(file->Write(data4.data(), data4.size()),
                  static_cast<int64_t>(data4.size()));
        ASSERT_TRUE(file->Flush());
      },
      [&data1, &data2, &data3, &data4](FilePtr& file,
                                       nlohmann::json json) -> void {
        ASSERT_TRUE(json.is_object());
        ASSERT_TRUE(file.release()->Close());

        ASSERT_JSON_STRING(json, "method", "PUT");
        ASSERT_JSON_STRING(json, "data", data1 + data2 + data3 + data4);
        ASSERT_JSON_STRING(json, "headers.Content-Type",
                           "application/octet-stream");

        // Curl may choose to send chunked or not based on the data.  We request
        // chunked encoding, but don't control if it is actually used.  If we
        // get chunked transfer, there is no Content-Length header reflected
        // back to us.
        if (!GetJsonString(json, "headers.Content-Length").empty()) {
          auto totalSize =
              data1.size() + data2.size() + data3.size() + data4.size();
          ASSERT_JSON_STRING(json, "headers.Content-Length",
                             std::to_string(totalSize));
        } else {
          ASSERT_JSON_STRING(json, "headers.Transfer-Encoding", "chunked");
        }
      });
}

// TODO: Test chunked uploads explicitly.

TEST(HttpFileTest, Error404) {
  RetryTest(
      []() -> HttpFile* {
        return new HttpFile(HttpMethod::kGet, "https://httpbin.org/status/404");
      },
      [](FilePtr&) -> void {},
      [](FilePtr& file, nlohmann::json) -> void {
        // The site returns an empty response, not JSON.
        auto status = file.release()->CloseWithStatus();
        ASSERT_FALSE(status.ok());
        ASSERT_EQ(status.error_code(), error::HTTP_FAILURE);
      });
}

TEST(HttpFileTest, TimeoutTriggered) {
  RetryTest(
      []() -> HttpFile* {
        return new HttpFile(HttpMethod::kGet, "https://httpbin.org/delay/8", "",
                            {}, 1);
      },
      [](FilePtr&) -> void {},
      [](FilePtr& file, nlohmann::json) -> void {
        // Request should timeout; error is reported in Close/CloseWithStatus.
        auto status = file.release()->CloseWithStatus();
        ASSERT_FALSE(status.ok());
        ASSERT_EQ(status.error_code(), error::TIME_OUT);
      });
}

TEST(HttpFileTest, TimeoutNotTriggered) {
  RetryTest(
      []() -> HttpFile* {
        return new HttpFile(HttpMethod::kGet, "https://httpbin.org/delay/1", "",
                            {}, 5);
      },
      [](FilePtr&) -> void {},
      [](FilePtr& file, nlohmann::json json) -> void {
        // The timeout was not triggered.  We got back some JSON.
        ASSERT_TRUE(json.is_object());
        ASSERT_TRUE(file.release()->Close());
      });
}

}  // namespace shaka
