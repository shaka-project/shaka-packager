// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "media/filters/h264_byte_to_unit_stream_converter.h"

#include "base/logging.h"
#include "media/base/buffer_writer.h"
#include "media/filters/h264_parser.h"

namespace edash_packager {
namespace media {

namespace {
// Additional space to reserve for output frame. This value ought to be enough
// to acommodate frames consisting of 100 NAL units with 3-byte start codes.
const size_t kStreamConversionOverhead = 100;
}

H264ByteToUnitStreamConverter::H264ByteToUnitStreamConverter() {}

H264ByteToUnitStreamConverter::~H264ByteToUnitStreamConverter() {}

bool H264ByteToUnitStreamConverter::ConvertByteStreamToNalUnitStream(
    const uint8* input_frame,
    size_t input_frame_size,
    std::vector<uint8>* output_frame) {
  DCHECK(input_frame);
  DCHECK(output_frame);

  BufferWriter output_buffer(input_frame_size + kStreamConversionOverhead);

  const uint8* input_ptr(input_frame);
  const uint8* input_end(input_ptr + input_frame_size);
  off_t next_start_code_offset;
  off_t next_start_code_size;
  bool first_nalu(true);
  while (H264Parser::FindStartCode(input_ptr,
                                   input_end - input_ptr,
                                   &next_start_code_offset,
                                   &next_start_code_size)) {
    if (first_nalu) {
      if (next_start_code_offset != 0) {
        LOG(ERROR) << "H.264 byte stream frame did not begin with start code.";
        return false;
      }
      first_nalu = false;
    } else {
      ProcessNalu(input_ptr, next_start_code_offset, &output_buffer);
    }
    input_ptr += next_start_code_offset + next_start_code_size;
  }

  if (first_nalu) {
    LOG(ERROR) << "H.264 byte stream frame did not contain start codes.";
    return false;
  } else {
    ProcessNalu(input_ptr, input_end - input_ptr, &output_buffer);
  }

  output_buffer.SwapBuffer(output_frame);
  return true;
}

void H264ByteToUnitStreamConverter::ProcessNalu(
    const uint8* nalu_ptr,
    size_t nalu_size,
    BufferWriter* output_buffer) {
  DCHECK(nalu_ptr);
  DCHECK(output_buffer);

  if (!nalu_size)
    return;  // Edge case.

  uint8 nalu_type = *nalu_ptr & 0x0f;
  switch (nalu_type) {
    case H264NALU::kSPS:
      // Grab SPS NALU.
      last_sps_.assign(nalu_ptr, nalu_ptr + nalu_size);
      return;
    case H264NALU::kPPS:
      // Grab PPS NALU.
      last_pps_.assign(nalu_ptr, nalu_ptr + nalu_size);
      return;
    case H264NALU::kAUD:
      // Ignore AUD NALU.
      return;
    default:
      // Copy all other NALUs.
      break;
  }

  // Append 4-byte length and NAL unit data to the buffer.
  output_buffer->AppendInt(static_cast<uint32>(nalu_size));
  output_buffer->AppendArray(nalu_ptr, nalu_size);
}

bool H264ByteToUnitStreamConverter::GetAVCDecoderConfigurationRecord(
    std::vector<uint8>* decoder_config) {
  DCHECK(decoder_config);

  if ((last_sps_.size() < 4) || last_pps_.empty()) {
    // No data available to construct AVCDecoderConfigurationRecord.
    return false;
  }

  // Construct an AVCDecoderConfigurationRecord containing a single SPS and a
  // single PPS NALU. Please refer to ISO/IEC 14496-15 for format specifics.
  BufferWriter buffer(last_sps_.size() + last_pps_.size() + 11);
  uint8 version(1);
  buffer.AppendInt(version);
  buffer.AppendInt(last_sps_[1]);
  buffer.AppendInt(last_sps_[2]);
  buffer.AppendInt(last_sps_[3]);
  uint8 reserved_and_length_size_minus_one(0xff);
  buffer.AppendInt(reserved_and_length_size_minus_one);
  uint8 reserved_and_num_sps(0xe1);
  buffer.AppendInt(reserved_and_num_sps);
  buffer.AppendInt(static_cast<uint16>(last_sps_.size()));
  buffer.AppendVector(last_sps_);
  uint8 num_pps(1);
  buffer.AppendInt(num_pps);
  buffer.AppendInt(static_cast<uint16>(last_pps_.size()));
  buffer.AppendVector(last_pps_);
  buffer.SwapBuffer(decoder_config);

  return true;
}

}  // namespace media
}  // namespace edash_packager
