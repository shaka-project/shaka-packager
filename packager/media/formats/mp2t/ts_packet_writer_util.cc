// Copyright 2016 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/media/formats/mp2t/ts_packet_writer_util.h>

#include <absl/log/check.h>
#include <absl/log/log.h>

#include <packager/media/base/buffer_writer.h>
#include <packager/media/formats/mp2t/continuity_counter.h>

namespace shaka {
namespace media {
namespace mp2t {

namespace {

const int kPcrFieldsSize = 6;
const uint8_t kSyncByte = 0x47;

// This is the size of the first few fields in a TS packet, i.e. TS packet size
// without adaptation field or the payload.
const int kTsPacketHeaderSize = 4;
const int kTsPacketSize = 188;
const int kTsPacketMaximumPayloadSize =
    kTsPacketSize - kTsPacketHeaderSize;

// Used for adaptation field padding bytes.
const uint8_t kPaddingBytes[] = {
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
};
static_assert(std::size(kPaddingBytes) >= kTsPacketMaximumPayloadSize,
              "Padding array is not big enough.");

// |remaining_data_size| is the amount of data that has to be written. This may
// be bigger than a TS packet size.
// |remaining_data_size| matters if it is short and requires padding.
void WriteAdaptationField(bool has_pcr,
                          uint64_t pcr_base,
                          size_t remaining_data_size,
                          BufferWriter* writer) {
  // Special case where a TS packet requires 1 byte padding.
  if (!has_pcr && remaining_data_size == kTsPacketMaximumPayloadSize - 1) {
    writer->AppendInt(static_cast<uint8_t>(0));
    return;
  }

  // The size of the field itself.
  const int kAdaptationFieldLengthSize = 1;

  // The size of all leading flags (not including the adaptation_field_length).
  const int kAdaptationFieldHeaderSize = 1;
  size_t adaptation_field_length =
      kAdaptationFieldHeaderSize + (has_pcr ? kPcrFieldsSize : 0);
  if (remaining_data_size < kTsPacketMaximumPayloadSize) {
    const size_t current_ts_size = kTsPacketHeaderSize + remaining_data_size +
                                   adaptation_field_length +
                                   kAdaptationFieldLengthSize;
    if (current_ts_size < kTsPacketSize) {
      adaptation_field_length += kTsPacketSize - current_ts_size;
    }
  }

  writer->AppendInt(static_cast<uint8_t>(adaptation_field_length));
  int remaining_bytes = static_cast<int>(adaptation_field_length);
  writer->AppendInt(static_cast<uint8_t>(
      // All flags except PCR_flag are 0.
      static_cast<uint8_t>(has_pcr) << 4));
  remaining_bytes -= 1;

  if (has_pcr) {
    // program_clock_reference_extension = 0.
    const uint32_t most_significant_32bits_pcr =
        static_cast<uint32_t>(pcr_base >> 1);
    const uint16_t pcr_last_bit_reserved_and_pcr_extension =
        ((pcr_base & 1) << 15) | 0x7e00;  // Set the 6 reserved bits to '1'
    writer->AppendInt(most_significant_32bits_pcr);
    writer->AppendInt(pcr_last_bit_reserved_and_pcr_extension);
    remaining_bytes -= kPcrFieldsSize;
  }
  DCHECK_GE(remaining_bytes, 0);
  if (remaining_bytes == 0)
    return;

  DCHECK_GE(static_cast<int>(std::size(kPaddingBytes)), remaining_bytes);
  writer->AppendArray(kPaddingBytes, remaining_bytes);
}

}  // namespace

void WritePayloadToBufferWriter(const uint8_t* payload,
                                size_t payload_size,
                                bool payload_unit_start_indicator,
                                int pid,
                                bool has_pcr,
                                uint64_t pcr_base,
                                ContinuityCounter* continuity_counter,
                                BufferWriter* writer) {
  size_t payload_bytes_written = 0;

  do {
    const bool must_write_adaptation_header = has_pcr;
    const size_t bytes_left = payload_size - payload_bytes_written;
    const bool has_adaptation_field = must_write_adaptation_header ||
                                      bytes_left < kTsPacketMaximumPayloadSize;

    writer->AppendInt(kSyncByte);
    writer->AppendInt(static_cast<uint16_t>(
        // transport_error_indicator and transport_priority are both '0'.
        static_cast<int>(payload_unit_start_indicator) << 14 | pid));

    const uint8_t adaptation_field_control =
        ((has_adaptation_field ? 1 : 0) << 1) | ((bytes_left != 0) ? 1 : 0);
    // transport_scrambling_control is '00'.
    writer->AppendInt(static_cast<uint8_t>(adaptation_field_control << 4 |
                                           continuity_counter->GetNext()));

    if (has_adaptation_field) {
      const size_t before = writer->Size();
      WriteAdaptationField(has_pcr, pcr_base, bytes_left, writer);
      const size_t bytes_for_adaptation_field = writer->Size() - before;

      const size_t write_bytes =
          kTsPacketMaximumPayloadSize - bytes_for_adaptation_field;
      writer->AppendArray(payload + payload_bytes_written, write_bytes);
      payload_bytes_written += write_bytes;
    } else {
      writer->AppendArray(payload + payload_bytes_written,
                          kTsPacketMaximumPayloadSize);
      payload_bytes_written += kTsPacketMaximumPayloadSize;
    }

    // Once written, not needed for this payload.
    has_pcr = false;
    payload_unit_start_indicator = false;
  } while (payload_bytes_written < payload_size);
}

}  // namespace mp2t
}  // namespace media
}  // namespace shaka
