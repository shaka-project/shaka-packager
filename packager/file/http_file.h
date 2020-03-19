// Copyright 2018 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_FILE_HTTP_H_
#define PACKAGER_FILE_HTTP_H_

#include <curl/curl.h>

#include "packager/base/compiler_specific.h"
#include "packager/base/synchronization/waitable_event.h"
#include "packager/file/file.h"
#include "packager/file/io_cache.h"
#include "packager/status.h"

namespace shaka {

// Scoped CURL implementation which cleans up itself when goes out of scope.
// Stolen from `http_key_fetcher.cc`.
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

/// HttpFile delegates write calls to HTTP PUT requests.
///
/// About how to use this, please visit the corresponding documentation [1,2].
///
/// [1] https://google.github.io/shaka-packager/html/tutorials/http_upload.html
/// [2]
/// https://github.com/3QSDN/shaka-packager/blob/http-upload/docs/source/tutorials/http_upload.rst
///
class HttpFile : public File {
 public:

  /// Create a HTTP client
  /// @param file_name contains the url of the resource to be accessed.
  ///        Note that the file type prefix should be stripped off already.
  /// @param mode contains file access mode. Implementation dependent.
  HttpFile(const char* file_name, const char* mode, bool https);
  HttpFile(const char* file_name, const char* mode);

  /// @name File implementation overrides.
  /// @{
  bool Close() override;
  int64_t Read(void* buffer, uint64_t length) override;
  int64_t Write(const void* buffer, uint64_t length) override;
  int64_t Size() override;
  bool Flush() override;
  bool Seek(uint64_t position) override;
  bool Tell(uint64_t* position) override;
  /// @}

  /// @return The full resource url
  const std::string& resource_url() const { return resource_url_; }

 protected:
  // Destructor
  ~HttpFile() override;

  bool Open() override;

 private:
  enum HttpMethod {
    GET,
    POST,
    PUT,
    PATCH,
  };

  HttpFile(const HttpFile&) = delete;
  HttpFile& operator=(const HttpFile&) = delete;

  // Internal implementation of HTTP functions, e.g. Get and Post.
  Status Request(HttpMethod http_method,
                 const std::string& url,
                 const std::string& data,
                 std::string* response);

  void SetupRequestBase(HttpMethod http_method,
                        const std::string& url,
                        std::string* response);

  void SetupRequestData(const std::string& data);

  void CurlPut();

  std::string method_as_text(HttpMethod method);

  const char* file_mode_;
  std::string resource_url_;
  const uint32_t timeout_in_seconds_;
  IoCache cache_;

  ScopedCurl scoped_curl;
  CURL* curl_ = nullptr;

  std::string response_body_;

  // Signaled when the "curl easy perform" task completes.
  base::WaitableEvent task_exit_event_;
};

}  // namespace shaka

#endif  // PACKAGER_FILE_HTTP_H_
