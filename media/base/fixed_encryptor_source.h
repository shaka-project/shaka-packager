// Copyright (c) 2013 Google Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Defines a fixed encryptor source with keys provided by user.

#ifndef MEDIA_BASE_FIXED_ENCRYPTOR_SOURCE_H_
#define MEDIA_BASE_FIXED_ENCRYPTOR_SOURCE_H_

#include "media/base/encryptor_source.h"

namespace media {

class FixedEncryptorSource : public EncryptorSource {
 public:
  FixedEncryptorSource(const std::string& key_id_hex,
                       const std::string& key_hex,
                       const std::string& pssh_hex);
  virtual ~FixedEncryptorSource();

  // EncryptorSource implementation.
  virtual Status Initialize() OVERRIDE;

 private:
  std::string key_id_hex_;
  std::string key_hex_;
  std::string pssh_hex_;

  DISALLOW_COPY_AND_ASSIGN(FixedEncryptorSource);
};

}  // namespace media

#endif  // MEDIA_BASE_FIXED_ENCRYPTOR_SOURCE_H_
