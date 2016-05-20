// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "packager/media/base/network_util.h"

namespace shaka {
namespace media {

uint32_t ntohlFromBuffer(const unsigned char* buf) {
  return (static_cast<uint32_t>(buf[0]) << 24) |
         (static_cast<uint32_t>(buf[1]) << 16) |
         (static_cast<uint32_t>(buf[2]) << 8) | (static_cast<uint32_t>(buf[3]));
}

uint16_t ntohsFromBuffer(const unsigned char* buf) {
  return (static_cast<uint16_t>(buf[0]) << 8) | (static_cast<uint16_t>(buf[1]));
}

uint64_t ntohllFromBuffer(const unsigned char* buf) {
  return (static_cast<uint64_t>(buf[0]) << 56) |
         (static_cast<uint64_t>(buf[1]) << 48) |
         (static_cast<uint64_t>(buf[2]) << 40) |
         (static_cast<uint64_t>(buf[3]) << 32) |
         (static_cast<uint64_t>(buf[4]) << 24) |
         (static_cast<uint64_t>(buf[5]) << 16) |
         (static_cast<uint64_t>(buf[6]) << 8) | (static_cast<uint64_t>(buf[7]));
}

}  // namespace media
}  // namespace shaka

