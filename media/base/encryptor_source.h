// Copyright (c) 2013 Google Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// EncryptorSource is responsible for encryption key acquisition.

#ifndef MEDIA_BASE_ENCRYPTOR_SOURCE_H_
#define MEDIA_BASE_ENCRYPTOR_SOURCE_H_

#include "base/memory/scoped_ptr.h"
#include "media/base/container_names.h"
#include "media/base/status.h"

namespace media {

class EncryptorSource {
 public:
  EncryptorSource() {}
  virtual ~EncryptorSource() {}

  virtual Status Init() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(EncryptorSource);
};

}

#endif  // MEDIA_BASE_ENCRYPTOR_SOURCE_H_
