// Copyright 2017 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_BASE_PSSH_GENERATOR_H_
#define PACKAGER_MEDIA_BASE_PSSH_GENERATOR_H_

#include <cstdint>
#include <optional>
#include <vector>

#include <packager/media/base/protection_system_specific_info.h>
#include <packager/status.h>

// TODO(hmchen): move pssh related files into a sperate folder.
namespace shaka {
namespace media {

class PsshGenerator {
 public:
  /// @param system_id is the protection system id for the PSSH.
  /// @param box_version specifies the version of the new PSSH box.
  PsshGenerator(const std::vector<uint8_t>& system_id, uint8_t box_version);
  virtual ~PsshGenerator();

  /// @return  whether the PSSH generates the PSSH box based on multiple key
  ///          IDs.
  virtual bool SupportMultipleKeys() = 0;

  /// Generate the PSSH and set the ProtectionSystemSpecificInfo.
  /// @param key_ids is a vector of key IDs for all tracks.
  /// @param info is a pointer to the ProtectionSystemSpecificInfo for setting
  ///        the PSSH box.
  Status GeneratePsshFromKeyIds(
      const std::vector<std::vector<uint8_t>>& key_ids,
      ProtectionSystemSpecificInfo* info) const;

  /// Generate the PSSH and set the ProtectionSystemSpecificInfo.
  /// @param key_id is a the unique identifier for the key.
  /// @param key is the content key.
  /// @param info is a pointer to the ProtectionSystemSpecificInfo for setting
  ///        the PSSH box.
  Status GeneratePsshFromKeyIdAndKey(const std::vector<uint8_t>& key_id,
                                     const std::vector<uint8_t>& key,
                                     ProtectionSystemSpecificInfo* info) const;

 private:
  /// Return the PSSH data generated from multiple |key_ids| on success. If
  /// error happens, it returns no value.
  /// @param key_ids is a set of key IDs for all tracks.
  virtual std::optional<std::vector<uint8_t>> GeneratePsshDataFromKeyIds(
      const std::vector<std::vector<uint8_t>>& key_ids) const = 0;

  /// Return the PSSH data generated from a pair of |key_id| and |key| on
  /// success. If error happens, it returns no value.
  /// @param key_id is a the unique identifier for the key.
  /// @param key is the key for generating the PSSH box.
  virtual std::optional<std::vector<uint8_t>> GeneratePsshDataFromKeyIdAndKey(
      const std::vector<uint8_t>& key_id,
      const std::vector<uint8_t>& key) const = 0;

  std::vector<uint8_t> system_id_;
  uint8_t box_version_ = 0;
};

}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_BASE_PSSH_GENERATOR_H_
