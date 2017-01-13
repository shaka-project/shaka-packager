// Copyright 2016 Inside Secure Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef MEDIA_BASE_PLAYREADY_SOURCE_H_
#define MEDIA_BASE_PLAYREADY_SOURCE_H_

#include <memory>
#include <string>
#include <vector>

#include "packager/media/base/key_source.h"

namespace shaka {
namespace media {

// Playready system id.
// https://goo.gl/kUv2Xd
const uint8_t kPlayreadySystemId[] = {0x9a, 0x04, 0xf0, 0x79, 0x98, 0x40,
                                      0x42, 0x86, 0xab, 0x92, 0xe6, 0x5b,
                                      0xe0, 0x88, 0x5f, 0x95};

/// A key source that uses fixed keys for encryption.
class PlayReadyKeySource : public KeySource {
 public:
  ~PlayReadyKeySource() override;

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

  /// Creates a new PlayReadyKeySource from the given hex strings.  Returns null
  /// if the strings are invalid.
  /// @param key_id_hex is the key id in hex string.
  /// @param key_hex is the key in hex string.
  /// @param pssh_boxes_hex is the pssh_boxes in hex string.
  /// @param iv_hex is the IV in hex string. If not specified, a randomly
  ///        generated IV with the default length will be used.
  /// Note: GetKey on the created key source will always return the same key
  ///       for all track types.
  static std::unique_ptr<PlayReadyKeySource> CreateFromHexStrings(
      const std::string& key_id_hex,
      const std::string& key_hex,
      const std::string& iv_hex,
      const std::string& la_url,
      const std::string& lui_url,
      bool include_empty_license_store);

 protected:
  // Allow default constructor for mock key sources.
  PlayReadyKeySource();

 private:
  explicit PlayReadyKeySource(std::unique_ptr<EncryptionKey> key);

  std::unique_ptr<EncryptionKey> encryption_key_;

  DISALLOW_COPY_AND_ASSIGN(PlayReadyKeySource);
};

}  // namespace media
}  // namespace shaka

#endif  // MEDIA_BASE_PLAYREADY_SOURCE_H_
