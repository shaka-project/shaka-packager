// Copyright (c) 2013 Google Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/encryptor_source.h"

namespace {
const char kWidevineSystemId[] = {0xed, 0xef, 0x8b, 0xa9, 0x79, 0xd6,
                                  0x4a, 0xce, 0xa3, 0xc8, 0x27, 0xdc,
                                  0xd5, 0x1d, 0x21, 0xed};
}  // namespace

namespace media {

EncryptorSource::EncryptorSource()
    : key_system_id_(kWidevineSystemId,
                     kWidevineSystemId + arraysize(kWidevineSystemId)),
      clear_milliseconds_(0) {}

EncryptorSource::~EncryptorSource() {}

}  // namespace media
