// Copyright 2018 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/file/http_file.h"

#include <gflags/gflags.h>
#include "packager/base/bind.h"
#include "packager/base/files/file_util.h"
#include "packager/base/logging.h"
#include "packager/base/strings/string_number_conversions.h"
#include "packager/base/strings/stringprintf.h"
#include "packager/base/synchronization/lock.h"
#include "packager/base/threading/worker_pool.h"

DECLARE_uint64(io_cache_size);

namespace shaka {

// curl_ primitives stolen from `http_key_fetcher.cc`.
namespace {

const char kUserAgentString[] = "shaka-packager-uploader/0.1";

const int kMinLogLevelForCurlDebugFunction = 3;

size_t AppendToString(char* ptr,
                      size_t size,
                      size_t nmemb,
                      std::string* response) {
  DCHECK(ptr);
  DCHECK(response);
  const size_t total_size = size * nmemb;
  response->append(ptr, total_size);
  return total_size;
}

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
    // HTTPS
    /*
    case CURLINFO_SSL_DATA_IN:
      type_text = "<= Recv SSL data";
      log_level = kMinLogLevelForCurlDebugFunction + 2;
      break;
    case CURLINFO_SSL_DATA_OUT:
      type_text = "=> Send SSL data";
      log_level = kMinLogLevelForCurlDebugFunction + 2;
      break;
    */
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

}  // namespace

/// Create a HTTP client
HttpFile::HttpFile(const char* file_name, TransferMode transfer_mode)
    : File(file_name),
      transfer_mode_(transfer_mode),
      timeout_in_seconds_(0),
      cache_(FLAGS_io_cache_size),
      task_exit_event_(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                       base::WaitableEvent::InitialState::NOT_SIGNALED) {
  // FIXME: Prepend the scheme again. Improve: It could be https or even others.
  std::string scheme = "http://";
  resource_url_ = scheme + std::string(file_name);

  static LibCurlInitializer lib_curl_initializer;

  // Setup libcurl scope
  curl_ = scoped_curl.get();
  if (!curl_) {
    LOG(ERROR) << "curl_easy_init() failed.";
    // return Status(error::HTTP_FAILURE, "curl_easy_init() failed.");
    delete this;
  }
}

// Destructor
HttpFile::~HttpFile() {}

bool HttpFile::Open() {
  VLOG(1) << "Opening " << resource_url();

  if (transfer_mode_ == PUT_CHUNKED) {
    if (cache_.closed()) {
      cache_.Reopen();
    }
    base::WorkerPool::PostTask(
        FROM_HERE, base::Bind(&HttpFile::CurlPut, base::Unretained(this)),
        true  // task_is_slow
    );
  }

  return true;
}

void HttpFile::CurlPut() {
  // Put a libcurl handle into chunked transfer mode
  std::string request_body;
  Request(PUT, resource_url(), request_body, &response_body_);
}

bool HttpFile::Close() {
  VLOG(1) << "Closing " << resource_url();
  if (transfer_mode_ == PUT_CHUNKED) {
    cache_.Close();
  }
  task_exit_event_.Wait();
  delete this;
  return true;
}

int64_t HttpFile::Read(void* buffer, uint64_t length) {
  LOG(WARNING) << "HttpFile does not support Read().";
  return -1;
}

int64_t HttpFile::Write(const void* buffer, uint64_t length) {
  std::string url = resource_url();

  VLOG(1) << "Writing to " << url << ", length=" << length;

  // TODO: Implement retrying with exponential backoff, see
  // "widevine_key_source.cc"
  Status status;

  switch (transfer_mode_) {
    case PUT_CHUNKED: {
      uint64_t bytes_written = cache_.Write(buffer, length);
      VLOG(1) << "PUT CHUNK bytes_written: " << bytes_written;
      return bytes_written;
      break;
    }

    case PATCH_APPEND: {
      // Convert void pointer to C buffer into C++ std::string
      std::string request_body = std::string((const char*)buffer, length);

      // Debugging. Attention: This yields binary output which will ring your
      // bell.
      // VLOG(1) << "Request: " << request_body;

      // Perform HTTP request
      std::string response_body;
      status = Request(PATCH, url, request_body, &response_body);

      break;
    }

      /*
      case POST_RAW:
      case POST_MULTIPART:
      case PUT_FULL:
      */

    default:
      LOG(ERROR) << "TransferMode " << transfer_mode()
                 << " not implemented yet";
      // break;
  }

  // Debugging based on response status
  /*
  if (status.ok()) {
    VLOG(1) << "Writing chunk succeeded";

  } else {
    VLOG(1) << "Writing chunk failed";
    if (!response_body.empty()) {
      VLOG(2) << "Response:\n" << response_body;
    }
  }
  */

  // Always signal success to the downstream pipeline
  return length;
}

int64_t HttpFile::Size() {
  VLOG(1) << "HttpFile does not support Size().";
  return -1;
}

bool HttpFile::Flush() {
  // Do nothing on Flush.
  return true;
}

bool HttpFile::Seek(uint64_t position) {
  VLOG(1) << "HttpFile does not support Seek().";
  return false;
}

bool HttpFile::Tell(uint64_t* position) {
  VLOG(1) << "HttpFile does not support Tell().";
  return false;
}

// Perform HTTP request
Status HttpFile::Request(HttpMethod http_method,
                         const std::string& url,
                         const std::string& data,
                         std::string* response) {
  // DCHECK(http_method == GET || http_method == POST);
  VLOG(1) << "HttpFile::Request url=" << url;

  SetupRequestBase(http_method, url, response);
  SetupRequestData(data);

  // Perform HTTP request
  CURLcode res = curl_easy_perform(curl_);

  // Assume successful request
  Status status = Status::OK;

  // Handle request failure
  if (res != CURLE_OK) {
    std::string method_text = method_as_text(http_method);
    std::string error_message = base::StringPrintf(
        "%s request for %s failed. Reason: %s.", method_text.c_str(),
        url.c_str(), curl_easy_strerror(res));
    if (res == CURLE_HTTP_RETURNED_ERROR) {
      long response_code = 0;
      curl_easy_getinfo(curl_, CURLINFO_RESPONSE_CODE, &response_code);
      error_message +=
          base::StringPrintf(" Response code: %ld.", response_code);
    }

    // Signal error to logfile
    LOG(ERROR) << error_message;

    // Signal error to caller
    status = Status(
        res == CURLE_OPERATION_TIMEDOUT ? error::TIME_OUT : error::HTTP_FAILURE,
        error_message);
  }

  // Signal task completion
  task_exit_event_.Signal();

  // Return request status to caller
  return status;
}

// Configure curl_ handle with reasonable defaults
void HttpFile::SetupRequestBase(HttpMethod http_method,
                                const std::string& url,
                                std::string* response) {
  response->clear();

  // Configure HTTP request method/verb
  switch (http_method) {
    case GET:
      curl_easy_setopt(curl_, CURLOPT_HTTPGET, 1L);
      break;
    case POST:
      curl_easy_setopt(curl_, CURLOPT_POST, 1L);
      break;
    case PUT:
      curl_easy_setopt(curl_, CURLOPT_PUT, 1L);
      break;
    case PATCH:
      curl_easy_setopt(curl_, CURLOPT_CUSTOMREQUEST, "PATCH");
      break;
  }

  // Configure HTTP request
  curl_easy_setopt(curl_, CURLOPT_VERBOSE, 3);
  curl_easy_setopt(curl_, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl_, CURLOPT_USERAGENT, kUserAgentString);
  curl_easy_setopt(curl_, CURLOPT_TIMEOUT, timeout_in_seconds_);
  curl_easy_setopt(curl_, CURLOPT_FAILONERROR, 1L);
  curl_easy_setopt(curl_, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(curl_, CURLOPT_WRITEFUNCTION, AppendToString);
  curl_easy_setopt(curl_, CURLOPT_WRITEDATA, response);

  // HTTPS
  /*
  if (!client_cert_private_key_file_.empty() && !client_cert_file_.empty()) {
    // Some PlayReady packaging servers only allow connects via HTTPS with
    // client certificates.
    curl_easy_setopt(curl_, CURLOPT_SSLKEY,
                     client_cert_private_key_file_.data());
    if (!client_cert_private_key_password_.empty()) {
      curl_easy_setopt(curl_, CURLOPT_KEYPASSWD,
                       client_cert_private_key_password_.data());
    }
    curl_easy_setopt(curl_, CURLOPT_SSLKEYTYPE, "PEM");
    curl_easy_setopt(curl_, CURLOPT_SSLCERTTYPE, "PEM");
    curl_easy_setopt(curl_, CURLOPT_SSLCERT, client_cert_file_.data());
  }
  if (!ca_file_.empty()) {
    // Host validation needs to be off when using self-signed certificates.
    curl_easy_setopt(curl_, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(curl_, CURLOPT_CAINFO, ca_file_.data());
  }
  */

  // Enable libcurl debugging
  if (VLOG_IS_ON(kMinLogLevelForCurlDebugFunction)) {
    curl_easy_setopt(curl_, CURLOPT_DEBUGFUNCTION, CurlDebugFunction);
    curl_easy_setopt(curl_, CURLOPT_VERBOSE, 1L);
  }
}

// https://ec.haxx.se/callback-read.html
size_t read_callback(char* buffer, size_t size, size_t nitems, void* stream) {
  VLOG(3) << "read_callback";

  // Cast stream back to what is actually is
  // IoCache* cache = reinterpret_cast<IoCache*>(stream);
  IoCache* cache = (IoCache*)stream;
  VLOG(3) << "read_callback, cache: " << cache;

  // Copy cache content into buffer
  size_t length = cache->Read(buffer, size * nitems);
  VLOG(3) << "read_callback, length: " << length << "; buffer: " << buffer;
  return length;
}

// Configure curl_ handle wrt to transfer mode
void HttpFile::SetupRequestData(const std::string& data) {
  // if (method == POST || method == PUT || method == PATCH)

  // Collect HTTP request headers
  struct curl_slist* chunk = nullptr;

  chunk = curl_slist_append(chunk, "Content-Type: application/octet-stream");

  if (transfer_mode_ == PATCH_APPEND) {
    chunk = curl_slist_append(chunk, "Update-Range: append");

  } else if (transfer_mode_ == PUT_CHUNKED) {
    VLOG(1) << "SetupRequestData: PUT_CHUNKED";
    chunk = curl_slist_append(chunk, "Transfer-Encoding: chunked");
    chunk = curl_slist_append(chunk, "Expect:");

    curl_easy_setopt(curl_, CURLOPT_READFUNCTION, read_callback);
    curl_easy_setopt(curl_, CURLOPT_READDATA, &cache_);
    curl_easy_setopt(curl_, CURLOPT_UPLOAD, 1L);

    // curl_easy_setopt(curl_, CURLOPT_INFILESIZE_LARGE, 1000);
  }

  if (transfer_mode_ == POST_RAW || transfer_mode_ == PUT_FULL ||
      transfer_mode_ == PATCH_APPEND) {
    // Add request data
    curl_easy_setopt(curl_, CURLOPT_POSTFIELDS, data.c_str());
    curl_easy_setopt(curl_, CURLOPT_POSTFIELDSIZE, data.size());
  }

  // Add HTTP request headers
  curl_easy_setopt(curl_, CURLOPT_HTTPHEADER, chunk);
}

// Return HTTP request method (verb) as string
std::string HttpFile::method_as_text(HttpMethod method) {
  std::string method_text = "UNKNOWN";
  switch (method) {
    case GET:
      method_text = "GET";
      break;
    case POST:
      method_text = "POST";
      break;
    case PUT:
      method_text = "PUT";
      break;
    case PATCH:
      method_text = "PATCH";
      break;
  }
  return method_text;
}

}  // namespace shaka
