// Copyright 2022 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_UTILS_BYTES_TO_STRING_VIEW_H_
#define PACKAGER_UTILS_BYTES_TO_STRING_VIEW_H_

#include <cstdint>
#include <string_view>
#include <vector>

namespace shaka {

/// Convert byte array to string_view
inline std::string_view byte_array_to_string_view(const uint8_t* bytes,
                                                  size_t bytes_size) {
  return {reinterpret_cast<const char*>(bytes), bytes_size};
}

/// Convert byte vector to string_view
inline std::string_view byte_vector_to_string_view(
    const std::vector<uint8_t>& bytes) {
  return byte_array_to_string_view(bytes.data(), bytes.size());
}

}  // namespace shaka
#endif  // PACKAGER_UTILS_BYTES_TO_STRING_VIEW_H_
