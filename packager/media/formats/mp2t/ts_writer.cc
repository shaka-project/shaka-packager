// Copyright 2016 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/formats/mp2t/ts_writer.h"

#include <algorithm>

#include "packager/base/logging.h"
#include "packager/media/base/audio_stream_info.h"
#include "packager/media/base/buffer_writer.h"
#include "packager/media/base/stream_info.h"
#include "packager/media/base/video_stream_info.h"

namespace edash_packager {
namespace media {
namespace mp2t {

namespace {

enum Pid : uint8_t {
  // The pid can be 13 bits long but 8 bits is sufficient for this library.
  // This is the minimum PID that can be used for PMT.
  kPmtPid = 0x20,
  // This is arbitrary number that is not reserved by the spec.
  kElementaryPid = 0x50,
};

// Program number is 16 bits but 8 bits is sufficient.
const uint8_t kProgramNumber = 0x01;

const uint8_t kStreamTypeH264 = 0x1B;
const uint8_t kStreamTypeAdtsAac = 0x0F;

// For all the pointer fields in the following PAT and PMTs, they are not really
// part of PAT or PMT but it's there so that TsPacket can point to a memory
// location that starts from pointer field.

const uint8_t kProgramAssociationTableId = 0x00;
const uint8_t kProgramMapTableId = 0x02;

// TODO(rkuroiwa):
// Once encryption is added, another PAT must be used for the encrypted portion
// e.g. version number set to 1.
// But this works for clear lead and for clear segments.
// Write PSI generator.
const uint8_t kPat[] = {
    0x00,  // pointer field
    kProgramAssociationTableId,
    0xB0,        // The last 2 '00' assumes that this PAT is not very long.
    0x0D,        // Length of the rest of this array.
    0x00, 0x00,  // Transport stream ID is 0.
    0xC1,        // version number 0, current next indicator 1.
    0x00,        // section number
    0x00,        // last section number
    // program number -> PMT PID mapping.
    0x00, 0x01,  // program number is 1.
    0xE0,        // first 3 bits is reserved.
    kPmtPid,
    // CRC32.
    0xF9, 0x62, 0xF5, 0x8B,
};

// Like PAT, with encryption different PMTs are required.
// It might make sense to add a PmtGenerator class.
const uint8_t kPmtH264[] = {
    0x00,  // pointer field
    kProgramMapTableId,
    0xB0,  // assumes length is <= 256 bytes.
    0x12,  // length of the rest of this array.
    0x00, kProgramNumber,
    0xC1,            // version 0, current next indicator 1.
    0x00,            // section number
    0x00,            // last section number.
    0xE0,            // first 3 bits reserved.
    kElementaryPid,  // PCR PID is the elementary streams PID.
    0xF0,            // first 4 bits reserved.
    0x00,            // No descriptor at this level.
    kStreamTypeH264, 0xE0, kElementaryPid,  // stream_type -> PID.
    0xF0, 0x00,                             // Es_info_length is 0.
    // CRC32.
    0x43, 0x49, 0x97, 0xBE,
};

const uint8_t kPmtAac[] = {
    0x00,  // pointer field
    0x02,  // table id must be 0x02.
    0xB0,  // assumes length is <= 256 bytes.
    0x12,  // length of the rest of this array.
    0x00, kProgramNumber,
    0xC1,            // version 0, current next indicator 1.
    0x00,            // section number
    0x00,            // last section number.
    0xE0,            // first 3 bits reserved.
    kElementaryPid,  // PCR PID is the elementary streams PID.
    0xF0,            // first 4 bits reserved.
    0x00,            // No descriptor at this level.
    kStreamTypeAdtsAac, 0xE0, kElementaryPid,  // stream_type -> PID.
    0xF0, 0x00,                                // Es_info_length is 0.
    // CRC32.
    0xE0, 0x6F, 0x1A, 0x31,
};

const bool kHasPcr = true;
const bool kPayloadUnitStartIndicator = true;

const uint8_t kSyncByte = 0x47;
const int kPcrFieldsSize = 6;

// This is the size of the first few fields in a TS packet, i.e. TS packet size
// without adaptation field or the payload.
const int kTsPacketHeaderSize = 4;
const int kTsPacketSize = 188;
const int kTsPacketMaximumPayloadSize =
    kTsPacketSize - kTsPacketHeaderSize;

const size_t kMaxPesPacketLengthValue = 0xFFFF;

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
static_assert(arraysize(kPaddingBytes) >= kTsPacketMaximumPayloadSize,
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
  int adaptation_field_length =
      kAdaptationFieldHeaderSize + (has_pcr ? kPcrFieldsSize : 0);
  if (remaining_data_size < kTsPacketMaximumPayloadSize) {
    const int current_ts_size = kTsPacketHeaderSize + remaining_data_size +
                                adaptation_field_length +
                                kAdaptationFieldLengthSize;
    if (current_ts_size < kTsPacketSize) {
      adaptation_field_length += kTsPacketSize - current_ts_size;
    }
  }

  writer->AppendInt(static_cast<uint8_t>(adaptation_field_length));
  int remaining_bytes = adaptation_field_length;
  writer->AppendInt(static_cast<uint8_t>(
      // All flags except PCR_flag are 0.
      static_cast<uint8_t>(has_pcr) << 4));
  remaining_bytes -= 1;

  if (has_pcr) {
    // program_clock_reference_extension = 0.
    const uint32_t most_significant_32bits_pcr =
        static_cast<uint32_t>(pcr_base >> 1);
    const uint16_t pcr_last_bit_reserved_and_pcr_extension =
        ((pcr_base & 1) << 15);
    writer->AppendInt(most_significant_32bits_pcr);
    writer->AppendInt(pcr_last_bit_reserved_and_pcr_extension);
    remaining_bytes -= kPcrFieldsSize;
  }
  DCHECK_GE(remaining_bytes, 0);
  if (remaining_bytes == 0)
    return;

  DCHECK_GE(static_cast<int>(arraysize(kPaddingBytes)), remaining_bytes);
  writer->AppendArray(kPaddingBytes, remaining_bytes);
}

// |payload| can be any payload. Most likely raw PSI tables or PES packet
// payload.
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

