// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd
//
/// NOTE: Inclusion of this module will cause curl_global_init and
///       curl_global_cleanup to be called at static initialization /
///       deinitialization time.

#ifndef MEDIA_BASE_HTTP_KEY_FETCHER_H_
#define MEDIA_BASE_HTTP_KEY_FETCHER_H_

#include "packager/base/compiler_specific.h"
#include "packager/media/base/key_fetcher.h"
#include "packager/media/base/status.h"

namespace edash_packager {
namespace media {

/// A KeyFetcher implementation that retrieves keys over HTTP(s).
/// This class is not fully thread safe. It can be used in multi-thread
/// environment once constructed, but it may not be safe to create a
/// HttpKeyFetcher object when any other thread is running due to use of
/// curl_global_init.
class HttpKeyFetcher : public KeyFetcher {
 public:
  /// Creates a fetcher with no timeout.
  HttpKeyFetcher();
  /// Create a fetcher with timeout.
  /// @param timeout_in_seconds specifies the timeout in seconds.
  HttpKeyFetcher(uint32_t timeout_in_seconds);
  ~HttpKeyFetcher() override;

  /// @name KeyFetcher implementation overrides.
  Status FetchKeys(const std::string& url,
                   const std::string& request,
                   std::string* response) override;

  /// Fetch content using HTTP GET.
  /// @param url specifies the content URL.
  /// @param[out] response will contain the body of the http response on
  ///             success. It should not be NULL.
  /// @return OK on success.
  virtual Status Get(const std::string& url, std::string* response);

  /// Fetch content using HTTP POST.
  /// @param url specifies the content URL.
  /// @param[out] response will contain the body of the http response on
  ///             success. It should not be NULL.
  /// @return OK on success.
  virtual Status Post(const std::string& url,
                      const std::string& data,
                      std::string* response);

 private:
  enum HttpMethod {
    GET,
    POST,
    PUT
  };

  // Internal implementation of HTTP functions, e.g. Get and Post.
  Status FetchInternal(HttpMethod method, const std::string& url,
                       const std::string& data, std::string* response);

  const uint32_t timeout_in_seconds_;

  DISALLOW_COPY_AND_ASSIGN(HttpKeyFetcher);
};

}  // namespace media
}  // namespace edash_packager

#endif  // MEDIA_BASE_HTTP_KEY_FETCHER_H_
