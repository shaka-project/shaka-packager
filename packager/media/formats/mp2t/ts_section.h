// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PACKAGER_MEDIA_FORMATS_MP2T_TS_SECTION_H_
#define PACKAGER_MEDIA_FORMATS_MP2T_TS_SECTION_H_

#include <cstdint>

namespace shaka {
namespace media {
namespace mp2t {

class TsSection {
 public:
  // From ISO/IEC 13818-1 or ITU H.222 spec: Table 2-3 - PID table.
  enum SpecialPid {
    kPidPat = 0x0,
    kPidCat = 0x1,
    kPidTsdt = 0x2,
    kPidNullPacket = 0x1fff,
    kPidMax = 0x1fff,
  };

  virtual ~TsSection() {}

  // Parse the data bytes of the TS packet.
  // Return true if parsing is successful.
  virtual bool Parse(bool payload_unit_start_indicator,
                     const uint8_t* buf,
                     int size) = 0;

  // Process bytes that have not been processed yet (pending buffers in the
  // pipe). Flush might thus results in frame emission, as an example.
  virtual bool Flush() = 0;

  // Reset the state of the parser to its initial state.
  virtual void Reset() = 0;
};

}  // namespace mp2t
}  // namespace media
}  // namespace shaka

#endif
