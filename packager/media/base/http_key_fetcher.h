// Copyright 2014 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_BASE_HTTP_KEY_FETCHER_H_
#define PACKAGER_MEDIA_BASE_HTTP_KEY_FETCHER_H_

#include <string>

#include <packager/file/http_file.h>
#include <packager/macros/classes.h>
#include <packager/media/base/key_fetcher.h>
#include <packager/status.h>

namespace shaka {
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
  HttpKeyFetcher(int32_t timeout_in_seconds);
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
  Status FetchInternal(HttpMethod method, const std::string& url,
                       const std::string& data, std::string* response);

  const int32_t timeout_in_seconds_;

  DISALLOW_COPY_AND_ASSIGN(HttpKeyFetcher);
};

}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_BASE_HTTP_KEY_FETCHER_H_
