// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "packager/media/formats/mp2t/ts_section_pmt.h"

#include <vector>

#include "packager/base/logging.h"
#include "packager/media/base/bit_reader.h"
#include "packager/media/formats/mp2t/mp2t_common.h"
#include "packager/media/formats/mp2t/ts_stream_type.h"

namespace shaka {
namespace media {
namespace mp2t {

TsSectionPmt::TsSectionPmt(const RegisterPesCb& register_pes_cb)
    : register_pes_cb_(register_pes_cb) {
}

TsSectionPmt::~TsSectionPmt() {
}

bool TsSectionPmt::ParsePsiSection(BitReader* bit_reader) {
  // Read up to |last_section_number|.
  int table_id;
  int section_syntax_indicator;
  int dummy_zero;
  int reserved;
  int section_length;
  int program_number;
  int version_number;
  int current_next_indicator;
  int section_number;
  int last_section_number;
  RCHECK(bit_reader->ReadBits(8, &table_id));
  RCHECK(bit_reader->ReadBits(1, &section_syntax_indicator));
  RCHECK(bit_reader->ReadBits(1, &dummy_zero));
  RCHECK(bit_reader->ReadBits(2, &reserved));
  RCHECK(bit_reader->ReadBits(12, &section_length));
  int section_start_marker = static_cast<int>(bit_reader->bits_available()) / 8;

  RCHECK(bit_reader->ReadBits(16, &program_number));
  RCHECK(bit_reader->ReadBits(2, &reserved));
  RCHECK(bit_reader->ReadBits(5, &version_number));
  RCHECK(bit_reader->ReadBits(1, &current_next_indicator));
  RCHECK(bit_reader->ReadBits(8, &section_number));
  RCHECK(bit_reader->ReadBits(8, &last_section_number));

  // Perform a few verifications:
  // - table ID should be 2 for a PMT.
  // - section_syntax_indicator should be one.
  // - section length should not exceed 1021.
  RCHECK(table_id == 0x2);
  RCHECK(section_syntax_indicator);
  RCHECK(!dummy_zero);
  RCHECK(section_length <= 1021);
  RCHECK(section_number == 0);
  RCHECK(last_section_number == 0);

  // Read the end of the fixed length section.
  int pcr_pid;
  int program_info_length;
  RCHECK(bit_reader->ReadBits(3, &reserved));
  RCHECK(bit_reader->ReadBits(13, &pcr_pid));
  RCHECK(bit_reader->ReadBits(4, &reserved));
  RCHECK(bit_reader->ReadBits(12, &program_info_length));
  RCHECK(program_info_length < 1024);

  // Read the program info descriptor.
  // Defined in section 2.6 of ISO-13818.
  RCHECK(bit_reader->SkipBits(8 * program_info_length));

  // Read the ES description table.
  // The end of the PID map if 4 bytes away from the end of the section
  // (4 bytes = size of the CRC).
  int pid_map_end_marker = section_start_marker - section_length + 4;
  struct Info {
    int pid_es;
    TsStreamType stream_type;
    const uint8_t* descriptor;
    size_t descriptor_length;
  };
  std::vector<Info> pid_info;
  while (static_cast<int>(bit_reader->bits_available()) >
         8 * pid_map_end_marker) {
    TsStreamType stream_type;
    int pid_es;
    size_t es_info_length;
    RCHECK(bit_reader->ReadBits(8, &stream_type));
    RCHECK(bit_reader->SkipBits(3));  // reserved
    RCHECK(bit_reader->ReadBits(13, &pid_es));
    RCHECK(bit_reader->ReadBits(4, &reserved));
    RCHECK(bit_reader->ReadBits(12, &es_info_length));
    const uint8_t* descriptor = bit_reader->current_byte_ptr();

    // Do not register the PID right away.
    // Wait for the end of the section to be fully parsed
    // to make sure there is no error.
    pid_info.push_back({pid_es, stream_type, descriptor, es_info_length});

    // Read the ES info descriptors.
    // Defined in section 2.6 of ISO-13818.
    if (es_info_length > 0) {
      uint8_t descriptor_tag;
      RCHECK(bit_reader->ReadBits(8, &descriptor_tag));
      es_info_length--;

      // See ETSI EN 300 468 Section 6.1
      if (stream_type == TsStreamType::kPesPrivateData) {
        switch (descriptor_tag) {
          case 0x56: // teletext_descriptor
            pid_info.back().stream_type = TsStreamType::kTeletextSubtitles;
            break;
          case 0x59: // subtitling_descriptor
            pid_info.back().stream_type = TsStreamType::kDvbSubtitles;
            break;
          default:
            break;
        }
      }
    }
    RCHECK(bit_reader->SkipBits(8 * es_info_length));
  }

  // Read the CRC.
  int crc32;
  RCHECK(bit_reader->ReadBits(32, &crc32));

  // Once the PMT has been proved to be correct, register the PIDs.
  for (auto& info : pid_info) {
    register_pes_cb_.Run(info.pid_es, info.stream_type, info.descriptor,
                         info.descriptor_length);
  }

  return true;
}

void TsSectionPmt::ResetPsiSection() {
}

}  // namespace mp2t
}  // namespace media
}  // namespace shaka
