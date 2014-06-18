// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef MEDIA_BASE_HTTP_FETCHER_H_
#define MEDIA_BASE_HTTP_FETCHER_H_

#include "base/compiler_specific.h"
#include "media/base/status.h"

namespace media {

/// Defines a generic http fetcher interface.
class HttpFetcher {
 public:
  HttpFetcher();
  virtual ~HttpFetcher();

  /// Fetch content using HTTP GET.
  /// @param url specifies the content URL.
  /// @param[out] response will contain the body of the http response on
  ///             success. It should not be NULL.
  /// @return OK on success.
  virtual Status Get(const std::string& url, std::string* response) = 0;

  /// Fetch content using HTTP POST.
  /// @param url specifies the content URL.
  /// @param[out] response will contain the body of the http response on
  ///             success. It should not be NULL.
  /// @return OK on success.
  virtual Status Post(const std::string& url,
                      const std::string& data,
                      std::string* response) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(HttpFetcher);
};

/// A simple HttpFetcher implementation.
/// This class is not fully thread safe. It can be used in multi-thread
/// environment once constructed, but it may not be safe to create a
/// SimpleHttpFetcher object when any other thread is running due to use of
/// curl_global_init.
class SimpleHttpFetcher : public HttpFetcher {
 public:
  /// Creates a fetcher with no timeout.
  SimpleHttpFetcher();
  /// Create a fetcher with timeout.
  /// @param timeout_in_seconds specifies the timeout in seconds.
  SimpleHttpFetcher(uint32 timeout_in_seconds);
  virtual ~SimpleHttpFetcher();

  /// @name HttpFetcher implementation overrides.
  /// @{
  virtual Status Get(const std::string& url, std::string* response) OVERRIDE;
  virtual Status Post(const std::string& url,
                      const std::string& data,
                      std::string* response) OVERRIDE;
  /// @}

 private:
  enum HttpMethod {
    GET,
    POST,
    PUT
  };

  // Internal implementation of HTTP functions, e.g. Get and Post.
  Status FetchInternal(HttpMethod method, const std::string& url,
                       const std::string& data, std::string* response);

  const uint32 timeout_in_seconds_;

  DISALLOW_COPY_AND_ASSIGN(SimpleHttpFetcher);
};

}  // namespace media

#endif  // MEDIA_BASE_HTTP_FETCHER_H_

