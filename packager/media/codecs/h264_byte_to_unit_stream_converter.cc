// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/codecs/h264_byte_to_unit_stream_converter.h"

#include <limits>

#include "packager/base/logging.h"
#include "packager/media/base/buffer_writer.h"
#include "packager/media/codecs/h264_parser.h"

namespace shaka {
namespace media {

H264ByteToUnitStreamConverter::H264ByteToUnitStreamConverter()
    : H26xByteToUnitStreamConverter(Nalu::kH264) {}

H264ByteToUnitStreamConverter::H264ByteToUnitStreamConverter(
    H26xStreamFormat stream_format)
    : H26xByteToUnitStreamConverter(Nalu::kH264, stream_format) {}

H264ByteToUnitStreamConverter::~H264ByteToUnitStreamConverter() {}

bool H264ByteToUnitStreamConverter::GetDecoderConfigurationRecord(
    std::vector<uint8_t>* decoder_config) const {
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

bool H264ByteToUnitStreamConverter::ProcessNalu(const Nalu& nalu) {
  DCHECK(nalu.data());

  // Skip the start code, but keep the 1-byte NALU type.
  const uint8_t* nalu_ptr = nalu.data();
  const uint64_t nalu_size = nalu.payload_size() + nalu.header_size();

  switch (nalu.type()) {
    case Nalu::H264_SPS:
      if (strip_parameter_set_nalus())
        WarnIfNotMatch(nalu.type(), nalu_ptr, nalu_size, last_sps_);
      // Grab SPS NALU.
      last_sps_.assign(nalu_ptr, nalu_ptr + nalu_size);
      return strip_parameter_set_nalus();
    case Nalu::H264_PPS:
      if (strip_parameter_set_nalus())
        WarnIfNotMatch(nalu.type(), nalu_ptr, nalu_size, last_pps_);
      // Grab PPS NALU.
      last_pps_.assign(nalu_ptr, nalu_ptr + nalu_size);
      return strip_parameter_set_nalus();
    case Nalu::H264_AUD:
      // Ignore AUD NALU.
      return true;
    default:
      // Have the base class handle other NALU types.
      return false;
  }
}

}  // namespace media
}  // namespace shaka
