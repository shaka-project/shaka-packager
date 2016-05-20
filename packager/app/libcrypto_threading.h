// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef APP_LIBCRYPTO_THREADING_H_
#define APP_LIBCRYPTO_THREADING_H_

#include "packager/base/macros.h"

namespace shaka {
namespace media {

/// Convenience class which initializes and terminates libcrypto threading.
class LibcryptoThreading {
 public:
  LibcryptoThreading();
  ~LibcryptoThreading();

 private:
  DISALLOW_COPY_AND_ASSIGN(LibcryptoThreading);
};

}  // namespace media
}  // namespace shaka

#endif  // APP_LIBCRYPTO_THREADING_H_
