// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd
//
/// NOTE: Inclusion of this module will cause curl_global_init and
///       curl_global_cleanup to be called at static initialization /
///       deinitialization time.

#ifndef PACKAGER_MEDIA_BASE_HTTP_KEY_FETCHER_H_
#define PACKAGER_MEDIA_BASE_HTTP_KEY_FETCHER_H_

#include "packager/base/compiler_specific.h"
#include "packager/media/base/key_fetcher.h"
#include "packager/status.h"

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

  /// Sets client certificate information for http requests.
  /// @param cert_file absolute path to the client certificate.
  /// @param private_key_file absolute path to the client certificate
  ///        private key file.
  /// @param private_key_password private key password.
  void SetClientCertInfo(const std::string& cert_file,
                         const std::string& private_key_file,
                         const std::string& private_key_password) {
    client_cert_file_ = cert_file;
    client_cert_private_key_file_ = private_key_file;
    client_cert_private_key_password_ = private_key_password;
  }
  /// Sets the Certifiate Authority file information for http requests.
  /// @param ca_file absolute path to the client certificate
  void SetCaFile(const std::string& ca_file) {
    ca_file_ = ca_file;
  }

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
  std::string ca_file_;
  std::string client_cert_file_;
  std::string client_cert_private_key_file_;
  std::string client_cert_private_key_password_;

  DISALLOW_COPY_AND_ASSIGN(HttpKeyFetcher);
};

}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_BASE_HTTP_KEY_FETCHER_H_
