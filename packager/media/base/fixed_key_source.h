// Copyright 2016 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef MEDIA_BASE_FIXED_KEY_SOURCE_H_
#define MEDIA_BASE_FIXED_KEY_SOURCE_H_

#include <memory>
#include <string>
#include <vector>

#include "packager/media/base/key_source.h"

namespace shaka {
namespace media {

// Common SystemID defined by EME, which requires Key System implementations
// supporting ISO Common Encryption to support this SystemID and format.
// https://goo.gl/kUv2Xd
const uint8_t kCommonSystemId[] = {0x10, 0x77, 0xef, 0xec, 0xc0, 0xb2,
                                   0x4d, 0x02, 0xac, 0xe3, 0x3c, 0x1e,
                                   0x52, 0xe2, 0xfb, 0x4b};

/// A key source that uses fixed keys for encryption.
class FixedKeySource : public KeySource {
 public:
  ~FixedKeySource() override;

  /// @name KeySource implementation overrides.
  /// @{
  Status FetchKeys(EmeInitDataType init_data_type,
                   const std::vector<uint8_t>& init_data) override;
  Status GetKey(const std::string& stream_label, EncryptionKey* key) override;
  Status GetKey(const std::vector<uint8_t>& key_id,
                EncryptionKey* key) override;
  Status GetCryptoPeriodKey(uint32_t crypto_period_index,
                            const std::string& stream_label,
                            EncryptionKey* key) override;
  /// @}

  /// Creates a new FixedKeySource from the given data.  Returns null
  /// if the strings are invalid.
  /// @param key_id is the key identifier. Must be 16 bytes.
  /// @param key is the encryption / decryption key. Must be 16 bytes.
  /// @param pssh_boxes is the concatenated pssh boxes.
  /// @param iv is the initialization vector. If not specified, a randomly
  ///        generated IV with the default length will be used.
  /// Note: GetKey on the created key source will always return the same key for
  /// all track types.
  static std::unique_ptr<FixedKeySource> Create(
      const std::vector<uint8_t>& key_id,
      const std::vector<uint8_t>& key,
      const std::vector<uint8_t>& pssh_boxes,
      const std::vector<uint8_t>& iv);

 protected:
  // Allow default constructor for mock key sources.
  FixedKeySource();

 private:
  explicit FixedKeySource(std::unique_ptr<EncryptionKey> key);

  std::unique_ptr<EncryptionKey> encryption_key_;

  DISALLOW_COPY_AND_ASSIGN(FixedKeySource);
};

}  // namespace media
}  // namespace shaka

#endif  // MEDIA_BASE_FIXED_KEY_SOURCE_H_
