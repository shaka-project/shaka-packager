// Copyright 2023 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_FORMATS_MP2T_TS_AUDIO_TYPE_H
#define PACKAGER_MEDIA_FORMATS_MP2T_TS_AUDIO_TYPE_H

#include <cstdint>

namespace shaka {
namespace media {
namespace mp2t {

enum class TsAudioType : uint8_t {
  // ISO-13818.1 / ITU H.222 Table 2-60 "Audio type values"
  kUndefined = 0x00,
  kCleanEffects = 0x01,
  kHearingImpaired = 0x02,
  kVisualyImpairedCommentary = 0x03,
  // 0x04-0x7F - user private
  // 0x80-0xFF - reserved
};

}  // namespace mp2t
}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_FORMATS_MP2T_TS_AUDIO_TYPE_H
