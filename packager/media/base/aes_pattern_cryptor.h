// Copyright 2016 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/base/aes_cryptor.h"

#include "packager/base/macros.h"
#include "packager/base/memory/scoped_ptr.h"

namespace edash_packager {
namespace media {

/// Implements pattern-based encryption/decryption.
class AesPatternCryptor : public AesCryptor {
 public:
  enum ConstantIvFlag {
    kUseConstantIv,
    kDontUseConstantIv,
  };

  /// @param crypt_byte_block indicates number of encrypted blocks (16-byte) in
  ///        pattern based encryption.
  /// @param skip_byte_block indicates number of unencrypted blocks (16-byte)
  ///        in pattern based encryption.
  /// @param constant_iv_flag indicates whether a constant iv is used,
  ///        kUseConstantIv means that the same iv is used for all Crypt calls
  ///        until iv is changed via SetIv; otherwise, iv can be incremented
  ///        (for counter mode) or chained (for cipher block chaining mode)
  ///        internally inside Crypt call, i.e. iv will be updated across Crypt
  ///        calls.
  /// @param cryptor points to an AesCryptor instance which performs the actual
  ///        encryption/decryption.
  AesPatternCryptor(uint8_t crypt_byte_block,
                    uint8_t skip_byte_block,
                    ConstantIvFlag constant_iv_flag,
                    scoped_ptr<AesCryptor> cryptor);
  ~AesPatternCryptor() override;

  /// @name AesCryptor implementation overrides.
  /// @{
  bool InitializeWithIv(const std::vector<uint8_t>& key,
                        const std::vector<uint8_t>& iv) override;
  bool SetIv(const std::vector<uint8_t>& iv) override;
  void UpdateIv() override;
  /// @}

 protected:
  bool CryptInternal(const uint8_t* text,
                     size_t text_size,
                     uint8_t* crypt_text,
                     size_t* crypt_text_size) override;

 private:
  const uint8_t crypt_byte_block_;
  const uint8_t skip_byte_block_;
  const ConstantIvFlag constant_iv_flag_;
  scoped_ptr<AesCryptor> cryptor_;
  std::vector<uint8_t> iv_;

  DISALLOW_COPY_AND_ASSIGN(AesPatternCryptor);
};

}  // namespace media
}  // namespace edash_packager
