// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/filters/h264_byte_to_unit_stream_converter.h"

#include <limits>

#include "packager/base/logging.h"
#include "packager/media/base/buffer_writer.h"
#include "packager/media/filters/h264_parser.h"

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
    const uint8_t* input_frame,
    size_t input_frame_size,
    std::vector<uint8_t>* output_frame) {
  DCHECK(input_frame);
  DCHECK(output_frame);

  BufferWriter output_buffer(input_frame_size + kStreamConversionOverhead);

  Nalu nalu;
  NaluReader reader(kIsAnnexbByteStream, input_frame, input_frame_size);
  if (!reader.StartsWithStartCode()) {
    LOG(ERROR) << "H.264 byte stream frame did not begin with start code.";
    return false;
  }
  while (reader.Advance(&nalu) == NaluReader::kOk) {
    ProcessNalu(nalu, &output_buffer);
  }

  output_buffer.SwapBuffer(output_frame);
  return true;
}

void H264ByteToUnitStreamConverter::ProcessNalu(const Nalu& nalu,
                                                BufferWriter* output_buffer) {
  DCHECK(nalu.data());
  DCHECK(output_buffer);

  // Skip the start code, but keep the 1-byte NALU type.
  const uint8_t* nalu_ptr = nalu.data() + nalu.header_size() - 1;
  const uint64_t nalu_size = nalu.data_size() + 1;
  DCHECK_LE(nalu_size, std::numeric_limits<uint32_t>::max());

  switch (nalu.type()) {
    case Nalu::H264_SPS:
      // Grab SPS NALU.
      last_sps_.assign(nalu_ptr, nalu_ptr + nalu_size);
      return;
    case Nalu::H264_PPS:
      // Grab PPS NALU.
      last_pps_.assign(nalu_ptr, nalu_ptr + nalu_size);
      return;
    case Nalu::H264_AUD:
      // Ignore AUD NALU.
      return;
    default:
      // Copy all other NALUs.
      break;
  }

  // Append 4-byte length and NAL unit data to the buffer.
  output_buffer->AppendInt(static_cast<uint32_t>(nalu_size));
  output_buffer->AppendArray(nalu_ptr, nalu_size);
}

bool H264ByteToUnitStreamConverter::GetAVCDecoderConfigurationRecord(
    std::vector<uint8_t>* decoder_config) {
  DCHECK(decoder_config);

  if ((last_sps_.size() < 4) || last_pps_.empty()) {
    // No data available to construct AVCDecoderConfigurationRecord.
    return false;
  }

  // Construct an AVCDecoderConfigurationRecord containing a single SPS and a
  // single PPS NALU. Please refer to ISO/IEC 14496-15 for format specifics.
  BufferWriter buffer(last_sps_.size() + last_pps_.size() + 11);
  uint8_t version(1);
  buffer.AppendInt(version);
  buffer.AppendInt(last_sps_[1]);
  buffer.AppendInt(last_sps_[2]);
  buffer.AppendInt(last_sps_[3]);
  uint8_t reserved_and_length_size_minus_one(0xff);
  buffer.AppendInt(reserved_and_length_size_minus_one);
  uint8_t reserved_and_num_sps(0xe1);
  buffer.AppendInt(reserved_and_num_sps);
  buffer.AppendInt(static_cast<uint16_t>(last_sps_.size()));
  buffer.AppendVector(last_sps_);
  uint8_t num_pps(1);
  buffer.AppendInt(num_pps);
  buffer.AppendInt(static_cast<uint16_t>(last_pps_.size()));
  buffer.AppendVector(last_pps_);
  buffer.SwapBuffer(decoder_config);

  return true;
}

}  // namespace media
}  // namespace edash_packager
