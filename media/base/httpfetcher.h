// Copyright (c) 2013 Google Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_HTTPFETCHER_H_
#define MEDIA_BASE_HTTPFETCHER_H_

#include "media/base/status.h"

namespace media {

// A simple HTTP fetcher implementation using happyhttp.
class HTTPFetcher {
 public:
  // TODO(kqyang): Add timeout support.
  HTTPFetcher();
  ~HTTPFetcher();

  // Fetch |response| from |url| using HTTP GET.
  // |response| should not be NULL, will contain the body of the http response
  // on success.
  // Return OK on success.
  Status Get(const std::string& url, std::string* response);

  // Fetch |response| from |url| using HTTP POST.
  // |response| should not be NULL, will contain the body of the http response
  // on success.
  // Return OK on success.
  Status Post(const std::string& url, const std::string& data,
              std::string* response);

 private:
  // Internal implementation of HTTP functions, e.g. Get and Post.
  Status FetchInternal(const std::string& method, const std::string& url,
                       const std::string& data, std::string* response);

#ifdef WIN32
  // Track whether WSAStartup executes successfully.
  bool wsa_startup_succeeded_;
#endif

  DISALLOW_COPY_AND_ASSIGN(HTTPFetcher);
};

}  // namespace media

#endif  // MEDIA_BASE_HTTPFETCHER_H_

