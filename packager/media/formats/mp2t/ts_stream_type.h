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
  kMpeg1Video = 0x01,  // ISO/IEC 11172-2 Video
  kMpeg2Video = 0x02,  // ITU-T H.262 | ISO/IEC 13818-2 Video |
                       // ISO/IEC 11172-2 constrained parameter video stream
  kMpeg1Audio = 0x03,  // ISO/IEC 11172-3 Audio
  kMpeg2Audio = 0x04,  // ISO/IEC 13818-3 Audio
  kPesPrivateData =
      0x06,  // ISO/IEC 13818-1 PES packets containing private data. It is also
             // used by DVB (TS 101 154 DVB specification ...) to carry AC3 in
             // TS (while ATSC uses 0x81 defined below).
  kAdtsAac = 0x0F,
  kAvc = 0x1B,
  kHevc = 0x24,
  // Below are extensions defined in other specifications.
  // AC3 and E-AC3 are defined in ATSC Standard A/52.
  // Cannot find specification for DTS-HD and DTS. They are extracted from
  // https://sno.phy.queensu.ca/~phil/exiftool/TagNames/M2TS.html.
  kAc3 = 0x81,
  kDtsHd = 0x86,
  kEac3 = 0x87,
  kDts = 0x8A,
  // MPEG-2 Stream Encryption Format for HTTP Live Streaming:
  // https://goo.gl/N7Tvqi.
  kEncryptedAc3 = 0xC1,
  kEncryptedEac3 = 0xC2,
  kEncryptedAdtsAac = 0xCF,
  kEncryptedAvc = 0xDB,

  // Below are internal values used to select other stream types based on other
  // info in headers.
  kDvbSubtitles = 0x100,
  kTeletextSubtitles = 0x101,
};

}  // namespace mp2t
}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_FORMATS_MP2T_TS_STREAM_TYPE_H_
