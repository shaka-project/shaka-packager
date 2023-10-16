// Copyright 2017 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/media/base/widevine_pssh_generator.h>

#include <packager/macros/compiler.h>
#include <packager/macros/logging.h>
#include <packager/media/base/protection_system_ids.h>
#include <packager/media/base/widevine_pssh_data.pb.h>

namespace shaka {
namespace media {
namespace {
// Use version 0 for backward compatibility.
const uint8_t kWidevinePsshBoxVersion = 0;

std::vector<uint8_t> StringToBytes(const std::string& string) {
  return std::vector<uint8_t>(string.begin(), string.end());
}
}  // namespace

WidevinePsshGenerator::WidevinePsshGenerator(FourCC protection_scheme)
    : PsshGenerator(std::vector<uint8_t>(std::begin(kWidevineSystemId),
                                         std::end(kWidevineSystemId)),
                    kWidevinePsshBoxVersion),
      protection_scheme_(protection_scheme) {}

WidevinePsshGenerator::~WidevinePsshGenerator() {}

bool WidevinePsshGenerator::SupportMultipleKeys() {
  return true;
}

std::optional<std::vector<uint8_t>>
WidevinePsshGenerator::GeneratePsshDataFromKeyIds(
    const std::vector<std::vector<uint8_t>>& key_ids) const {
  media::WidevinePsshData widevine_pssh_data;
  for (const std::vector<uint8_t>& key_id : key_ids)
    widevine_pssh_data.add_key_id(key_id.data(), key_id.size());
  if (protection_scheme_ != FOURCC_NULL)
    widevine_pssh_data.set_protection_scheme(protection_scheme_);
  return StringToBytes(widevine_pssh_data.SerializeAsString());
}

std::optional<std::vector<uint8_t>>
WidevinePsshGenerator::GeneratePsshDataFromKeyIdAndKey(
    const std::vector<uint8_t>& key_id,
    const std::vector<uint8_t>& key) const {
  UNUSED(key_id);
  UNUSED(key);
  NOTIMPLEMENTED();
  return std::nullopt;
}

}  // namespace media
}  // namespace shaka
