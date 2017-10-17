// Copyright 2016 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_BASE_RAW_KEY_SOURCE_H_
#define PACKAGER_MEDIA_BASE_RAW_KEY_SOURCE_H_

#include <memory>
#include <string>
#include <vector>

#include "packager/media/base/key_source.h"
#include "packager/media/public/crypto_params.h"

namespace shaka {
namespace media {

// Common SystemID defined by EME, which requires Key System implementations
// supporting ISO Common Encryption to support this SystemID and format.
// https://goo.gl/kUv2Xd
const uint8_t kCommonSystemId[] = {0x10, 0x77, 0xef, 0xec, 0xc0, 0xb2,
                                   0x4d, 0x02, 0xac, 0xe3, 0x3c, 0x1e,
                                   0x52, 0xe2, 0xfb, 0x4b};

// Unofficial fairplay system id extracted from
// https://forums.developer.apple.com/thread/6185.
const uint8_t kFairplaySystemId[] = {0x29, 0x70, 0x1F, 0xE4, 0x3C, 0xC7,
                                     0x4A, 0x34, 0x8C, 0x5B, 0xAE, 0x90,
                                     0xC7, 0x43, 0x9A, 0x47};

/// A key source that uses raw keys for encryption.
class RawKeySource : public KeySource {
 public:
  ~RawKeySource() override;

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

  /// Creates a new RawKeySource from the given data.  Returns null
  /// if the parameter is malformed.
  /// @param raw_key contains parameters to setup the key source.
  static std::unique_ptr<RawKeySource> Create(const RawKeyParams& raw_key);

 protected:
  // Allow default constructor for mock key sources.
  RawKeySource();

 private:
  typedef std::map<std::string, std::unique_ptr<EncryptionKey>>
      EncryptionKeyMap;
  explicit RawKeySource(EncryptionKeyMap&& encryption_key_map);
  RawKeySource(const RawKeySource&) = delete;
  RawKeySource& operator=(const RawKeySource&) = delete;

  EncryptionKeyMap encryption_key_map_;
};

}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_BASE_RAW_KEY_SOURCE_H_
