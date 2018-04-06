// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/base/http_key_fetcher.h"

#include <curl/curl.h>

#include "packager/base/logging.h"
#include "packager/base/strings/string_number_conversions.h"
#include "packager/base/strings/stringprintf.h"
#include "packager/base/synchronization/lock.h"

namespace shaka {

namespace {
const char kUserAgentString[] = "shaka-packager-http_fetcher/1.0";
const char kSoapActionHeader[] =
    "SOAPAction: \"http://schemas.microsoft.com/DRM/2007/03/protocols/"
    "AcquirePackagingData\"";
const char kXmlContentTypeHeader[] = "Content-Type: text/xml; charset=UTF-8";
const char kJsonContentTypeHeader[] = "Content-Type: application/json";

const int kMinLogLevelForCurlDebugFunction = 2;

int CurlDebugFunction(CURL* /* handle */,
                      curl_infotype type,
                      const char* data,
                      size_t size,
                      void* /* userptr */) {
  const char* type_text;
  int log_level = kMinLogLevelForCurlDebugFunction;
  switch (type) {
    case CURLINFO_TEXT:
      type_text = "== Info";
      log_level = kMinLogLevelForCurlDebugFunction + 1;
      break;
    case CURLINFO_HEADER_IN:
      type_text = "<= Recv header";
      log_level = kMinLogLevelForCurlDebugFunction;
      break;
    case CURLINFO_HEADER_OUT:
      type_text = "=> Send header";
      log_level = kMinLogLevelForCurlDebugFunction;
      break;
    case CURLINFO_DATA_IN:
      type_text = "<= Recv data";
      log_level = kMinLogLevelForCurlDebugFunction + 1;
      break;
    case CURLINFO_DATA_OUT:
      type_text = "=> Send data";
      log_level = kMinLogLevelForCurlDebugFunction + 1;
      break;
    case CURLINFO_SSL_DATA_IN:
      type_text = "<= Recv SSL data";
      log_level = kMinLogLevelForCurlDebugFunction + 2;
      break;
    case CURLINFO_SSL_DATA_OUT:
      type_text = "=> Send SSL data";
      log_level = kMinLogLevelForCurlDebugFunction + 2;
      break;
    default:
      // Ignore other debug data.
      return 0;
  }

  VLOG(log_level) << "\n\n"
                  << type_text << " (0x" << std::hex << size << std::dec
                  << " bytes)"
                  << "\n"
                  << std::string(data, size) << "\nHex Format: \n"
                  << base::HexEncode(data, size);
  return 0;
}

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

class LibCurlInitializer {
 public:
  LibCurlInitializer() : initialized_(false) {
    base::AutoLock lock(lock_);
    if (!initialized_) {
      curl_global_init(CURL_GLOBAL_DEFAULT);
      initialized_ = true;
    }
  }

  ~LibCurlInitializer() {
    base::AutoLock lock(lock_);
    if (initialized_) {
      curl_global_cleanup();
      initialized_ = false;
    }
  }

 private:
  base::Lock lock_;
  bool initialized_;

  DISALLOW_COPY_AND_ASSIGN(LibCurlInitializer);
};

}  // namespace

namespace media {

HttpKeyFetcher::HttpKeyFetcher() : timeout_in_seconds_(0) {}

HttpKeyFetcher::HttpKeyFetcher(uint32_t timeout_in_seconds)
    : timeout_in_seconds_(timeout_in_seconds) {}

HttpKeyFetcher::~HttpKeyFetcher() {}

Status HttpKeyFetcher::FetchKeys(const std::string& url,
                                 const std::string& request,
                                 std::string* response) {
  return Post(url, request, response);
}

Status HttpKeyFetcher::Get(const std::string& path, std::string* response) {
  return FetchInternal(GET, path, "", response);
}

Status HttpKeyFetcher::Post(const std::string& path,
                            const std::string& data,
                            std::string* response) {
  return FetchInternal(POST, path, data, response);
}

Status HttpKeyFetcher::FetchInternal(HttpMethod method,
                                     const std::string& path,
                                     const std::string& data,
                                     std::string* response) {
  DCHECK(method == GET || method == POST);
  static LibCurlInitializer lib_curl_initializer;

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

  if (!client_cert_private_key_file_.empty() && !client_cert_file_.empty()) {
    // Some PlayReady packaging servers only allow connects via HTTPS with
    // client certificates.
    curl_easy_setopt(curl, CURLOPT_SSLKEY,
                     client_cert_private_key_file_.data());
    if (!client_cert_private_key_password_.empty()) {
      curl_easy_setopt(curl, CURLOPT_KEYPASSWD,
                       client_cert_private_key_password_.data());
    }
    curl_easy_setopt(curl, CURLOPT_SSLKEYTYPE, "PEM");
    curl_easy_setopt(curl, CURLOPT_SSLCERTTYPE, "PEM");
    curl_easy_setopt(curl, CURLOPT_SSLCERT, client_cert_file_.data());
  }
  if (!ca_file_.empty()) {
    // Host validation needs to be off when using self-signed certificates.
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(curl, CURLOPT_CAINFO, ca_file_.data());
  }
  if (method == POST) {
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, data.size());

    curl_slist* chunk = nullptr;
    if (data.find("soap:Envelope") != std::string::npos) {
      // Adds Http headers for SOAP requests.
      chunk = curl_slist_append(chunk, kXmlContentTypeHeader);
      chunk = curl_slist_append(chunk, kSoapActionHeader);
    } else {
      chunk = curl_slist_append(chunk, kJsonContentTypeHeader);
    }
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
  }

  if (VLOG_IS_ON(kMinLogLevelForCurlDebugFunction)) {
    curl_easy_setopt(curl, CURLOPT_DEBUGFUNCTION, CurlDebugFunction);
    curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
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
}  // namespace shaka
