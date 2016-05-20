// Copyright 2016 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

// This file contains utility functions that help write TS packets to buffer.

#ifndef PACKAGER_MEDIA_FORMATS_MP2T_TS_WRITER_UTIL_H_
#define PACKAGER_MEDIA_FORMATS_MP2T_TS_WRITER_UTIL_H_

#include <stddef.h>
#include <stdint.h>

namespace shaka {
namespace media {

class BufferWriter;

namespace mp2t {

class ContinuityCounter;

/// General purpose TS packet writing function. The output goes to @a output.
/// @param payload can be any payload. Most likely raw PSI tables or PES packet
///        payload.
/// @param payload_size is the size of payload.
/// @param payload_unit_start_indicator is the same as the definition in spec.
/// @param pid is the same the same defition in the spec.
/// @param has_pcr is true if @a pcr_base should be used.
/// @param pcr_base is the PCR_base value in the spec.
/// @param continuity_counter is the continuity_counter for this TS packet.
/// @param output is where the TS packets get written.
void WritePayloadToBufferWriter(const uint8_t* payload,
                                size_t payload_size,
                                bool payload_unit_start_indicator,
                                int pid,
                                bool has_pcr,
                                uint64_t pcr_base,
                                ContinuityCounter* continuity_counter,
                                BufferWriter* output);

}  // namespace mp2t
}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_FORMATS_MP2T_TS_WRITER_UTIL_H_
