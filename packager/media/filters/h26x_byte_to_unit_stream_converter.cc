// Copyright 2016 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/filters/h26x_byte_to_unit_stream_converter.h"

#include <limits>

#include "packager/base/logging.h"
#include "packager/media/base/buffer_writer.h"

namespace shaka {
namespace media {

namespace {
// Additional space to reserve for output frame. This value ought to be enough
// to acommodate frames consisting of 100 NAL units with 3-byte start codes.
const size_t kStreamConversionOverhead = 100;
}

H26xByteToUnitStreamConverter::H26xByteToUnitStreamConverter(
    Nalu::CodecType type)
    : type_(type) {}
H26xByteToUnitStreamConverter::~H26xByteToUnitStreamConverter() {}

bool H26xByteToUnitStreamConverter::ConvertByteStreamToNalUnitStream(
    const uint8_t* input_frame,
    size_t input_frame_size,
    std::vector<uint8_t>* output_frame) {
  DCHECK(input_frame);
  DCHECK(output_frame);

  BufferWriter output_buffer(input_frame_size + kStreamConversionOverhead);

  Nalu nalu;
  NaluReader reader(type_, kIsAnnexbByteStream, input_frame, input_frame_size);
  if (!reader.StartsWithStartCode()) {
    LOG(ERROR) << "H.26x byte stream frame did not begin with start code.";
    return false;
  }

  while (reader.Advance(&nalu) == NaluReader::kOk) {
    const uint64_t nalu_size = nalu.payload_size() + nalu.header_size();
    DCHECK_LE(nalu_size, std::numeric_limits<uint32_t>::max());

    if (ProcessNalu(nalu))
      continue;

    // Append 4-byte length and NAL unit data to the buffer.
    output_buffer.AppendInt(static_cast<uint32_t>(nalu_size));
    output_buffer.AppendArray(nalu.data(), nalu_size);
  }

  output_buffer.SwapBuffer(output_frame);
  return true;
}

}  // namespace media
}  // namespace shaka

