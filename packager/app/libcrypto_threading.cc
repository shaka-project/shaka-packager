// Copyright 2014 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/app/libcrypto_threading.h"

#include <glog/logging.h>
#include <mbedtls/threading.h>
#include <memory>

namespace shaka {
namespace media {

LibcryptoThreading::LibcryptoThreading() {
  //    mbedtls_threading_set_alt();
}

LibcryptoThreading::~LibcryptoThreading() {
  //  mbedtls_threading_free_alt();
}

}  // namespace media
}  // namespace shaka
