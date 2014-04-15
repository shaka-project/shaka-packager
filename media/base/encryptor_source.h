// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef MEDIA_BASE_ENCRYPTOR_SOURCE_H_
#define MEDIA_BASE_ENCRYPTOR_SOURCE_H_

#include <vector>

#include "base/memory/scoped_ptr.h"
#include "media/base/status.h"

namespace media {

struct EncryptionKey {
  EncryptionKey();
  ~EncryptionKey();

  std::vector<uint8> key_id;
  std::vector<uint8> key;
  std::vector<uint8> pssh;
  std::vector<uint8> iv;
};

/// EncryptorSource is responsible for encryption key acquisition.
class EncryptorSource {
 public:
  enum TrackType {
    TRACK_TYPE_UNKNOWN = 0,
    TRACK_TYPE_SD = 1,
    TRACK_TYPE_HD = 2,
    TRACK_TYPE_AUDIO = 3,
    NUM_VALID_TRACK_TYPES = 3
  };

  virtual ~EncryptorSource();

  /// Get encryption key of the specified track type.
  /// @return OK on success, an error status otherwise.
  virtual Status GetKey(TrackType track_type, EncryptionKey* key);

  /// Create EncryptorSource object from hex strings.
  /// @param key_id_hex is the key id in hex string.
  /// @param key_hex is the key in hex string.
  /// @param pssh_data_hex is the pssh_data in hex string.
  /// @param iv_hex is the IV in hex string. If not specified, a randomly
  ///        generated IV with the default length will be used.
  /// Note: GetKey on the created key source will always return the same key
  ///       for all track types.
  static scoped_ptr<EncryptorSource> CreateFromHexStrings(
      const std::string& key_id_hex,
      const std::string& key_hex,
      const std::string& pssh_data_hex,
      const std::string& iv_hex);

  /// Convert string representation of track type to enum representation.
  static TrackType GetTrackTypeFromString(const std::string& track_type_string);

  /// Convert TrackType to string.
  static std::string TrackTypeToString(TrackType track_type);

 protected:
  EncryptorSource();

  /// @return the raw bytes of the pssh box with system ID and box header
  ///         included.
  static std::vector<uint8> PsshBoxFromPsshData(
      const std::vector<uint8>& pssh_data);

 private:
  explicit EncryptorSource(scoped_ptr<EncryptionKey> encryption_key);

  scoped_ptr<EncryptionKey> encryption_key_;

  DISALLOW_COPY_AND_ASSIGN(EncryptorSource);
};

}  // namespace media

#endif  // MEDIA_BASE_ENCRYPTOR_SOURCE_H_
