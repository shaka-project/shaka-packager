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

/// Fetches CPIX documents over HTTP(S). Replaceable for testing.
class CpixFetcher {
 public:
  virtual ~CpixFetcher() = default;

  /// Fetches the CPIX document at @a url. If @a request_body is non-empty
  /// it is sent with POST and the response body is the document; otherwise
  /// the document is fetched with GET.
  /// @param headers contains extra HTTP headers in "Name: value" form.
  /// @param[out] response receives the response body. Should not be NULL.
  virtual Status Fetch(const std::string& url,
                       const std::string& request_body,
                       const std::vector<std::string>& headers,
                       std::string* response) = 0;
};

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
  /// the document cannot be read or fetched, or is malformed.
  /// @param cpix_params contains parameters to setup the key source.
  /// @param protection_scheme is the protection scheme the content will be
  ///        encrypted with. It is validated against the document's
  ///        `commonEncryptionScheme` attributes if present.
  static std::unique_ptr<CpixKeySource> Create(
      const CpixEncryptionParams& cpix_params,
      FourCC protection_scheme);

  /// Same as above, with an injected document fetcher. Should be used for
  /// testing only.
  static std::unique_ptr<CpixKeySource> CreateWithFetcher(
      const CpixEncryptionParams& cpix_params,
      FourCC protection_scheme,
      CpixFetcher* fetcher);

  /// Creates a new CpixKeySource for decryption. Keys are looked up by key
  /// ID, so the document's usage rules and `commonEncryptionScheme` binding
  /// are ignored. Returns null if the document cannot be read or fetched, or
  /// is malformed.
  /// @param cpix_params contains parameters to setup the key source.
  static std::unique_ptr<CpixKeySource> CreateForDecryption(
      const CpixEncryptionParams& cpix_params);

 private:
  explicit CpixKeySource(EncryptionKeyMap&& encryption_key_map);
  CpixKeySource(const CpixKeySource&) = delete;
  CpixKeySource& operator=(const CpixKeySource&) = delete;

  EncryptionKeyMap encryption_key_map_;
};

}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_BASE_CPIX_KEY_SOURCE_H_
