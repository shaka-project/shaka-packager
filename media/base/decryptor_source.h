// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef MEDIA_BASE_DECRYPTOR_SOURCE_H_
#define MEDIA_BASE_DECRYPTOR_SOURCE_H_

#include "base/memory/scoped_ptr.h"
#include "media/base/container_names.h"
#include "media/base/status.h"

namespace media {

/// DecryptorSource is responsible for decryption key acquisition.
class DecryptorSource {
 public:
  DecryptorSource() {}
  virtual ~DecryptorSource() {}

  /// NeedKey event handler.
  /// @param container indicates the container format.
  /// @param init_data specifies container dependent initialization data that
  ///        is used to initialize the decryption key.
  /// @return OK on success, an adequate status on error.
  virtual Status OnNeedKey(MediaContainerName container,
                           const std::string& init_data) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(DecryptorSource);
};

}  // namespace media

#endif  // MEDIA_BASE_DECRYPTOR_SOURCE_H_
