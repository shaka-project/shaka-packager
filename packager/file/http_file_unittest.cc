// Copyright 2020 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/file/http_file.h>

#include <memory>
#include <vector>

#include <absl/strings/str_split.h>
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <packager/file.h>
#include <packager/file/file_closer.h>
#include <packager/macros/logging.h>
#include <packager/media/test/test_web_server.h>

#define ASSERT_JSON_STRING(json, key, value) \
  ASSERT_EQ(GetJsonString((json), (key)), (value)) << "JSON is " << (json)

namespace shaka {

namespace {

const std::vector<std::string> kNoHeaders;
const std::string kNoContentType;
const std::string kBinaryContentType = "application/octet-stream";
const int kDefaultTestTimeout = 10;  // For a local, embedded server

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

// Quoting gtest docs:
//   "For each TEST_F, GoogleTest will create a fresh test fixture object,
//   immediately call SetUp(), run the test body, call TearDown(), and then
//   delete the test fixture object."
// So we don't need a TearDown method.  The destructor on TestWebServer is good
// enough.
class HttpFileTest : public testing::Test {
 protected:
  void SetUp() override { ASSERT_TRUE(server_.Start()); }

  media::TestWebServer server_;
};

}  // namespace

TEST_F(HttpFileTest, BasicGet) {
  FilePtr file(new HttpFile(HttpMethod::kGet, server_.ReflectUrl(),
                            kNoContentType, kNoHeaders, kDefaultTestTimeout));
  ASSERT_TRUE(file);
  ASSERT_TRUE(file->Open());

  auto json = HandleResponse(file);
  ASSERT_TRUE(json.is_object());
  ASSERT_TRUE(file.release()->Close());
  ASSERT_JSON_STRING(json, "method", "GET");
}

TEST_F(HttpFileTest, CustomHeaders) {
  std::vector<std::string> headers{"Host: foo", "X-My-Header: Something"};
  FilePtr file(new HttpFile(HttpMethod::kGet, server_.ReflectUrl(),
                            kNoContentType, headers, kDefaultTestTimeout));
  ASSERT_TRUE(file);
  ASSERT_TRUE(file->Open());

  auto json = HandleResponse(file);
  ASSERT_TRUE(json.is_object());
  ASSERT_TRUE(file.release()->Close());

  ASSERT_JSON_STRING(json, "method", "GET");
  ASSERT_JSON_STRING(json, "headers.Host", "foo");
  ASSERT_JSON_STRING(json, "headers.X-My-Header", "Something");
}

TEST_F(HttpFileTest, BasicPost) {
  FilePtr file(new HttpFile(HttpMethod::kPost, server_.ReflectUrl(),
                            kBinaryContentType, kNoHeaders,
                            kDefaultTestTimeout));
  ASSERT_TRUE(file);
  ASSERT_TRUE(file->Open());

  const std::string data = "abcd";

  ASSERT_EQ(file->Write(data.data(), data.size()),
            static_cast<int64_t>(data.size()));
  // Signal that there will be no more writes.
  // If we don't do this, the request can hang in libcurl.
  file->CloseForWriting();

  auto json = HandleResponse(file);
  ASSERT_TRUE(json.is_object());
  ASSERT_TRUE(file.release()->Close());

  ASSERT_JSON_STRING(json, "method", "POST");
  ASSERT_JSON_STRING(json, "body", data);
  ASSERT_JSON_STRING(json, "headers.Content-Type", kBinaryContentType);

  // Curl may choose to send chunked or not based on the data.  We request
  // chunked encoding, but don't control if it is actually used.  If we get
  // chunked transfer, there is no Content-Length header reflected back to us.
  if (!GetJsonString(json, "headers.Content-Length").empty()) {
    ASSERT_JSON_STRING(json, "headers.Content-Length",
                       std::to_string(data.size()));
  } else {
    ASSERT_JSON_STRING(json, "headers.Transfer-Encoding", "chunked");
  }
}

TEST_F(HttpFileTest, BasicPut) {
  FilePtr file(new HttpFile(HttpMethod::kPut, server_.ReflectUrl(),
                            kBinaryContentType, kNoHeaders,
                            kDefaultTestTimeout));
  ASSERT_TRUE(file);
  ASSERT_TRUE(file->Open());

  const std::string data = "abcd";

  ASSERT_EQ(file->Write(data.data(), data.size()),
            static_cast<int64_t>(data.size()));
  // Signal that there will be no more writes.
  // If we don't do this, the request can hang in libcurl.
  file->CloseForWriting();

  auto json = HandleResponse(file);
  ASSERT_TRUE(json.is_object());
  ASSERT_TRUE(file.release()->Close());

  ASSERT_JSON_STRING(json, "method", "PUT");
  ASSERT_JSON_STRING(json, "body", data);
  ASSERT_JSON_STRING(json, "headers.Content-Type", kBinaryContentType);

  // Curl may choose to send chunked or not based on the data.  We request
  // chunked encoding, but don't control if it is actually used.  If we get
  // chunked transfer, there is no Content-Length header reflected back to us.
  if (!GetJsonString(json, "headers.Content-Length").empty()) {
    ASSERT_JSON_STRING(json, "headers.Content-Length",
                       std::to_string(data.size()));
  } else {
    ASSERT_JSON_STRING(json, "headers.Transfer-Encoding", "chunked");
  }
}

TEST_F(HttpFileTest, MultipleWrites) {
  FilePtr file(new HttpFile(HttpMethod::kPut, server_.ReflectUrl(),
                            kBinaryContentType, kNoHeaders,
                            kDefaultTestTimeout));
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
  // Signal that there will be no more writes.
  // If we don't do this, the request can hang in libcurl.
  file->CloseForWriting();

  auto json = HandleResponse(file);
  ASSERT_TRUE(json.is_object());
  ASSERT_TRUE(file.release()->Close());

  ASSERT_JSON_STRING(json, "method", "PUT");
  ASSERT_JSON_STRING(json, "body", data1 + data2 + data3 + data4);
  ASSERT_JSON_STRING(json, "headers.Content-Type", kBinaryContentType);

  // Curl may choose to send chunked or not based on the data.  We request
  // chunked encoding, but don't control if it is actually used.  If we get
  // chunked transfer, there is no Content-Length header reflected back to us.
  if (!GetJsonString(json, "headers.Content-Length").empty()) {
    auto totalSize = data1.size() + data2.size() + data3.size() + data4.size();
    ASSERT_JSON_STRING(json, "headers.Content-Length",
                       std::to_string(totalSize));
  } else {
    ASSERT_JSON_STRING(json, "headers.Transfer-Encoding", "chunked");
  }
}

TEST_F(HttpFileTest, MultipleChunks) {
  FilePtr file(new HttpFile(HttpMethod::kPut, server_.ReflectUrl(),
                            kBinaryContentType, kNoHeaders,
                            kDefaultTestTimeout));
  ASSERT_TRUE(file);
  ASSERT_TRUE(file->Open());

  // Each of these is written as an explicit chunk to the server.
  const std::string data1 = "abcd";
  const std::string data2 = "efgh";
  const std::string data3 = "ijkl";
  const std::string data4 = "mnop";

  ASSERT_EQ(file->Write(data1.data(), data1.size()),
            static_cast<int64_t>(data1.size()));
  // Flush the first chunk.
  ASSERT_TRUE(file->Flush());

  ASSERT_EQ(file->Write(data2.data(), data2.size()),
            static_cast<int64_t>(data2.size()));
  // Flush the second chunk.
  ASSERT_TRUE(file->Flush());

  ASSERT_EQ(file->Write(data3.data(), data3.size()),
            static_cast<int64_t>(data3.size()));
  // Flush the third chunk.
  ASSERT_TRUE(file->Flush());

  ASSERT_EQ(file->Write(data4.data(), data4.size()),
            static_cast<int64_t>(data4.size()));
  // Flush the fourth chunk.
  ASSERT_TRUE(file->Flush());

  // Signal that there will be no more writes.
  // If we don't do this, the request can hang in libcurl.
  file->CloseForWriting();

  auto json = HandleResponse(file);
  ASSERT_TRUE(json.is_object());
  ASSERT_TRUE(file.release()->Close());

  ASSERT_JSON_STRING(json, "method", "PUT");
  ASSERT_JSON_STRING(json, "body", data1 + data2 + data3 + data4);
  ASSERT_JSON_STRING(json, "headers.Content-Type", kBinaryContentType);
  ASSERT_JSON_STRING(json, "headers.Transfer-Encoding", "chunked");
}

TEST_F(HttpFileTest, Error404) {
  FilePtr file(new HttpFile(HttpMethod::kGet, server_.StatusCodeUrl(404),
                            kNoContentType, kNoHeaders, kDefaultTestTimeout));
  ASSERT_TRUE(file);
  ASSERT_TRUE(file->Open());

  // The site returns an empty response.
  uint8_t buffer[1];
  ASSERT_EQ(file->Read(buffer, sizeof(buffer)), 0);

  auto status = file.release()->CloseWithStatus();
  ASSERT_FALSE(status.ok());
  ASSERT_EQ(status.error_code(), error::HTTP_FAILURE);
}

TEST_F(HttpFileTest, TimeoutTriggered) {
  FilePtr file(new HttpFile(HttpMethod::kGet, server_.DelayUrl(8),
                            kNoContentType, kNoHeaders,
                            1 /* timeout in seconds */));
  ASSERT_TRUE(file);
  ASSERT_TRUE(file->Open());

  // Request should timeout; error is reported in Close/CloseWithStatus.
  uint8_t buffer[1];
  ASSERT_EQ(file->Read(buffer, sizeof(buffer)), 0);

  auto status = file.release()->CloseWithStatus();
  ASSERT_FALSE(status.ok());
  ASSERT_EQ(status.error_code(), error::TIME_OUT);
}

TEST_F(HttpFileTest, TimeoutNotTriggered) {
  FilePtr file(new HttpFile(HttpMethod::kGet, server_.DelayUrl(1),
                            kNoContentType, kNoHeaders,
                            5 /* timeout in seconds */));
  ASSERT_TRUE(file);
  ASSERT_TRUE(file->Open());

  auto json = HandleResponse(file);
  ASSERT_TRUE(json.is_object());
  ASSERT_TRUE(file.release()->Close());
}

}  // namespace shaka
