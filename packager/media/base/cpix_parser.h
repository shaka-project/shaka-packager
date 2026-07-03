// Copyright 2026 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_BASE_CPIX_PARSER_H_
#define PACKAGER_MEDIA_BASE_CPIX_PARSER_H_

#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include <packager/status.h>

namespace shaka {
namespace media {

/// A content key from a CPIX ContentKeyList.
struct CpixContentKey {
  /// 16-byte CENC key ID, from the `kid` attribute.
  std::vector<uint8_t> key_id;
  /// Clear key value, from `Data/Secret/PlainValue`.
  std::vector<uint8_t> key;
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

/// In-memory representation of the parts of a CPIX 2.3 document
/// (https://dashif.org/docs/CPIX2.3/HTML/Index.html) needed for packaging.
struct CpixDocument {
  std::vector<CpixContentKey> content_keys;
  std::vector<CpixDrmSystem> drm_systems;
  std::vector<CpixUsageRule> usage_rules;
};

/// Parses a CPIX document.
/// @param xml contains the CPIX document text.
/// @param document is a pointer to the parsed document. Should not be NULL.
/// @return OK on success, an error status otherwise. Encrypted documents
///         (encrypted content key values) are not supported and result in an
///         error.
Status ParseCpixDocument(const std::string& xml, CpixDocument* document);

}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_BASE_CPIX_PARSER_H_
