// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "packager/media/formats/mp2t/ts_section_pes.h"

#include "packager/base/logging.h"
#include "packager/media/base/bit_reader.h"
#include "packager/media/base/timestamp.h"
#include "packager/media/formats/mp2t/es_parser.h"
#include "packager/media/formats/mp2t/mp2t_common.h"

static const int kPesStartCode = 0x000001;

// Given that |time| is coded using 33 bits,
// UnrollTimestamp returns the corresponding unrolled timestamp.
// The unrolled timestamp is defined by:
// |time| + k * (2 ^ 33)
// where k is estimated so that the unrolled timestamp
// is as close as possible to |previous_unrolled_time|.
static int64_t UnrollTimestamp(int64_t previous_unrolled_time, int64_t time) {
  // Mpeg2 TS timestamps have an accuracy of 33 bits.
  const int nbits = 33;

  // |timestamp| has a precision of |nbits|
  // so make sure the highest bits are set to 0.
  DCHECK_EQ((time >> nbits), 0);

  // Consider 3 possibilities to estimate the missing high bits of |time|.
  int64_t previous_unrolled_time_high = (previous_unrolled_time >> nbits);
  int64_t time0 = ((previous_unrolled_time_high - 1) << nbits) | time;
  int64_t time1 = ((previous_unrolled_time_high + 0) << nbits) | time;
  int64_t time2 = ((previous_unrolled_time_high + 1) << nbits) | time;

  // Select the min absolute difference with the current time
  // so as to ensure time continuity.
  int64_t diff0 = time0 - previous_unrolled_time;
  int64_t diff1 = time1 - previous_unrolled_time;
  int64_t diff2 = time2 - previous_unrolled_time;
  if (diff0 < 0)
    diff0 = -diff0;
  if (diff1 < 0)
    diff1 = -diff1;
  if (diff2 < 0)
    diff2 = -diff2;

  int64_t unrolled_time;
  int64_t min_diff;
  if (diff1 < diff0) {
    unrolled_time = time1;
    min_diff = diff1;
  } else {
    unrolled_time = time0;
    min_diff = diff0;
  }
  if (diff2 < min_diff)
    unrolled_time = time2;

  return unrolled_time;
}

static bool IsTimestampSectionValid(int64_t timestamp_section) {
  // |pts_section| has 40 bits:
  // - starting with either '0010' or '0011' or '0001'
  // - and ending with a marker bit.
  // See ITU H.222 standard - PES section.

  // Verify that all the marker bits are set to one.
  return ((timestamp_section & 0x1) != 0) &&
         ((timestamp_section & 0x10000) != 0) &&
         ((timestamp_section & 0x100000000LL) != 0);
}

static int64_t ConvertTimestampSectionToTimestamp(int64_t timestamp_section) {
  return (((timestamp_section >> 33) & 0x7) << 30) |
         (((timestamp_section >> 17) & 0x7fff) << 15) |
         (((timestamp_section >> 1) & 0x7fff) << 0);
}