      const int write_bytes =
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

void WritePatPmtToBuffer(const uint8_t* data,
                         int data_size,
                         int pid,
                         ContinuityCounter* continuity_counter,
                         BufferWriter* writer) {
  WritePayloadToBufferWriter(data, data_size, kPayloadUnitStartIndicator, pid,
                             !kHasPcr, 0, continuity_counter, writer);
}

void WritePatToBuffer(const uint8_t* pat,
                      int pat_size,
                      ContinuityCounter* continuity_counter,
                      BufferWriter* writer) {
  const int kPatPid = 0;
  WritePatPmtToBuffer(pat, pat_size, kPatPid, continuity_counter, writer);
}

void WritePmtToBuffer(const uint8_t* pmt,
                      int pmt_size,
                      ContinuityCounter* continuity_counter,
                      BufferWriter* writer) {
  WritePatPmtToBuffer(pmt, pmt_size, kPmtPid, continuity_counter, writer);
}

// The only difference between writing PTS or DTS is the leading bits.
void WritePtsOrDts(uint8_t leading_bits,
                   uint64_t pts_or_dts,
                   BufferWriter* writer) {
  // First byte has 3 MSB of PTS.
  uint8_t first_byte =
      leading_bits << 4 | (((pts_or_dts >> 30) & 0x07) << 1) | 1;
  // Second byte has the next 8 bits of pts.
  uint8_t second_byte = (pts_or_dts >> 22) & 0xFF;
  // Third byte has the next 7 bits of pts followed by a marker bit.
  uint8_t third_byte = (((pts_or_dts >> 15) & 0x7F) << 1) | 1;
  // Fourth byte has the next 8 bits of pts.
  uint8_t fourth_byte = ((pts_or_dts >> 7) & 0xFF);
  // Fifth byte has the last 7 bits of pts followed by a marker bit.
  uint8_t fifth_byte = ((pts_or_dts & 0x7F) << 1) | 1;
  writer->AppendInt(first_byte);
  writer->AppendInt(second_byte);
  writer->AppendInt(third_byte);
  writer->AppendInt(fourth_byte);
  writer->AppendInt(fifth_byte);
}

bool WritePesToFile(const PesPacket& pes,
                    ContinuityCounter* continuity_counter,
                    File* file) {
  // The size of the length field.
  const int kAdaptationFieldLengthSize = 1;
  // The size of the flags field.
  const int kAdaptationFieldHeaderSize = 1;
  const int kPcrFieldSize = 6;
  const int kTsPacketMaxPayloadWithPcr =
      kTsPacketMaximumPayloadSize - kAdaptationFieldLengthSize -
      kAdaptationFieldHeaderSize - kPcrFieldSize;
  const uint64_t pcr_base = pes.has_dts() ? pes.dts() : pes.pts();
  const int pid = kElementaryPid;

  // This writer will hold part of PES packet after PES_packet_length field.
  BufferWriter pes_header_writer;
  // The first bit must be '10' for PES with video or audio stream id. The other
  // flags (bits) don't matter so they are 0.
  pes_header_writer.AppendInt(static_cast<uint8_t>(0x80));
  pes_header_writer.AppendInt(
      static_cast<uint8_t>(static_cast<int>(pes.has_pts()) << 7 |
                           static_cast<int>(pes.has_dts()) << 6
                           // Other fields are all 0.
                           ));
  uint8_t pes_header_data_length = 0;
  if (pes.has_pts())
    pes_header_data_length += 5;
  if (pes.has_dts())
    pes_header_data_length += 5;
  pes_header_writer.AppendInt(pes_header_data_length);

  if (pes.has_pts() && pes.has_dts()) {
    WritePtsOrDts(0x03, pes.pts(), &pes_header_writer);
    WritePtsOrDts(0x01, pes.dts(), &pes_header_writer);
  } else if (pes.has_pts()) {
    WritePtsOrDts(0x02, pes.pts(), &pes_header_writer);
  }

  // Put the first TS packet's payload into a buffer. This contains the PES
  // packet's header.
  BufferWriter first_ts_packet_buffer(kTsPacketSize);
  first_ts_packet_buffer.AppendNBytes(static_cast<uint64_t>(0x000001), 3);
  first_ts_packet_buffer.AppendInt(pes.stream_id());
  const size_t pes_packet_length = pes.data().size() + pes_header_writer.Size();
  first_ts_packet_buffer.AppendInt(static_cast<uint16_t>(
      pes_packet_length > kMaxPesPacketLengthValue ? 0 : pes_packet_length));
  first_ts_packet_buffer.AppendBuffer(pes_header_writer);

  const int available_payload =
      kTsPacketMaxPayloadWithPcr - first_ts_packet_buffer.Size();
  const int bytes_consumed =
      std::min(static_cast<int>(pes.data().size()), available_payload);
  first_ts_packet_buffer.AppendArray(pes.data().data(), bytes_consumed);

  BufferWriter output_writer;
  WritePayloadToBufferWriter(first_ts_packet_buffer.Buffer(),
                             first_ts_packet_buffer.Size(),
                             kPayloadUnitStartIndicator, pid, kHasPcr, pcr_base,
                             continuity_counter, &output_writer);

  const size_t remaining_pes_data_size = pes.data().size() - bytes_consumed;
  if (remaining_pes_data_size > 0) {
    WritePayloadToBufferWriter(pes.data().data() + bytes_consumed,
                               remaining_pes_data_size,
                               !kPayloadUnitStartIndicator, pid, !kHasPcr, 0,
                               continuity_counter, &output_writer);
  }
  return output_writer.WriteToFile(file).ok();
}

}  // namespace

ContinuityCounter::ContinuityCounter() {}
ContinuityCounter::~ContinuityCounter() {}

int ContinuityCounter::GetNext() {
  int ret = counter_;
  ++counter_;
  counter_ %= 16;
  return ret;
}

TsWriter::TsWriter() {}
TsWriter::~TsWriter() {}

bool TsWriter::Initialize(const StreamInfo& stream_info) {
  // This buffer will hold PMT data after section_length field so that this
  // can be used to get the section_length.
  time_scale_ = stream_info.time_scale();
  if (time_scale_ == 0) {
    LOG(ERROR) << "Timescale is 0.";
    return false;
  }
  const StreamType stream_type = stream_info.stream_type();
  if (stream_type != StreamType::kStreamVideo &&
      stream_type != StreamType::kStreamAudio) {
    LOG(ERROR) << "TsWriter cannot handle stream type " << stream_type
               << " yet.";
    return false;
  }

  const uint8_t* pmt = nullptr;
  size_t pmt_size = 0u;
  if (stream_info.stream_type() == StreamType::kStreamVideo) {
    const VideoStreamInfo& video_stream_info =
        static_cast<const VideoStreamInfo&>(stream_info);
    if (video_stream_info.codec() != VideoCodec::kCodecH264) {
      LOG(ERROR) << "TsWriter cannot handle video codec "
                 << video_stream_info.codec() << " yet.";
      return false;
    }
    pmt = kPmtH264;
    pmt_size = arraysize(kPmtH264);
  } else {
    DCHECK_EQ(stream_type, StreamType::kStreamAudio);
    const AudioStreamInfo& audio_stream_info =
        static_cast<const AudioStreamInfo&>(stream_info);
    if (audio_stream_info.codec() != AudioCodec::kCodecAAC) {
      LOG(ERROR) << "TsWriter cannot handle audio codec "
                 << audio_stream_info.codec() << " yet.";
      return false;
    }
    pmt = kPmtAac;
    pmt_size = arraysize(kPmtAac);
  }
  DCHECK(pmt);
  DCHECK_GT(pmt_size, 0u);

  // Most likely going to fit in 2 TS packets.
  BufferWriter psi_writer(kTsPacketSize * 2);
  WritePatToBuffer(kPat, arraysize(kPat), &pat_continuity_counter_,
                   &psi_writer);
  WritePmtToBuffer(pmt, pmt_size, &pmt_continuity_counter_, &psi_writer);

  psi_writer.SwapBuffer(&psi_ts_packets_);
  return true;
}

bool TsWriter::NewSegment(const std::string& file_name) {
  DCHECK(!psi_ts_packets_.empty());
  if (current_file_) {
    LOG(ERROR) << "File " << current_file_->file_name() << " still open.";
    return false;
  }
  current_file_.reset(File::Open(file_name.c_str(), "w"));
  if (!current_file_) {
    LOG(ERROR) << "Failed to open file " << file_name;
    return false;
  }

  // TODO(kqyang): Add WriteArrayToFile().
  BufferWriter psi_writer(psi_ts_packets_.size());
  psi_writer.AppendVector(psi_ts_packets_);
  if (!psi_writer.WriteToFile(current_file_.get()).ok()) {
    LOG(ERROR) << "Failed to write PSI to file.";
    return false;
  }

  return true;
}

bool TsWriter::FinalizeSegment() {
  return current_file_.release()->Close();
}

bool TsWriter::AddPesPacket(scoped_ptr<PesPacket> pes_packet) {
  if (time_scale_ == 0) {
    LOG(ERROR) << "Timescale is 0.";
    return false;
  }
  DCHECK(current_file_);
  if (!WritePesToFile(*pes_packet, &elementary_stream_continuity_counter_,
                      current_file_.get())) {
    LOG(ERROR) << "Failed to write pes to file.";
    return false;
  }

  // No need to keep pes_packet around so not passing it anywhere.
  return true;
}

}  // namespace mp2t
}  // namespace media
}  // namespace edash_packager
