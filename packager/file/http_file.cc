// Copyright 2020 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/file/http_file.h"

#include <curl/curl.h>
#include <gflags/gflags.h>

#include "packager/base/bind.h"
#include "packager/base/files/file_util.h"
#include "packager/base/logging.h"
#include "packager/base/strings/string_number_conversions.h"
#include "packager/base/strings/stringprintf.h"
#include "packager/base/threading/worker_pool.h"
#include "packager/version/version.h"

DEFINE_string(user_agent, "",
              "Set a custom User-Agent string for HTTP requests.");
DEFINE_string(ca_file,
              "",
              "Absolute path to the Certificate Authority file for the "
              "server cert. PEM format");
DEFINE_string(client_cert_file,
              "",
              "Absolute path to client certificate file.");
DEFINE_string(client_cert_private_key_file,
              "",
              "Absolute path to the Private Key file.");
DEFINE_string(client_cert_private_key_password,
              "",
              "Password to the private key file.");
DEFINE_bool(disable_peer_verification,
            false,
            "Disable peer verification. This is needed to talk to servers "
            "without valid certificates.");
DECLARE_uint64(io_cache_size);

namespace shaka {

namespace {

constexpr const char* kBinaryContentType = "application/octet-stream";
constexpr const int kMinLogLevelForCurlDebugFunction = 2;

size_t CurlWriteCallback(char* buffer, size_t size, size_t nmemb, void* user) {
  IoCache* cache = reinterpret_cast<IoCache*>(user);
  size_t length = size * nmemb;
  if (cache) {
    length = cache->Write(buffer, length);
    VLOG(3) << "CurlWriteCallback length=" << length;
  } else {
    // For the case of HTTP Put, the returned data may not be consumed. Return
    // the size of the data to avoid curl errors.
  }
  return length;
}

size_t CurlReadCallback(char* buffer, size_t size, size_t nitems, void* user) {
  IoCache* cache = reinterpret_cast<IoCache*>(user);
  size_t length = cache->Read(buffer, size * nitems);
  VLOG(3) << "CurlRead length=" << length;
  return length;
}

int CurlDebugCallback(CURL* /* handle */,
                      curl_infotype type,
                      const char* data,
                      size_t size,
                      void* /* userptr */) {
  const char* type_text;
  int log_level;
  bool in_hex;
  switch (type) {
    case CURLINFO_TEXT:
      type_text = "== Info";
      log_level = kMinLogLevelForCurlDebugFunction + 1;
      in_hex = false;
      break;
    case CURLINFO_HEADER_IN:
      type_text = "<= Recv header";
      log_level = kMinLogLevelForCurlDebugFunction;
      in_hex = false;
      break;
    case CURLINFO_HEADER_OUT:
      type_text = "=> Send header";
      log_level = kMinLogLevelForCurlDebugFunction;
      in_hex = false;
      break;
    case CURLINFO_DATA_IN:
      type_text = "<= Recv data";
      log_level = kMinLogLevelForCurlDebugFunction + 1;
      in_hex = true;
      break;
    case CURLINFO_DATA_OUT:
      type_text = "=> Send data";
      log_level = kMinLogLevelForCurlDebugFunction + 1;
      in_hex = true;
      break;
    case CURLINFO_SSL_DATA_IN:
      type_text = "<= Recv SSL data";
      log_level = kMinLogLevelForCurlDebugFunction + 2;
      in_hex = true;
      break;
    case CURLINFO_SSL_DATA_OUT:
      type_text = "=> Send SSL data";
      log_level = kMinLogLevelForCurlDebugFunction + 2;
      in_hex = true;
      break;
    default:
      // Ignore other debug data.
      return 0;
  }

  VLOG(log_level) << "\n\n"
                  << type_text << " (0x" << std::hex << size << std::dec
                  << " bytes)\n"
                  << (in_hex ? base::HexEncode(data, size)
                             : std::string(data, size));
  return 0;
}

class LibCurlInitializer {
 public:
  LibCurlInitializer() {
    curl_global_init(CURL_GLOBAL_DEFAULT);
  }

