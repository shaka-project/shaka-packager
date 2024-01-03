// Copyright 2016 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_BASE_RAW_KEY_SOURCE_H_
#define PACKAGER_MEDIA_BASE_RAW_KEY_SOURCE_H_

#include <memory>
#include <string>
#include <vector>

#include <packager/crypto_params.h>
#include <packager/media/base/key_source.h>

namespace shaka {
namespace media {

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
                            int32_t crypto_period_duration_in_seconds,
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
  RawKeySource(EncryptionKeyMap&& encryption_key_map);
  RawKeySource(const RawKeySource&) = delete;
  RawKeySource& operator=(const RawKeySource&) = delete;

  EncryptionKeyMap encryption_key_map_;
};

}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_BASE_RAW_KEY_SOURCE_H_
