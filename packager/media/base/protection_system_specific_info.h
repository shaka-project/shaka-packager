// Copyright 2016 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_BASE_PROTECTION_SYSTEM_SPECIFIC_INFO_H_
#define PACKAGER_MEDIA_BASE_PROTECTION_SYSTEM_SPECIFIC_INFO_H_

#include <cstdint>
#include <memory>
#include <vector>

#include <absl/log/check.h>
#include <absl/log/log.h>

#include <packager/crypto_params.h>

namespace shaka {
namespace media {

struct ProtectionSystemSpecificInfo {
  std::vector<uint8_t> system_id;
  std::vector<uint8_t> psshs;

  /// Parses multiple PSSH boxes from @a data.  These boxes should be
  /// concatenated together.  Any non-PSSH box is an error.
  /// @return true on success; false on failure.
  static bool ParseBoxes(
      const uint8_t* data,
      size_t data_size,
      std::vector<ProtectionSystemSpecificInfo>* pssh_boxes);
};

class PsshBoxBuilder {
 public:
  PsshBoxBuilder() = default;
  ~PsshBoxBuilder() = default;

  /// Parses the given PSSH box into this object.
  /// @return nullptr on failure.
  static std::unique_ptr<PsshBoxBuilder> ParseFromBox(const uint8_t* data,
                                                      size_t data_size);

  /// Creates a PSSH box for the current data.
  std::vector<uint8_t> CreateBox() const;

  uint8_t pssh_box_version() const { return version_; }
  const std::vector<uint8_t>& system_id() const { return system_id_; }
  const std::vector<std::vector<uint8_t>>& key_ids() const { return key_ids_; }
  const std::vector<uint8_t>& pssh_data() const { return pssh_data_; }

  void set_pssh_box_version(uint8_t version) {
    DCHECK_LT(version, 2);
    version_ = version;
  }
  void set_system_id(const uint8_t* system_id, size_t system_id_size) {
    DCHECK_EQ(16u, system_id_size);
    system_id_.assign(system_id, system_id + system_id_size);
  }
  void add_key_id(const std::vector<uint8_t>& key_id) {
    key_ids_.push_back(key_id);
  }
  void clear_key_ids() { key_ids_.clear(); }
  void set_pssh_data(const std::vector<uint8_t>& pssh_data) {
    pssh_data_ = pssh_data;
  }

 private:
  PsshBoxBuilder(const PsshBoxBuilder&) = delete;
  PsshBoxBuilder& operator=(const PsshBoxBuilder&) = delete;

  uint8_t version_ = 0;
  std::vector<uint8_t> system_id_;
  std::vector<std::vector<uint8_t>> key_ids_;
  std::vector<uint8_t> pssh_data_;
};

}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_BASE_PROTECTION_SYSTEM_SPECIFIC_INFO_H_