  ~LibCurlInitializer() {
    curl_global_cleanup();
  }

  LibCurlInitializer(const LibCurlInitializer&) = delete;
  LibCurlInitializer& operator=(const LibCurlInitializer&) = delete;
};

template <typename List>
bool AppendHeader(const std::string& header, List* list) {
  auto* temp = curl_slist_append(list->get(), header.c_str());
  if (temp) {
    list->release();  // Don't free old list since it's part of the new one.
    list->reset(temp);
    return true;
  } else {
    return false;
  }
}

}  // namespace

HttpFile::HttpFile(HttpMethod method, const std::string& url)
    : HttpFile(method, url, kBinaryContentType, {}, 0) {}

HttpFile::HttpFile(HttpMethod method,
                   const std::string& url,
                   const std::string& upload_content_type,
                   const std::vector<std::string>& headers,
                   int32_t timeout_in_seconds)
    : File(url.c_str()),
      url_(url),
      upload_content_type_(upload_content_type),
      timeout_in_seconds_(timeout_in_seconds),
      method_(method),
      download_cache_(FLAGS_io_cache_size),
      upload_cache_(FLAGS_io_cache_size),
      curl_(curl_easy_init()),
      status_(Status::OK),
      user_agent_(FLAGS_user_agent),
      task_exit_event_(base::WaitableEvent::ResetPolicy::MANUAL,
                       base::WaitableEvent::InitialState::NOT_SIGNALED) {
  static LibCurlInitializer lib_curl_initializer;
  if (user_agent_.empty()) {
    user_agent_ += "ShakaPackager/" + GetPackagerVersion();
  }

  // We will have at least one header, so use a null header to signal error
  // to Open.

  // Don't wait for 100-Continue.
  std::unique_ptr<curl_slist, CurlDelete> temp_headers;
  if (!AppendHeader("Expect:", &temp_headers))
    return;
  if (!upload_content_type.empty() &&
      !AppendHeader("Content-Type: " + upload_content_type_, &temp_headers)) {
    return;
  }
  if (method != HttpMethod::kGet &&
      !AppendHeader("Transfer-Encoding: chunked", &temp_headers)) {
    return;
  }
  for (const auto& item : headers) {
    if (!AppendHeader(item, &temp_headers)) {
      return;
    }
  }
  request_headers_ = std::move(temp_headers);
}

HttpFile::~HttpFile() {}

bool HttpFile::Open() {
  VLOG(2) << "Opening " << url_;

  if (!curl_ || !request_headers_) {
    LOG(ERROR) << "curl_easy_init() failed.";
    return false;
  }
  // TODO: Try to connect initially so we can return connection error here.

  // TODO: Implement retrying with exponential backoff, see
  // "widevine_key_source.cc"

  base::WorkerPool::PostTask(
      FROM_HERE, base::Bind(&HttpFile::ThreadMain, base::Unretained(this)),
      /* task_is_slow= */ true);

  return true;
}

Status HttpFile::CloseWithStatus() {
  VLOG(2) << "Closing " << url_;
  // Close the cache first so the thread will finish uploading. Otherwise it
  // will wait for more data forever.
  download_cache_.Close();
  upload_cache_.Close();
  task_exit_event_.Wait();

  const Status result = status_;
  LOG_IF(ERROR, !result.ok()) << "HttpFile request failed: " << result;
  delete this;
  return result;
}

bool HttpFile::Close() {
  return CloseWithStatus().ok();
}

int64_t HttpFile::Read(void* buffer, uint64_t length) {
  VLOG(2) << "Reading from " << url_ << ", length=" << length;
  return download_cache_.Read(buffer, length);
}

int64_t HttpFile::Write(const void* buffer, uint64_t length) {
  DCHECK(!upload_cache_.closed());
  VLOG(2) << "Writing to " << url_ << ", length=" << length;
  return upload_cache_.Write(buffer, length);
}

int64_t HttpFile::Size() {
  VLOG(1) << "HttpFile does not support Size().";
  return -1;
}

bool HttpFile::Flush() {
  // Wait for curl to read any data we may have buffered.
  upload_cache_.WaitUntilEmptyOrClosed();
  return true;
}

bool HttpFile::Seek(uint64_t position) {
  LOG(ERROR) << "HttpFile does not support Seek().";
  return false;
}

bool HttpFile::Tell(uint64_t* position) {
  LOG(ERROR) << "HttpFile does not support Tell().";
  return false;
}

void HttpFile::CurlDelete::operator()(CURL* curl) {
  curl_easy_cleanup(curl);
}

void HttpFile::CurlDelete::operator()(curl_slist* headers) {
  curl_slist_free_all(headers);
}

void HttpFile::SetupRequest() {
  auto* curl = curl_.get();

  switch (method_) {
    case HttpMethod::kGet:
      curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
      break;
    case HttpMethod::kPost:
      curl_easy_setopt(curl, CURLOPT_POST, 1L);
      break;
    case HttpMethod::kPut:
      curl_easy_setopt(curl, CURLOPT_PUT, 1L);
      break;
  }

  curl_easy_setopt(curl, CURLOPT_URL, url_.c_str());
  curl_easy_setopt(curl, CURLOPT_USERAGENT, user_agent_.c_str());
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout_in_seconds_);
  curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &CurlWriteCallback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &download_cache_);
  if (method_ != HttpMethod::kGet) {
    curl_easy_setopt(curl, CURLOPT_READFUNCTION, &CurlReadCallback);
    curl_easy_setopt(curl, CURLOPT_READDATA, &upload_cache_);
  }

  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, request_headers_.get());

  if (FLAGS_disable_peer_verification)
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);

  // Client authentication
  if (!FLAGS_client_cert_private_key_file.empty() &&
      !FLAGS_client_cert_file.empty()) {
    curl_easy_setopt(curl, CURLOPT_SSLKEY,
                     FLAGS_client_cert_private_key_file.data());
    curl_easy_setopt(curl, CURLOPT_SSLCERT, FLAGS_client_cert_file.data());
    curl_easy_setopt(curl, CURLOPT_SSLKEYTYPE, "PEM");
    curl_easy_setopt(curl, CURLOPT_SSLCERTTYPE, "PEM");

    if (!FLAGS_client_cert_private_key_password.empty()) {
      curl_easy_setopt(curl, CURLOPT_KEYPASSWD,
                       FLAGS_client_cert_private_key_password.data());
    }
  }
  if (!FLAGS_ca_file.empty()) {
    curl_easy_setopt(curl, CURLOPT_CAINFO, FLAGS_ca_file.data());
  }

  if (VLOG_IS_ON(kMinLogLevelForCurlDebugFunction)) {
    curl_easy_setopt(curl, CURLOPT_DEBUGFUNCTION, CurlDebugCallback);
    curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
  }
}

void HttpFile::ThreadMain() {
  SetupRequest();

  CURLcode res = curl_easy_perform(curl_.get());
  if (res != CURLE_OK) {
    std::string error_message = curl_easy_strerror(res);
    if (res == CURLE_HTTP_RETURNED_ERROR) {
      long response_code = 0;
      curl_easy_getinfo(curl_.get(), CURLINFO_RESPONSE_CODE, &response_code);
      error_message +=
          base::StringPrintf(", response code: %ld.", response_code);
    }

    status_ = Status(
        res == CURLE_OPERATION_TIMEDOUT ? error::TIME_OUT : error::HTTP_FAILURE,
        error_message);
  }

  download_cache_.Close();
  task_exit_event_.Signal();
}

}  // namespace shaka
