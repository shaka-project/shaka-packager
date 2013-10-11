// Copyright (c) 2013 Google Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// DecryptorSource is responsible for decryption key acquisition.

#ifndef MEDIA_BASE_DECRYPTOR_SOURCE_H_
#define MEDIA_BASE_DECRYPTOR_SOURCE_H_

#include "base/memory/scoped_ptr.h"
#include "media/base/container_names.h"
#include "media/base/status.h"

namespace media {

class DecryptorSource {
 public:
  DecryptorSource() {}
  virtual ~DecryptorSource() {}

  virtual Status OnNeedKey(MediaContainerName container,
                           const std::string& init_data) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(DecryptorSource);
};

}

#endif  // MEDIA_BASE_DECRYPTOR_SOURCE_H_
