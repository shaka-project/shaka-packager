// Copyright 2017 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd
//
// Extends gflags to support hex formatted bytes.

#ifndef PACKAGER_APP_GFLAGS_HEX_BYTES_H_
#define PACKAGER_APP_GFLAGS_HEX_BYTES_H_

#include <gflags/gflags.h>

#include <string>
#include <vector>

namespace shaka {
bool ValidateHexString(const char* flagname,
                       const std::string& value,
                       std::vector<uint8_t>* value_bytes);
}  // namespace shaka

// The raw bytes will be available in FLAGS_##name##_bytes.
// The original gflag variable FLAGS_##name is defined in shaka_gflags_extension
// and not exposed directly.
#define DECLARE_hex_bytes(name)                     \
  namespace shaka_gflags_extension {                \
  DECLARE_string(name);                             \
  }                                                 \
  namespace shaka_gflags_extension {                \
  extern std::vector<uint8_t> FLAGS_##name##_bytes; \
  }                                                 \
  using shaka_gflags_extension::FLAGS_##name##_bytes

#define DEFINE_hex_bytes(name, val, txt)                            \
  namespace shaka_gflags_extension {                                \
  DEFINE_string(name, val, txt);                                    \
  }                                                                 \
  namespace shaka_gflags_extension {                                \
  std::vector<uint8_t> FLAGS_##name##_bytes;                        \
  static bool hex_validator_##name = gflags::RegisterFlagValidator( \
      &FLAGS_##name,                                                \
      [](const char* flagname, const std::string& value) {          \
        return shaka::ValidateHexString(flagname, value,            \
                                        &FLAGS_##name##_bytes);     \
      });                                                           \
  }                                                                 \
  using shaka_gflags_extension::FLAGS_##name##_bytes

#endif  // PACKAGER_APP_GFLAGS_HEX_BYTES_H_
