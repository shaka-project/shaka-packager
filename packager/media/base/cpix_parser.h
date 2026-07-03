// Copyright 2026 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_BASE_CPIX_PARSER_H_
#define PACKAGER_MEDIA_BASE_CPIX_PARSER_H_

#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <vector>

#include <packager/status.h>

namespace shaka {
namespace media {

/// An encrypted value (`pskc:EncryptedValue`, optionally with a sibling
/// `pskc:ValueMAC`).
struct CpixEncryptedValue {
  /// XML Encryption algorithm URI from `EncryptionMethod@Algorithm`. Empty
  /// if not present.
  std::string algorithm;
  /// Ciphertext, from `CipherData/CipherValue`.
  std::vector<uint8_t> cipher_value;
  /// Optional MAC over the ciphertext, from the sibling `ValueMAC` element.
  /// Empty if not present.
  std::vector<uint8_t> value_mac;
};

/// A content key from a CPIX ContentKeyList.
struct CpixContentKey {
  /// 16-byte CENC key ID, from the `kid` attribute.
  std::vector<uint8_t> key_id;
  /// Clear key value, from `Data/Secret/PlainValue`. Empty if the key is
  /// encrypted (see `encrypted_key`).
  std::vector<uint8_t> key;
  /// Set when `Data/Secret` contains an `EncryptedValue` instead of a
  /// `PlainValue`. The key must then be decrypted with the document key
  /// before use.
  std::optional<CpixEncryptedValue> encrypted_key;
  /// Optional IV, from the `explicitIV` attribute. Empty if not present.
  std::vector<uint8_t> iv;
  /// Optional `commonEncryptionScheme` attribute, e.g. "cenc" or "cbcs".
  /// Empty if not present.
  std::string common_encryption_scheme;
};

/// DRM system signaling for one (key ID, DRM system) pair from a CPIX
/// DRMSystemList.
struct CpixDrmSystem {
  /// 16-byte CENC key ID, from the `kid` attribute.
  std::vector<uint8_t> key_id;
  /// 16-byte DRM system ID, from the `systemId` attribute.
  std::vector<uint8_t> system_id;
  /// Full PSSH box(es), from the `PSSH` element. May be empty for DRM
  /// systems that do not use PSSH, e.g. FairPlay.
  std::vector<uint8_t> pssh;
};

/// A video filter from a usage rule, restricting the rule to video streams
/// within a pixel count (width x height) range. Both bounds are inclusive.
struct CpixVideoFilter {
  /// From the optional `minPixels` attribute. 0 if not present.
  int64_t min_pixels = 0;
  /// From the optional `maxPixels` attribute. Unbounded if not present.
  int64_t max_pixels = std::numeric_limits<int64_t>::max();
};

/// A usage rule from a CPIX ContentKeyUsageRuleList, mapping a key to
/// streams by intended track type (DRM label) and/or filters.
struct CpixUsageRule {
  /// 16-byte CENC key ID, from the `kid` attribute.
  std::vector<uint8_t> key_id;
  /// The optional `intendedTrackType` attribute, e.g. "SD", "HD", "AUDIO".
  /// Empty if not present.
  std::string intended_track_type;
  /// True if the rule contains an `AudioFilter` element, restricting the
  /// rule to audio streams.
  bool has_audio_filter = false;
  /// `VideoFilter` elements, restricting the rule to video streams. Multiple
  /// filters form a union of their pixel ranges.
  std::vector<CpixVideoFilter> video_filters;
};

/// Delivery data for one document recipient, from a CPIX DeliveryDataList.
/// Carries the document key (encrypting the content key values) and the MAC
/// key, both encrypted to the recipient.
struct CpixDeliveryData {
  /// Encrypted document key, from `DocumentKey/Data/Secret/EncryptedValue`.
  CpixEncryptedValue document_key;
  /// MAC algorithm URI from `MACMethod@Algorithm`. Empty if the document has
  /// no MACMethod.
  std::string mac_algorithm;
  /// Encrypted MAC key, from `MACMethod/Key/EncryptedValue`. Only valid if
  /// `mac_algorithm` is non-empty.
  CpixEncryptedValue mac_key;
};

/// In-memory representation of the parts of a CPIX 2.3 document
/// (https://dashif.org/CPIX/) needed for packaging.
struct CpixDocument {
  std::vector<CpixContentKey> content_keys;
  std::vector<CpixDrmSystem> drm_systems;
  std::vector<CpixUsageRule> usage_rules;
  std::vector<CpixDeliveryData> delivery_data;
};

/// Parses a CPIX document. Encrypted content key values are parsed into
/// `CpixContentKey::encrypted_key` but not decrypted; decryption is the
/// caller's responsibility.
/// @param xml contains the CPIX document text.
/// @param document is a pointer to the parsed document. Should not be NULL.
/// @return OK on success, an error status otherwise.
Status ParseCpixDocument(const std::string& xml, CpixDocument* document);

}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_BASE_CPIX_PARSER_H_