namespace shaka {
namespace media {
namespace mp2t {

TsSectionPes::TsSectionPes(std::unique_ptr<EsParser> es_parser)
    : es_parser_(es_parser.release()),
      wait_for_pusi_(true),
      previous_pts_valid_(false),
      previous_pts_(0),
      previous_dts_valid_(false),
      previous_dts_(0) {
  DCHECK(es_parser_);
}

TsSectionPes::~TsSectionPes() {
}

bool TsSectionPes::Parse(bool payload_unit_start_indicator,
                         const uint8_t* buf,
                         int size) {
  // Ignore partial PES.
  if (wait_for_pusi_ && !payload_unit_start_indicator)
    return true;

  bool parse_result = true;
  if (payload_unit_start_indicator) {
    // Try emitting a packet since we might have a pending PES packet
    // with an undefined size.
    // In this case, a unit is emitted when the next unit is coming.
    int raw_pes_size;
    const uint8_t* raw_pes;
    pes_byte_queue_.Peek(&raw_pes, &raw_pes_size);
    if (raw_pes_size > 0)
      parse_result = Emit(true);

    // Reset the state.
    ResetPesState();

    // Update the state.
    wait_for_pusi_ = false;
  }

  // Add the data to the parser state.
  if (size > 0)
    pes_byte_queue_.Push(buf, size);

  // Try emitting the current PES packet.
  return (parse_result && Emit(false));
}

bool TsSectionPes::Flush() {
  // Try emitting a packet since we might have a pending PES packet
  // with an undefined size.
  RCHECK(Emit(true));

  // Flush the underlying ES parser.
  return es_parser_->Flush();
}

void TsSectionPes::Reset() {
  ResetPesState();

  previous_pts_valid_ = false;
  previous_pts_ = 0;
  previous_dts_valid_ = false;
  previous_dts_ = 0;

  es_parser_->Reset();
}

bool TsSectionPes::Emit(bool emit_for_unknown_size) {
  int raw_pes_size;
  const uint8_t* raw_pes;
  pes_byte_queue_.Peek(&raw_pes, &raw_pes_size);

  // A PES should be at least 6 bytes.
  // Wait for more data to come if not enough bytes.
  if (raw_pes_size < 6)
    return true;

  // Check whether we have enough data to start parsing.
  int pes_packet_length =
      (static_cast<int>(raw_pes[4]) << 8) |
      (static_cast<int>(raw_pes[5]));
  if ((pes_packet_length == 0 && !emit_for_unknown_size) ||
      (pes_packet_length != 0 && raw_pes_size < pes_packet_length + 6)) {
    // Wait for more data to come either because:
    // - there are not enough bytes,
    // - or the PES size is unknown and the "force emit" flag is not set.
    //   (PES size might be unknown for video PES packet).
    return true;
  }
  DVLOG(LOG_LEVEL_PES) << "pes_packet_length=" << pes_packet_length;

  // Parse the packet.
  bool parse_result = ParseInternal(raw_pes, raw_pes_size);

  // Reset the state.
  ResetPesState();

  return parse_result;
}

bool TsSectionPes::ParseInternal(const uint8_t* raw_pes, int raw_pes_size) {
  BitReader bit_reader(raw_pes, raw_pes_size);

  // Read up to the pes_packet_length (6 bytes).
  int packet_start_code_prefix;
  int stream_id;
  int pes_packet_length;
  RCHECK(bit_reader.ReadBits(24, &packet_start_code_prefix));
  RCHECK(bit_reader.ReadBits(8, &stream_id));
  RCHECK(bit_reader.ReadBits(16, &pes_packet_length));

  RCHECK(packet_start_code_prefix == kPesStartCode);
  DVLOG(LOG_LEVEL_PES) << "stream_id=" << stream_id;
  if (pes_packet_length == 0)
    pes_packet_length = static_cast<int>(bit_reader.bits_available()) / 8;

  // Ignore the PES for unknown stream IDs.
  // ATSC Standard A/52:2012 3. GENERIC IDENTIFICATION OF AN AC-3 STREAM.
  // AC3/E-AC3 stream uses private stream id.
  const int kPrivateStream1 = 0xBD;
  // See ITU H.222 Table 2-22 "Stream_id assignments"
  bool is_audio_stream_id =
      ((stream_id & 0xe0) == 0xc0) || stream_id == kPrivateStream1;
  bool is_video_stream_id = ((stream_id & 0xf0) == 0xe0);
  if (!is_audio_stream_id && !is_video_stream_id)
    return true;

  // Read up to "pes_header_data_length".
  int dummy_2;
  int PES_scrambling_control;
  int PES_priority;
  int data_alignment_indicator;
  int copyright;
  int original_or_copy;
  int pts_dts_flags;
  int escr_flag;
  int es_rate_flag;
  int dsm_trick_mode_flag;
  int additional_copy_info_flag;
  int pes_crc_flag;
  int pes_extension_flag;
  int pes_header_data_length;
  RCHECK(bit_reader.ReadBits(2, &dummy_2));
  RCHECK(dummy_2 == 0x2);
  RCHECK(bit_reader.ReadBits(2, &PES_scrambling_control));
  RCHECK(bit_reader.ReadBits(1, &PES_priority));
  RCHECK(bit_reader.ReadBits(1, &data_alignment_indicator));
  RCHECK(bit_reader.ReadBits(1, &copyright));
  RCHECK(bit_reader.ReadBits(1, &original_or_copy));
  RCHECK(bit_reader.ReadBits(2, &pts_dts_flags));
  RCHECK(bit_reader.ReadBits(1, &escr_flag));
  RCHECK(bit_reader.ReadBits(1, &es_rate_flag));
  RCHECK(bit_reader.ReadBits(1, &dsm_trick_mode_flag));
  RCHECK(bit_reader.ReadBits(1, &additional_copy_info_flag));
  RCHECK(bit_reader.ReadBits(1, &pes_crc_flag));
  RCHECK(bit_reader.ReadBits(1, &pes_extension_flag));
  RCHECK(bit_reader.ReadBits(8, &pes_header_data_length));
  int pes_header_start_size = static_cast<int>(bit_reader.bits_available()) / 8;

  // Compute the size and the offset of the ES payload.
  // "6" for the 6 bytes read before and including |pes_packet_length|.
  // "3" for the 3 bytes read before and including |pes_header_data_length|.
  int es_size = pes_packet_length - 3 - pes_header_data_length;
  int es_offset = 6 + 3 + pes_header_data_length;
  RCHECK(es_size >= 0);
  RCHECK(es_offset + es_size <= raw_pes_size);

  // Read the timing information section.
  bool is_pts_valid = false;
  bool is_dts_valid = false;
  int64_t pts_section = 0;
  int64_t dts_section = 0;
  if (pts_dts_flags == 0x2) {
    RCHECK(bit_reader.ReadBits(40, &pts_section));
    RCHECK((((pts_section >> 36) & 0xf) == 0x2) &&
           IsTimestampSectionValid(pts_section));
    is_pts_valid = true;
  }
  if (pts_dts_flags == 0x3) {
    RCHECK(bit_reader.ReadBits(40, &pts_section));
    RCHECK(bit_reader.ReadBits(40, &dts_section));
    RCHECK((((pts_section >> 36) & 0xf) == 0x3) &&
           IsTimestampSectionValid(pts_section));
    RCHECK((((dts_section >> 36) & 0xf) == 0x1) &&
           IsTimestampSectionValid(dts_section));
    is_pts_valid = true;
    is_dts_valid = true;
  }

  // Convert and unroll the timestamps.
  int64_t media_pts(kNoTimestamp);
  int64_t media_dts(kNoTimestamp);
  if (is_dts_valid) {
    int64_t dts = ConvertTimestampSectionToTimestamp(dts_section);
    if (previous_dts_valid_)
      dts = UnrollTimestamp(previous_dts_, dts);
    previous_dts_ = dts;
    previous_dts_valid_ = true;
    media_dts = dts;
  }
  if (is_pts_valid) {
    int64_t pts = ConvertTimestampSectionToTimestamp(pts_section);
    if (previous_pts_valid_) {
      pts = UnrollTimestamp(previous_pts_, pts);
    } else {
      if (media_dts != kNoTimestamp) {
        pts = UnrollTimestamp(media_dts, pts);
      }
    }
    previous_pts_ = pts;
    previous_pts_valid_ = true;
    media_pts = pts;
  }

  // Discard the rest of the PES packet header.
  DCHECK_EQ(bit_reader.bits_available() % 8, 0u);
  int pes_header_remaining_size =
      pes_header_data_length -
      (pes_header_start_size -
       static_cast<int>(bit_reader.bits_available()) / 8);
  RCHECK(pes_header_remaining_size >= 0);

  // Read the PES packet.
  DVLOG(LOG_LEVEL_PES) << "Emit a reassembled PES:"
                       << " size=" << es_size << " pts=" << media_pts
                       << " dts=" << media_dts << " data_alignment_indicator="
                       << data_alignment_indicator;
  return es_parser_->Parse(&raw_pes[es_offset], es_size, media_pts, media_dts);
}

void TsSectionPes::ResetPesState() {
  pes_byte_queue_.Reset();
  wait_for_pusi_ = true;
}

}  // namespace mp2t
}  // namespace media
}  // namespace shaka
