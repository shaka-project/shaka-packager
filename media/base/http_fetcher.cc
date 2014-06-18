// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "media/base/http_fetcher.h"

#include <curl/curl.h>
#include "base/strings/stringprintf.h"

namespace {
const char kUserAgentString[] = "edash-packager-http_fetcher/1.0";

// Scoped CURL implementation which cleans up itself when goes out of scope.
class ScopedCurl {
 public:
  ScopedCurl() { ptr_ = curl_easy_init(); }
  ~ScopedCurl() {
    if (ptr_)
      curl_easy_cleanup(ptr_);
  }

  CURL* get() { return ptr_; }

 private:
  CURL* ptr_;
  DISALLOW_COPY_AND_ASSIGN(ScopedCurl);
};

size_t AppendToString(char* ptr, size_t size, size_t nmemb, std::string* response) {
  DCHECK(ptr);
  DCHECK(response);
  const size_t total_size = size * nmemb;
  response->append(ptr, total_size);
  return total_size;
}
}  // namespace

namespace media {

HttpFetcher::HttpFetcher() {}
HttpFetcher::~HttpFetcher() {}

SimpleHttpFetcher::SimpleHttpFetcher() : timeout_in_seconds_(0) {
  curl_global_init(CURL_GLOBAL_DEFAULT);
}

SimpleHttpFetcher::SimpleHttpFetcher(uint32 timeout_in_seconds)
    : timeout_in_seconds_(timeout_in_seconds) {
  curl_global_init(CURL_GLOBAL_DEFAULT);
}

SimpleHttpFetcher::~SimpleHttpFetcher() {
  curl_global_cleanup();
}

Status SimpleHttpFetcher::Get(const std::string& path, std::string* response) {
  return FetchInternal(GET, path, "", response);
}

Status SimpleHttpFetcher::Post(const std::string& path,
                               const std::string& data,
                               std::string* response) {
  return FetchInternal(POST, path, data, response);
}

Status SimpleHttpFetcher::FetchInternal(HttpMethod method,
                                        const std::string& path,
                                        const std::string& data,
                                        std::string* response) {
  DCHECK(method == GET || method == POST);

  ScopedCurl scoped_curl;
  CURL* curl = scoped_curl.get();
  if (!curl) {
    LOG(ERROR) << "curl_easy_init() failed.";
    return Status(error::HTTP_FAILURE, "curl_easy_init() failed.");
  }
  response->clear();

  curl_easy_setopt(curl, CURLOPT_URL, path.c_str());
  curl_easy_setopt(curl, CURLOPT_USERAGENT, kUserAgentString);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout_in_seconds_);
  curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, AppendToString);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, response);
  if (method == POST) {
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, data.size());
  }

  CURLcode res = curl_easy_perform(curl);
  if (res != CURLE_OK) {
    std::string error_message = base::StringPrintf(
        "curl_easy_perform() failed: %s.", curl_easy_strerror(res));
    if (res == CURLE_HTTP_RETURNED_ERROR) {
      long response_code = 0;
      curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
      error_message += base::StringPrintf(" Response code: %ld.", response_code);
    }

    LOG(ERROR) << error_message;
    return Status(
        res == CURLE_OPERATION_TIMEDOUT ? error::TIME_OUT : error::HTTP_FAILURE,
        error_message);
  }
  return Status::OK;
}

}  // namespace media
