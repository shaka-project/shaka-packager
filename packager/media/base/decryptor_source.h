// Copyright 2016 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef MEDIA_BASE_DECRYPTOR_SOURCE_H_
#define MEDIA_BASE_DECRYPTOR_SOURCE_H_

#include <map>
#include <memory>
#include <vector>

#include "packager/media/base/aes_decryptor.h"
#include "packager/media/base/decrypt_config.h"
#include "packager/media/base/key_source.h"

namespace shaka {
namespace media {

/// DecryptorSource wraps KeySource and is responsible for decryptor management.
class DecryptorSource {
 public:
  explicit DecryptorSource(KeySource* key_source);
  ~DecryptorSource();

  bool DecryptSampleBuffer(const DecryptConfig* decrypt_config,
                           uint8_t* buffer,
                           size_t buffer_size);

 private:
  KeySource* key_source_;
  std::map<std::vector<uint8_t>, std::unique_ptr<AesCryptor>> decryptor_map_;

  DISALLOW_COPY_AND_ASSIGN(DecryptorSource);
};

}  // namespace media
}  // namespace shaka

#endif  // MEDIA_BASE_DECRYPTOR_SOURCE_H_
