// Copyright 2018 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/file/http_file.h"

#include "packager/base/files/file_util.h"
#include "packager/base/logging.h"
#include "packager/base/strings/string_number_conversions.h"
#include "packager/base/strings/stringprintf.h"
#include "packager/base/synchronization/lock.h"

namespace shaka {

    // curl primitives stolen from http_key_fetcher.cc
    namespace {

        const char kUserAgentString[] = "shaka-packager-uploader/0.1";
        const char kOctetStreamContentTypeHeader[] = "Content-Type: application/octet-stream";
        const char kUpdateRangeAppendHeader[] = "Update-Range: append";

        const int kMinLogLevelForCurlDebugFunction = 3;

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


    /// Create a HTTP client with no timeout
    HttpFile::HttpFile(const char* file_name, const char* mode)
            : File(file_name), file_mode_(mode), timeout_in_seconds_(0) {

      // FIXME: Prepending the assumed again is insufficient. It could be https or even others.
      std::string scheme = "http://";
      resource_url_ = scheme + std::string(file_name);
    }

    /// Create a HTTP client with timeout
    HttpFile::HttpFile(const char* file_name, const char* mode, uint32_t timeout_in_seconds)
            : File(file_name), file_mode_(mode), timeout_in_seconds_(timeout_in_seconds) {

      // FIXME: Prepending the assumed again is insufficient. It could be https or even others.
      std::string scheme = "http://";
      resource_url_ = scheme + std::string(file_name);
    }

    // Destructor
    HttpFile::~HttpFile() {}

    bool HttpFile::Open() {
      VLOG(1) << "Opening " << resource_url();
      return true;
    }

    bool HttpFile::Close() {
      VLOG(1) << "Closing " << resource_url();
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

      // TODO: Implement retrying with exponential backoff, see "widevine_key_source.cc"

      // Convert void pointer to C buffer into C++ std::string
      std::string request_body = std::string((const char*)buffer, length);

      // Debugging. Attention: This yields binary output which will ring your bell.
      //VLOG(1) << "Request: " << request_body;

      // Perform HTTP request
      std::string response_body;
      Status status = Request(&response_body, PATCH, url, request_body, true);

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
    Status HttpFile::Request(std::string* response,
                             HttpMethod method,
                             const std::string& url,
                             const std::string& data,
                             const bool append_data) {

      //DCHECK(method == GET || method == POST);

      static LibCurlInitializer lib_curl_initializer;

      // Setup libcurl scope
      ScopedCurl scoped_curl;
      CURL* curl = scoped_curl.get();
      if (!curl) {
        LOG(ERROR) << "curl_easy_init() failed.";
        return Status(error::HTTP_FAILURE, "curl_easy_init() failed.");
      }

      if (!SetupRequest(curl, response, method, url, data, append_data)) {
        LOG(ERROR) << "SetupRequest() failed.";
        return Status(error::HTTP_FAILURE, "SetupRequest() failed.");
      }

      // Perform HTTP request
      CURLcode res = curl_easy_perform(curl);

      // Handle request failure
      if (res != CURLE_OK) {

        std::string method_text = method_as_text(method);
        std::string error_message = base::StringPrintf(
                "%s request for %s failed. Reason: %s.", method_text.c_str(), url.c_str(), curl_easy_strerror(res));
        if (res == CURLE_HTTP_RETURNED_ERROR) {
          long response_code = 0;
          curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
          error_message += base::StringPrintf(" Response code: %ld.", response_code);
        }

        // Signal error to logfile
        LOG(ERROR) << error_message;


        // Signal error to caller
        return Status(
                res == CURLE_OPERATION_TIMEDOUT ? error::TIME_OUT : error::HTTP_FAILURE,
                error_message);
      }

      // Request succeeded
      return Status::OK;
    }


    // Configure curl handle with reasonable defaults
    bool HttpFile::SetupRequest(CURL* curl,
                                std::string* response,
                                HttpMethod method,
                                const std::string& url,
                                const std::string& data,
                                const bool append_data) {

      response->clear();

      // Configure HTTP request method (verb)
      std::string method_text = "UNKNOWN";
      switch (method) {
        case GET:
          curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
          method_text = "GET";
          break;
        case POST:
          curl_easy_setopt(curl, CURLOPT_POST, 1L);
          method_text = "POST";
          break;
        case PUT:
          curl_easy_setopt(curl, CURLOPT_PUT, 1L);
          method_text = "PUT";
          break;
        case PATCH:
          curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PATCH");
          method_text = "PATCH";
          break;
      }

      // Configure HTTP request
      curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
      curl_easy_setopt(curl, CURLOPT_USERAGENT, kUserAgentString);
      curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout_in_seconds_);
      curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
      curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
      curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, AppendToString);
      curl_easy_setopt(curl, CURLOPT_WRITEDATA, response);

      // HTTPS
      /*
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
      */

      if (method == POST || method == PUT || method == PATCH) {

        // Add HTTP request headers
        curl_slist* chunk = nullptr;
        chunk = curl_slist_append(chunk, kOctetStreamContentTypeHeader);
        if (append_data) {
          chunk = curl_slist_append(chunk, kUpdateRangeAppendHeader);
        }
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);

        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, data.size());
      }

      // Enable libcurl debugging
      if (VLOG_IS_ON(kMinLogLevelForCurlDebugFunction)) {
        curl_easy_setopt(curl, CURLOPT_DEBUGFUNCTION, CurlDebugFunction);
        curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
      }

      return true;

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
