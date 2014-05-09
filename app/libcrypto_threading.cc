// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "app/libcrypto_threading.h"

namespace media {

LibcryptoThreading::LibcryptoThreading() {}

LibcryptoThreading::~LibcryptoThreading() {
  TerminateLibcryptoThreading();
}

bool LibcryptoThreading::Initialize() {
  return InitLibcryptoThreading();
}

}  // namespace media
