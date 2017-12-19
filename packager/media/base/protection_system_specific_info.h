// Copyright 2016 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_BASE_PSSH_H_
#define PACKAGER_MEDIA_BASE_PSSH_H_

#include <stdint.h>
#include <vector>

#include "packager/base/logging.h"
#include "packager/media/base/buffer_reader.h"

namespace shaka {
namespace media {

class ProtectionSystemSpecificInfo {
 public:
  ProtectionSystemSpecificInfo();
  ~ProtectionSystemSpecificInfo();

  /// Parses multiple PSSH boxes from @a data.  These boxes should be
  /// concatenated together.  Any non-PSSH box is an error.
  /// @return true on success; false on failure.
  static bool ParseBoxes(
      const uint8_t* data,
      size_t data_size,
      std::vector<ProtectionSystemSpecificInfo>* pssh_boxes);

  /// Parses the given PSSH box into this object.
  /// @return true on success; false on failure.
  bool Parse(const uint8_t* data, size_t data_size);

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
  uint8_t version_;
  std::vector<uint8_t> system_id_;
  std::vector<std::vector<uint8_t>> key_ids_;
  std::vector<uint8_t> pssh_data_;

  // Don't use DISALLOW_COPY_AND_ASSIGN since the data stored here should be
  // small, so the performance impact should be minimal.
};

}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_BASE_PSSH_H_
