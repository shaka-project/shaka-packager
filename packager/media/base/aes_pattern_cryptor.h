// Copyright 2016 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/base/aes_cryptor.h"

#include <memory>

#include "packager/base/macros.h"

namespace shaka {
namespace media {

/// Implements pattern-based encryption/decryption.
class AesPatternCryptor : public AesCryptor {
 public:
  /// Enumerator for controling encrytion/decryption mode for the last
  /// encryption/decrytion block(s).
  enum PatternEncryptionMode {
    /// Use kEncryptIfCryptByteBlockRemaining if the last blocks is exactly the
    /// same as the number of remaining bytes. IOW
    /// if (remaining_bytes == encryption_block_bytes)
    ///   encrypt(remaining_data)
    kEncryptIfCryptByteBlockRemaining,
    /// Use kSkipIfCryptByteBlockRemaining to not encrypt/decrypt the last
    /// clocks if it is exactly the same as the number of remaining bytes.
    /// if (remaining_bytes > encryption_block_bytes) {
    ///   // Since this is the last blocks, this is effectively the same
    ///   // condition as remaining_bytes != encryption_block_bytes.
    ///   encrypt().
    /// }
    /// Use this mode for HLS SAMPLE-AES.
    kSkipIfCryptByteBlockRemaining,
  };

  /// @param crypt_byte_block indicates number of encrypted blocks (16-byte) in
  ///        pattern based encryption.
  /// @param skip_byte_block indicates number of unencrypted blocks (16-byte)
  ///        in pattern based encryption.
  /// @param encryption_mode is used to determine the behavior for the last
  ///        block.
  /// @param constant_iv_flag indicates whether a constant iv is used,
  ///        kUseConstantIv means that the same iv is used for all Crypt calls
  ///        until iv is changed via SetIv; otherwise, iv can be incremented
  ///        (for counter mode) or chained (for cipher block chaining mode)
  ///        internally inside Crypt call, i.e. iv will be updated across Crypt
  ///        calls.
  /// @param cryptor points to an AesCryptor instance which performs the actual
  ///        encryption/decryption. Note that @a cryptor shall not use constant
  ///        iv.
  AesPatternCryptor(uint8_t crypt_byte_block,
                    uint8_t skip_byte_block,
                    PatternEncryptionMode encryption_mode,
                    ConstantIvFlag constant_iv_flag,
                    std::unique_ptr<AesCryptor> cryptor);
  ~AesPatternCryptor() override;

  /// @name AesCryptor implementation overrides.
  /// @{
  bool InitializeWithIv(const std::vector<uint8_t>& key,
                        const std::vector<uint8_t>& iv) override;
  /// @}

 private:
  bool CryptInternal(const uint8_t* text,
                     size_t text_size,
                     uint8_t* crypt_text,
                     size_t* crypt_text_size) override;
  void SetIvInternal() override;

  bool NeedEncrypt(size_t input_size, size_t target_data_size);

  uint8_t crypt_byte_block_;
  const uint8_t skip_byte_block_;
  const PatternEncryptionMode encryption_mode_;
  std::unique_ptr<AesCryptor> cryptor_;

  DISALLOW_COPY_AND_ASSIGN(AesPatternCryptor);
};

}  // namespace media
}  // namespace shaka
