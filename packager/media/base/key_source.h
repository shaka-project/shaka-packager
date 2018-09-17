// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_BASE_KEY_SOURCE_H_
#define PACKAGER_MEDIA_BASE_KEY_SOURCE_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "packager/media/base/fourccs.h"
#include "packager/media/base/protection_system_specific_info.h"
#include "packager/media/base/pssh_generator.h"
#include "packager/status.h"

namespace shaka {
namespace media {

/// Encrypted media init data types. It is extended from:
/// https://www.w3.org/TR/eme-initdata-registry/#registry.
enum class EmeInitDataType {
  UNKNOWN,
  /// One or multiple PSSH boxes.
  CENC,
  /// WebM init data is basically KeyId.
  WEBM,
  /// JSON formatted key ids.
  KEYIDS,
  /// Widevine classic asset id.
  WIDEVINE_CLASSIC,
  MAX = WIDEVINE_CLASSIC
};

struct EncryptionKey {
  std::vector<ProtectionSystemSpecificInfo> key_system_info;
  std::vector<uint8_t> key_id;
  std::vector<uint8_t> key;
  std::vector<uint8_t> iv;
};

typedef std::map<std::string, std::unique_ptr<EncryptionKey>> EncryptionKeyMap;

/// KeySource is responsible for encryption key acquisition.
class KeySource {
 public:
  KeySource(int protection_systems_flags, FourCC protection_scheme);

  virtual ~KeySource();

  /// Fetch keys based on the specified encrypted media init data.
  /// @param init_data_type specifies the encrypted media init data type.
  /// @param init_data contains the init data.
  /// @return OK on success, an error status otherwise.
  virtual Status FetchKeys(EmeInitDataType init_data_type,
                           const std::vector<uint8_t>& init_data) = 0;

  /// Get encryption key of the specified stream label.
  /// @param stream_label is the label of stream for which retrieving the key.
  /// @param key is a pointer to the EncryptionKey which will hold the retrieved
  ///        key. Owner retains ownership, and may not be NULL.
  /// @return OK on success, an error status otherwise.
  virtual Status GetKey(const std::string& stream_label,
                        EncryptionKey* key) = 0;

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
  /// @param stream_label is the label of stream for which retrieving the key.
  /// @param key is a pointer to the EncryptionKey which will hold the retrieved
  ///        key. Owner retains ownership, and may not be NULL.
  /// @return OK on success, an error status otherwise.
  virtual Status GetCryptoPeriodKey(uint32_t crypto_period_index,
                                    const std::string& stream_label,
                                    EncryptionKey* key) = 0;

 protected:
  /// Update the protection sysmtem specific info for the encryption keys.
  /// @param encryption_key_map is a map of encryption keys for all tracks.
  Status UpdateProtectionSystemInfo(EncryptionKeyMap* encryption_key_map);

 private:
  std::vector<std::unique_ptr<PsshGenerator>> pssh_generators_;
  std::vector<std::vector<uint8_t>> no_pssh_systems_;

  DISALLOW_COPY_AND_ASSIGN(KeySource);
};

}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_BASE_KEY_SOURCE_H_
