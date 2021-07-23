// Copyright 2020 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/file/http_file.h"

#include <gtest/gtest.h>

#include <memory>
#include <vector>

#include "packager/base/json/json_reader.h"
#include "packager/base/values.h"
#include "packager/file/file.h"
#include "packager/file/file_closer.h"

#define ASSERT_JSON_STRING(json, key, value)        \
  do {                                              \
    std::string actual;                             \
    ASSERT_TRUE((json)->GetString((key), &actual)); \
    ASSERT_EQ(actual, (value));                     \
  } while (false)

namespace shaka {

namespace {

using FilePtr = std::unique_ptr<HttpFile, FileCloser>;

std::unique_ptr<base::DictionaryValue> HandleResponse(const FilePtr& file) {
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

  auto value = base::JSONReader::Read(result);
  if (!value || !value->IsType(base::Value::TYPE_DICTIONARY))
    return nullptr;
  return std::unique_ptr<base::DictionaryValue>{
      static_cast<base::DictionaryValue*>(value.release())};
}

}  // namespace

TEST(HttpFileTest, DISABLED_BasicGet) {
  FilePtr file(new HttpFile(HttpMethod::kGet, "https://httpbin.org/anything"));
  ASSERT_TRUE(file);
  ASSERT_TRUE(file->Open());

  auto json = HandleResponse(file);
  ASSERT_TRUE(json);
  ASSERT_TRUE(file.release()->Close());
  ASSERT_JSON_STRING(json, "method", "GET");
}

TEST(HttpFileTest, DISABLED_CustomHeaders) {
  std::vector<std::string> headers{"Host: foo", "X-My-Header: Something"};
  FilePtr file(new HttpFile(HttpMethod::kGet, "https://httpbin.org/anything",
                            "", headers, 0));
  ASSERT_TRUE(file);
  ASSERT_TRUE(file->Open());

  auto json = HandleResponse(file);
  ASSERT_TRUE(json);
  ASSERT_TRUE(file.release()->Close());

  ASSERT_JSON_STRING(json, "method", "GET");
  ASSERT_JSON_STRING(json, "headers.Host", "foo");
  ASSERT_JSON_STRING(json, "headers.X-My-Header", "Something");
}

TEST(HttpFileTest, DISABLED_BasicPost) {
  FilePtr file(new HttpFile(HttpMethod::kPost, "https://httpbin.org/anything"));
  ASSERT_TRUE(file);
  ASSERT_TRUE(file->Open());

  const std::string data = "abcd";

  ASSERT_EQ(file->Write(data.data(), data.size()),
            static_cast<int64_t>(data.size()));
  ASSERT_TRUE(file->Flush());

  auto json = HandleResponse(file);
  ASSERT_TRUE(json);
  ASSERT_TRUE(file.release()->Close());

  ASSERT_JSON_STRING(json, "method", "POST");
  ASSERT_JSON_STRING(json, "data", data);
  ASSERT_JSON_STRING(json, "headers.Content-Type", "application/octet-stream");
  ASSERT_JSON_STRING(json, "headers.Content-Length",
                     std::to_string(data.size()));
}

TEST(HttpFileTest, DISABLED_BasicPut) {
  FilePtr file(new HttpFile(HttpMethod::kPut, "https://httpbin.org/anything"));
  ASSERT_TRUE(file);
  ASSERT_TRUE(file->Open());

  const std::string data = "abcd";

  ASSERT_EQ(file->Write(data.data(), data.size()),
            static_cast<int64_t>(data.size()));
  ASSERT_TRUE(file->Flush());

  auto json = HandleResponse(file);
  ASSERT_TRUE(json);
  ASSERT_TRUE(file.release()->Close());

  ASSERT_JSON_STRING(json, "method", "PUT");
  ASSERT_JSON_STRING(json, "data", data);
  ASSERT_JSON_STRING(json, "headers.Content-Type", "application/octet-stream");
  ASSERT_JSON_STRING(json, "headers.Content-Length",
                     std::to_string(data.size()));
}

TEST(HttpFileTest, DISABLED_MultipleWrites) {
  FilePtr file(new HttpFile(HttpMethod::kPut, "https://httpbin.org/anything"));
  ASSERT_TRUE(file);
  ASSERT_TRUE(file->Open());

  const std::string data1 = "abcd";
  const std::string data2 = "efgh";
  const std::string data3 = "ijkl";
  const std::string data4 = "mnop";

  ASSERT_EQ(file->Write(data1.data(), data1.size()),
            static_cast<int64_t>(data1.size()));
  ASSERT_EQ(file->Write(data2.data(), data2.size()),
            static_cast<int64_t>(data2.size()));
  ASSERT_EQ(file->Write(data3.data(), data3.size()),
            static_cast<int64_t>(data3.size()));
  ASSERT_EQ(file->Write(data4.data(), data4.size()),
            static_cast<int64_t>(data4.size()));
  ASSERT_TRUE(file->Flush());

  auto json = HandleResponse(file);
  ASSERT_TRUE(json);
  ASSERT_TRUE(file.release()->Close());

  ASSERT_JSON_STRING(json, "method", "PUT");
  ASSERT_JSON_STRING(json, "data", data1 + data2 + data3 + data4);
  ASSERT_JSON_STRING(json, "headers.Content-Type", "application/octet-stream");
  ASSERT_JSON_STRING(json, "headers.Content-Length",
                     std::to_string(data1.size() + data2.size() + data3.size() +
                                    data4.size()));
}

// TODO: Test chunked uploads.  Since we can only read the response, we have no
// way to detect if we are streaming the upload like we want.  httpbin seems to
// populate the Content-Length even if we don't give it in the request.

TEST(HttpFileTest, DISABLED_Error404) {
  FilePtr file(
      new HttpFile(HttpMethod::kGet, "https://httpbin.org/status/404"));
  ASSERT_TRUE(file);
  ASSERT_TRUE(file->Open());

  // The site returns an empty response.
  uint8_t buffer[1];
  ASSERT_EQ(file->Read(buffer, sizeof(buffer)), 0);

  auto status = file.release()->CloseWithStatus();
  ASSERT_FALSE(status.ok());
  ASSERT_EQ(status.error_code(), error::HTTP_FAILURE);
}

TEST(HttpFileTest, DISABLED_TimeoutTriggered) {
  FilePtr file(
      new HttpFile(HttpMethod::kGet, "https://httpbin.org/delay/8", "", {}, 1));
  ASSERT_TRUE(file);
  ASSERT_TRUE(file->Open());

  // Request should timeout; error is reported in Close/CloseWithStatus.
  uint8_t buffer[1];
  ASSERT_EQ(file->Read(buffer, sizeof(buffer)), 0);

  auto status = file.release()->CloseWithStatus();
  ASSERT_FALSE(status.ok());
  ASSERT_EQ(status.error_code(), error::TIME_OUT);
}

TEST(HttpFileTest, DISABLED_TimeoutNotTriggered) {
  FilePtr file(
      new HttpFile(HttpMethod::kGet, "https://httpbin.org/delay/1", "", {}, 5));
  ASSERT_TRUE(file);
  ASSERT_TRUE(file->Open());

  auto json = HandleResponse(file);
  ASSERT_TRUE(json);
  ASSERT_TRUE(file.release()->Close());
}

}  // namespace shaka
