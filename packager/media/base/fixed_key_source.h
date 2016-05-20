// Copyright 2016 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef MEDIA_BASE_FIXED_KEY_SOURCE_H_
#define MEDIA_BASE_FIXED_KEY_SOURCE_H_

#include <string>
#include <vector>

#include "packager/base/memory/scoped_ptr.h"
#include "packager/media/base/key_source.h"

namespace shaka {
namespace media {

// Common SystemID defined by EME, which requires Key System implementations
// supporting ISO Common Encryption to support this SystemID and format.
// http://goo.gl/PHZDAF
const uint8_t kCommonSystemId[] = {0x10, 0x77, 0xef, 0xec, 0xc0, 0xb2,
                                   0x4d, 0x02, 0xac, 0xe3, 0x3c, 0x1e,
                                   0x52, 0xe2, 0xfb, 0x4b};

/// A key source that uses fixed keys for encryption.
class FixedKeySource : public KeySource {
 public:
  ~FixedKeySource() override;

  /// @name KeySource implementation overrides.
  /// @{
  Status FetchKeys(const std::vector<uint8_t>& pssh_box) override;
  Status FetchKeys(const std::vector<std::vector<uint8_t>>& key_ids) override;
  Status FetchKeys(uint32_t asset_id) override;

  Status GetKey(TrackType track_type, EncryptionKey* key) override;
  Status GetKey(const std::vector<uint8_t>& key_id,
                EncryptionKey* key) override;
  Status GetCryptoPeriodKey(uint32_t crypto_period_index,
                            TrackType track_type,
                            EncryptionKey* key) override;
  /// @}

  /// Creates a new FixedKeySource from the given hex strings.  Returns null
  /// if the strings are invalid.
  /// @param key_id_hex is the key id in hex string.
  /// @param key_hex is the key in hex string.
  /// @param pssh_boxes_hex is the pssh_boxes in hex string.
  /// @param iv_hex is the IV in hex string. If not specified, a randomly
  ///        generated IV with the default length will be used.
  /// Note: GetKey on the created key source will always return the same key
  ///       for all track types.
  static scoped_ptr<FixedKeySource> CreateFromHexStrings(
      const std::string& key_id_hex,
      const std::string& key_hex,
      const std::string& pssh_boxes_hex,
      const std::string& iv_hex);

 protected:
  // Allow default constructor for mock key sources.
  FixedKeySource();

 private:
  explicit FixedKeySource(scoped_ptr<EncryptionKey> key);

  scoped_ptr<EncryptionKey> encryption_key_;

  DISALLOW_COPY_AND_ASSIGN(FixedKeySource);
};

}  // namespace media
}  // namespace shaka

#endif  // MEDIA_BASE_FIXED_KEY_SOURCE_H_
