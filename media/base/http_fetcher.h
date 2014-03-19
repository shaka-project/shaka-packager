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

class HttpFetcher {
 public:
  HttpFetcher();
  virtual ~HttpFetcher();

  // Fetch |response| from |url| using HTTP GET.
  // |response| should not be NULL, will contain the body of the http response
  // on success.
  // Return OK on success.
  virtual Status Get(const std::string& url, std::string* response) = 0;

  // Fetch |response| from |url| using HTTP POST.
  // |response| should not be NULL, will contain the body of the http response
  // on success.
  // Return OK on success.
  virtual Status Post(const std::string& url,
                      const std::string& data,
                      std::string* response) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(HttpFetcher);
};

// A simple HttpFetcher implementation using happyhttp.
class SimpleHttpFetcher : public HttpFetcher {
 public:
  // TODO: Add timeout support.
  SimpleHttpFetcher();
  virtual ~SimpleHttpFetcher();

  // HttpFetcher implementation overrides.
  virtual Status Get(const std::string& url, std::string* response) OVERRIDE;
  virtual Status Post(const std::string& url,
                      const std::string& data,
                      std::string* response) OVERRIDE;

 private:
  // Internal implementation of HTTP functions, e.g. Get and Post.
  Status FetchInternal(const std::string& method, const std::string& url,
                       const std::string& data, std::string* response);

#ifdef WIN32
  // Track whether WSAStartup executes successfully.
  bool wsa_startup_succeeded_;
#endif

  DISALLOW_COPY_AND_ASSIGN(SimpleHttpFetcher);
};

}  // namespace media

#endif  // MEDIA_BASE_HTTP_FETCHER_H_

