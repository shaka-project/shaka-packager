// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef MEDIA_BASE_KEY_SOURCE_H_
#define MEDIA_BASE_KEY_SOURCE_H_

#include <string>
#include <vector>

#include "packager/media/base/protection_system_specific_info.h"
#include "packager/media/base/status.h"

namespace shaka {
namespace media {

struct EncryptionKey {
  EncryptionKey();
  ~EncryptionKey();

  std::vector<ProtectionSystemSpecificInfo> key_system_info;
  std::vector<uint8_t> key_id;
  std::vector<uint8_t> key;
  std::vector<uint8_t> iv;
};

/// KeySource is responsible for encryption key acquisition.
class KeySource {
 public:
  enum TrackType {
    TRACK_TYPE_UNKNOWN = 0,
    TRACK_TYPE_SD = 1,
    TRACK_TYPE_HD = 2,
    TRACK_TYPE_UHD1 = 3,
    TRACK_TYPE_UHD2 = 4,
    TRACK_TYPE_AUDIO = 5,
    TRACK_TYPE_UNSPECIFIED = 6,
    NUM_VALID_TRACK_TYPES = 6
  };

  KeySource();
  virtual ~KeySource();

  /// Fetch keys for CENC from the key server.
  /// @param pssh_box The entire PSSH box for the content to be decrypted
  /// @return OK on success, an error status otherwise.
  virtual Status FetchKeys(const std::vector<uint8_t>& pssh_box) = 0;

  /// Fetch keys for CENC from the key server.
  /// @param key_ids the key IDs for the keys to fetch from the server.
  /// @return OK on success, an error status otherwise.
  virtual Status FetchKeys(
      const std::vector<std::vector<uint8_t>>& key_ids) = 0;

  /// Fetch keys for WVM decryption from the key server.
  /// @param asset_id is the Widevine Classic asset ID for the content to be
  /// decrypted.
  /// @return OK on success, an error status otherwise.
  virtual Status FetchKeys(uint32_t asset_id) = 0;

  /// Get encryption key of the specified track type.
  /// @param track_type is the type of track for which retrieving the key.
  /// @param key is a pointer to the EncryptionKey which will hold the retrieved
  ///        key. Owner retains ownership, and may not be NULL.
  /// @return OK on success, an error status otherwise.
  virtual Status GetKey(TrackType track_type, EncryptionKey* key) = 0;

  /// Get the encryption key specified by the CENC key ID.
  /// @param key_id is the unique identifier for the key being retreived.
  /// @param key is a pointer to the EncryptionKey which will hold the retrieved
  ///        key. Owner retains ownership, and may not be NULL.
  /// @return OK on success, or an error status otherwise.
  virtual Status GetKey(const std::vector<uint8_t>& key_id,
                        EncryptionKey* key) = 0;

  /// Get encryption key of the specified track type at the specified index.
  /// @param crypto_period_index is the sequence number of the key rotation
  ///        period for which the key is being retrieved.
  /// @param track_type is the type of track for which retrieving the key.
  /// @param key is a pointer to the EncryptionKey which will hold the retrieved
  ///        key. Owner retains ownership, and may not be NULL.
  /// @return OK on success, an error status otherwise.
  virtual Status GetCryptoPeriodKey(uint32_t crypto_period_index,
                                    TrackType track_type,
                                    EncryptionKey* key) = 0;

  /// Convert string representation of track type to enum representation.
  static TrackType GetTrackTypeFromString(const std::string& track_type_string);

  /// Convert TrackType to string.
  static std::string TrackTypeToString(TrackType track_type);

 private:
  DISALLOW_COPY_AND_ASSIGN(KeySource);
};

}  // namespace media
}  // namespace shaka

#endif  // MEDIA_BASE_KEY_SOURCE_H_
