// Copyright 2016 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/filters/nal_unit_to_byte_stream_converter.h"

#include <list>

#include "packager/base/logging.h"
#include "packager/media/base/bit_reader.h"
#include "packager/media/base/buffer_reader.h"
#include "packager/media/base/buffer_writer.h"
#include "packager/media/base/macros.h"
#include "packager/media/filters/avc_decoder_configuration.h"
#include "packager/media/filters/nalu_reader.h"

namespace edash_packager {
namespace media {

namespace {

const uint8_t kNaluStartCode[] = {0x00, 0x00, 0x00, 0x01};

const uint8_t kEmulationPreventionByte = 0x03;

const uint8_t kAccessUnitDelimiterRbspAnyPrimaryPicType = 0xF0;

// Inserts emulation byte where necessary.
void EscapeRawByteSequencePayload(const uint8_t* input,
                                  size_t input_size,
                                  BufferWriter* output_writer) {
  // Keep track of consecutive zeros that it has seen (not including the current
  // byte), so that the algorithm doesn't need to go back to check the same
  // bytes.
  int consecutive_zero_count = 0;
  for (size_t i = 0; i < input_size; ++i) {
    if (consecutive_zero_count <= 1) {
      output_writer->AppendInt(input[i]);
    } else if (consecutive_zero_count == 2) {
      if (input[i] == 0 || input[i] == 1 || input[i] == 2 || input[i] == 3) {
        // Must be escaped.
        output_writer->AppendInt(kEmulationPreventionByte);
      }
      output_writer->AppendInt(input[i]);
      // Note that input[i] can be 0.
      // 00 00 00 00 00 00 should become
      // 00 00 03 00 00 03 00 00 03
      // So consecutive_zero_count is reset here and incremented below if
      // input[i] is 0.
      consecutive_zero_count = 0;
    }

    consecutive_zero_count = input[i] == 0 ? consecutive_zero_count + 1 : 0;
  }

  // ISO 14496-10 Section 7.4.1.1 mentions that if the last byte is 0 (which
  // only happens if RBSP has cabac_zero_word), 0x03 must be appended.
  if (consecutive_zero_count > 0) {
    DCHECK_GT(input_size, 0u);
    DCHECK_EQ(input[input_size - 1], 0u);
    output_writer->AppendInt(kEmulationPreventionByte);
  }
}

void AppendNalu(const Nalu& nalu,
                int nalu_length_size,
                bool escape_data,
                BufferWriter* buffer_writer) {
  if (escape_data) {
    EscapeRawByteSequencePayload(
        nalu.data(), nalu.header_size() + nalu.payload_size(), buffer_writer);
  } else {
    buffer_writer->AppendArray(nalu.data(),
                               nalu.header_size() + nalu.payload_size());
  }
}

void AddAccessUnitDelimiter(BufferWriter* buffer_writer) {
  buffer_writer->AppendInt(static_cast<uint8_t>(Nalu::H264_AUD));
  // For now, primary_pic_type is 7 which is "anything".
  buffer_writer->AppendInt(kAccessUnitDelimiterRbspAnyPrimaryPicType);
}

}  // namespace

NalUnitToByteStreamConverter::NalUnitToByteStreamConverter()
    : nalu_length_size_(0), escape_data_(false) {}
NalUnitToByteStreamConverter::~NalUnitToByteStreamConverter() {}

bool NalUnitToByteStreamConverter::Initialize(
    const uint8_t* decoder_configuration_data,
    size_t decoder_configuration_data_size,
    bool escape_data) {
  escape_data_ = escape_data;
  if (!decoder_configuration_data || decoder_configuration_data_size == 0) {
    LOG(ERROR) << "Decoder conguration is empty.";
    return false;
  }

  AVCDecoderConfiguration decoder_config;
  if (!decoder_config.Parse(std::vector<uint8_t>(
          decoder_configuration_data,
          decoder_configuration_data + decoder_configuration_data_size))) {
    return false;
  }

  if (decoder_config.nalu_count() < 2) {
    LOG(ERROR) << "Cannot find SPS or PPS.";
    return false;
  }

  nalu_length_size_ = decoder_config.nalu_length_size();

  BufferWriter buffer_writer(decoder_configuration_data_size);
  bool found_sps = false;
  bool found_pps = false;
  for (uint32_t i = 0; i < decoder_config.nalu_count(); ++i) {
    const Nalu& nalu = decoder_config.nalu(i);
    if (nalu.type() == Nalu::H264NaluType::H264_SPS) {
      buffer_writer.AppendArray(kNaluStartCode, arraysize(kNaluStartCode));
      AppendNalu(nalu, nalu_length_size_, escape_data, &buffer_writer);
      found_sps = true;
    } else if (nalu.type() == Nalu::H264NaluType::H264_PPS) {
      buffer_writer.AppendArray(kNaluStartCode, arraysize(kNaluStartCode));
      AppendNalu(nalu, nalu_length_size_, escape_data, &buffer_writer);
      found_pps = true;
    }
  }
  if (!found_sps || !found_pps) {
    LOG(ERROR) << "Failed to find SPS or PPS.";
    return false;
  }

  buffer_writer.SwapBuffer(&decoder_configuration_in_byte_stream_);
  return true;
}

// This ignores all AUD, SPS, and PPS in the sample. Instead uses the data
// parsed in Initialize().
bool NalUnitToByteStreamConverter::ConvertUnitToByteStream(
    const uint8_t* sample,
    size_t sample_size,
    bool is_key_frame,
    std::vector<uint8_t>* output) {
  if (!sample || sample_size == 0) {
    LOG(WARNING) << "Sample is empty.";
    return true;
  }

  BufferWriter buffer_writer(sample_size);
  buffer_writer.AppendArray(kNaluStartCode, arraysize(kNaluStartCode));
  AddAccessUnitDelimiter(&buffer_writer);
  if (is_key_frame)
    buffer_writer.AppendVector(decoder_configuration_in_byte_stream_);

  NaluReader nalu_reader(NaluReader::kH264, nalu_length_size_, sample,
                         sample_size);
  Nalu nalu;
  NaluReader::Result result = nalu_reader.Advance(&nalu);

  while (result == NaluReader::kOk) {
    switch (nalu.type()) {
      case Nalu::H264_AUD:
        FALLTHROUGH_INTENDED;
      case Nalu::H264_SPS:
        FALLTHROUGH_INTENDED;
      case Nalu::H264_PPS:
        break;
      default:
        buffer_writer.AppendArray(kNaluStartCode, arraysize(kNaluStartCode));
        AppendNalu(nalu, nalu_length_size_, escape_data_, &buffer_writer);
        break;
    }
    result = nalu_reader.Advance(&nalu);
  }

  DCHECK_NE(result, NaluReader::kOk);
  if (result != NaluReader::kEOStream) {
    LOG(ERROR) << "Stopped reading before end of stream.";
    return false;
  }

  buffer_writer.SwapBuffer(output);
  return true;
}

}  // namespace media
}  // namespace edash_packager
