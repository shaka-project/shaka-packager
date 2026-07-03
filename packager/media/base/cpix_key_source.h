// Copyright 2026 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_BASE_CPIX_KEY_SOURCE_H_
#define PACKAGER_MEDIA_BASE_CPIX_KEY_SOURCE_H_

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <packager/crypto_params.h>
#include <packager/media/base/fourccs.h>
#include <packager/media/base/key_source.h>

namespace shaka {
namespace media {

/// A key source that uses keys from a CPIX (DASH-IF Content Protection
/// Information Exchange) document. Keys are mapped to streams through the
/// document's ContentKeyUsageRuleList: the `intendedTrackType` attribute is
/// matched against the DRM label of the stream (e.g. AUDIO, SD, HD, UHD1).
/// DRM signaling (PSSH) is taken from the document's DRMSystemList.
class CpixKeySource : public KeySource {
 public:
  ~CpixKeySource() override;

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

  /// Creates a new CpixKeySource from the given parameters. Returns null if
  /// the document cannot be read or is malformed.
  /// @param cpix_params contains parameters to setup the key source.
  /// @param protection_scheme is the protection scheme the content will be
  ///        encrypted with. It is validated against the document's
  ///        `commonEncryptionScheme` attributes if present.
  static std::unique_ptr<CpixKeySource> Create(
      const CpixEncryptionParams& cpix_params,
      FourCC protection_scheme);

 private:
  explicit CpixKeySource(EncryptionKeyMap&& encryption_key_map);
  CpixKeySource(const CpixKeySource&) = delete;
  CpixKeySource& operator=(const CpixKeySource&) = delete;

  EncryptionKeyMap encryption_key_map_;
};

}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_BASE_CPIX_KEY_SOURCE_H_
