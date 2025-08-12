// Copyright 2018 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_BASE_MOCK_AES_CRYPTOR_H_
#define PACKAGER_MEDIA_BASE_MOCK_AES_CRYPTOR_H_

#include <cstdint>

#include <packager/media/base/aes_cryptor.h>

namespace shaka {
namespace media {

class MockAesCryptor : public AesCryptor {
 public:
  MockAesCryptor() : AesCryptor(kDontUseConstantIv) {}

  MOCK_METHOD2(InitializeWithIv,
               bool(const std::vector<uint8_t>& key,
                    const std::vector<uint8_t>& iv));
  MOCK_METHOD4(CryptInternal,
               bool(const uint8_t* text,
                    size_t text_size,
                    uint8_t* crypt_text,
                    size_t* crypt_text_size));
  MOCK_METHOD0(SetIvInternal, void());
};

}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_BASE_MOCK_AES_CRYPTOR_H_
