// Copyright 2016 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/media/codecs/h26x_byte_to_unit_stream_converter.h>

#include <limits>

#include <absl/flags/flag.h>
#include <absl/log/check.h>
#include <absl/log/log.h>
#include <absl/strings/escaping.h>

#include <packager/macros/logging.h>
#include <packager/media/base/buffer_writer.h>
#include <packager/utils/bytes_to_string_view.h>

// TODO(kqyang): Move byte to unit stream convertion to muxer and make it a
// muxer option.
ABSL_FLAG(bool,
          strip_parameter_set_nalus,
          true,
          "When converting from NAL byte stream (AnnexB stream) to NAL unit "
          "stream, this flag determines whether to strip parameter sets NAL "
          "units, i.e. SPS/PPS for H264 and SPS/PPS/VPS for H265, from the "
          "frames. Note that avc1/hvc1 is generated if this flag is enabled; "
          "otherwise avc3/hev1 is generated.");

namespace shaka {
namespace media {

namespace {
// Additional space to reserve for output frame. This value ought to be enough
// to acommodate frames consisting of 100 NAL units with 3-byte start codes.
const size_t kStreamConversionOverhead = 100;
}  // namespace

H26xByteToUnitStreamConverter::H26xByteToUnitStreamConverter(
    Nalu::CodecType type)
    : type_(type),
      stream_format_(
          absl::GetFlag(FLAGS_strip_parameter_set_nalus)
              ? H26xStreamFormat::kNalUnitStreamWithoutParameterSetNalus
              : H26xStreamFormat::kNalUnitStreamWithParameterSetNalus) {}

H26xByteToUnitStreamConverter::H26xByteToUnitStreamConverter(
    Nalu::CodecType type,
    H26xStreamFormat stream_format)
    : type_(type), stream_format_(stream_format) {}

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

void H26xByteToUnitStreamConverter::WarnIfNotMatch(
    int nalu_type,
    const uint8_t* nalu_ptr,
    size_t nalu_size,
    const std::vector<uint8_t>& vector) {
  if (vector.empty())
    return;
  if (vector.size() != nalu_size ||
      memcmp(vector.data(), nalu_ptr, nalu_size) != 0) {
    LOG(WARNING) << "Seeing varying NAL unit of type " << nalu_type
                 << ". You may need to set --strip_parameter_set_nalus=false "
                    "during packaging to generate a playable stream.";
    VLOG(1) << "Old: "
            << absl::BytesToHexString(byte_vector_to_string_view(vector));
    VLOG(1) << "New: "
            << absl::BytesToHexString(
                   byte_array_to_string_view(nalu_ptr, nalu_size));
  }
}

}  // namespace media
}  // namespace shaka
