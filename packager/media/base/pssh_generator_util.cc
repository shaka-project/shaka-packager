// Copyright 2017 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/base/pssh_generator_util.h"

#include <string>

#include "packager/media/base/widevine_pssh_data.pb.h"

namespace shaka {
namespace media {
namespace {

std::vector<uint8_t> StringToBytes(const std::string& string) {
  return std::vector<uint8_t>(string.begin(), string.end());
}
}  // namespace

std::vector<uint8_t> GenerateWidevinePsshDataFromKeyIds(
    const std::vector<std::vector<uint8_t>>& key_ids) {
  media::WidevinePsshData widevine_pssh_data;
  for (const std::vector<uint8_t>& key_id : key_ids)
    widevine_pssh_data.add_key_id(key_id.data(), key_id.size());
  return StringToBytes(widevine_pssh_data.SerializeAsString());
}
}  // namespace media
}  // namespace shaka
