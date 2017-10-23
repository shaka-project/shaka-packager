// Copyright 2017 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_FORMATS_MP2T_TS_STREAM_TYPE_H_
#define PACKAGER_MEDIA_FORMATS_MP2T_TS_STREAM_TYPE_H_

#include <stdint.h>

namespace shaka {
namespace media {
namespace mp2t {

enum class TsStreamType {
  // ISO-13818.1 / ITU H.222 Table 2-34 "Stream type assignments"
  kAdtsAac = 0x0F,
  kAvc = 0x1B,
  kHevc = 0x24,
  // ATSC Standard A/52.
  kAc3 = 0x81,
  kEac3 = 0x87,
  // MPEG-2 Stream Encryption Format for HTTP Live Streaming:
  // https://goo.gl/N7Tvqi.
  kEncryptedAc3 = 0xC1,
  kEncryptedEac3 = 0xC2,
  kEncryptedAdtsAac = 0xCF,
  kEncryptedAvc = 0xDB,
};

}  // namespace mp2t
}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_FORMATS_MP2T_TS_STREAM_TYPE_H_
