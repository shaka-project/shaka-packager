// Copyright 2015 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/file/udp_file.h"

#include "packager/base/logging.h"

namespace edash_packager {
namespace media {

UdpFile::UdpFile(const char* file_name) : File(file_name), socket_(0) {
}

UdpFile::~UdpFile() {}

bool UdpFile::Close() {
  NOTIMPLEMENTED();
  delete this;
  return false;
}

int64_t UdpFile::Read(void* buffer, uint64_t length) {
  NOTIMPLEMENTED();
  return -1;
}

int64_t UdpFile::Write(const void* buffer, uint64_t length) {
  NOTIMPLEMENTED();
  return -1;
}

int64_t UdpFile::Size() {
  NOTIMPLEMENTED();
  return -1;
}

bool UdpFile::Flush() {
  NOTIMPLEMENTED();
  return false;
}

bool UdpFile::Open() {
  NOTIMPLEMENTED();
  return false;
}

}  // namespace media
}  // namespace edash_packager
