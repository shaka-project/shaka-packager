// Copyright 2014 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_BASE_KEY_FETCHER_H_
#define PACKAGER_MEDIA_BASE_KEY_FETCHER_H_

#include <packager/macros/classes.h>
#include <packager/status.h>

namespace shaka {
namespace media {

/// Base class for fetching keys from the license service.
class KeyFetcher {
 public:
  KeyFetcher();
  virtual ~KeyFetcher();

  /// Fetch Keys from license service.
  /// |response| is owned by caller.
  /// @param service_address license service address.
  /// @param request JSON formatted request.
  /// @param response JSON formatted response. Owned by caller.
  /// @return OK on success.
  virtual Status FetchKeys(const std::string& service_address,
                           const std::string& request,
                           std::string* response) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(KeyFetcher);
};

}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_BASE_KEY_FETCHER_H_

