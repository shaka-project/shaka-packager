// Copyright 2014 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/media/base/http_key_fetcher.h>

#include <packager/file/file_closer.h>

namespace shaka {
namespace media {

namespace {

const char kSoapActionHeader[] =
    "SOAPAction: \"http://schemas.microsoft.com/DRM/2007/03/protocols/"
    "AcquirePackagingData\"";
const char kXmlContentType[] = "text/xml; charset=UTF-8";
const char kJsonContentType[] = "application/json";
constexpr size_t kBufferSize = 64 * 1024;

}  // namespace

HttpKeyFetcher::HttpKeyFetcher() : timeout_in_seconds_(0) {}

HttpKeyFetcher::HttpKeyFetcher(int32_t timeout_in_seconds)
    : timeout_in_seconds_(timeout_in_seconds) {}

HttpKeyFetcher::~HttpKeyFetcher() {}

Status HttpKeyFetcher::FetchKeys(const std::string& url,
                                 const std::string& request,
                                 std::string* response) {
  return Post(url, request, response);
}

Status HttpKeyFetcher::Get(const std::string& path, std::string* response) {
  return FetchInternal(HttpMethod::kGet, path, "", response);
}

Status HttpKeyFetcher::Post(const std::string& path,
                            const std::string& data,
                            std::string* response) {
  return FetchInternal(HttpMethod::kPost, path, data, response);
}

Status HttpKeyFetcher::FetchInternal(HttpMethod method,
                                     const std::string& path,
                                     const std::string& data,
                                     std::string* response) {
  std::string content_type;
  std::vector<std::string> headers;
  if (data.find("soap:Envelope") != std::string::npos) {
    // Adds Http headers for SOAP requests.
    content_type = kXmlContentType;
    headers.push_back(kSoapActionHeader);
  } else {
    content_type = kJsonContentType;
  }

  std::unique_ptr<HttpFile, FileCloser> file(
      new HttpFile(method, path, content_type, headers, timeout_in_seconds_));
  if (!file->Open()) {
    return Status(error::INTERNAL_ERROR, "Cannot open URL");
  }
  file->Write(data.data(), data.size());
  file->Flush();
  file->CloseForWriting();

  while (true) {
    char temp[kBufferSize];
    int64_t ret = file->Read(temp, kBufferSize);
    if (ret <= 0)
      break;
    response->append(temp, ret);
  }
  return file.release()->CloseWithStatus();
}

}  // namespace media
}  // namespace shaka
