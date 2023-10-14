// Copyright 2016 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/media/codecs/video_slice_header_parser.h>

#include <absl/log/check.h>

#include <packager/macros/logging.h>
#include <packager/media/base/rcheck.h>
#include <packager/media/codecs/avc_decoder_configuration_record.h>
#include <packager/media/codecs/hevc_decoder_configuration_record.h>

namespace shaka {
namespace media {

namespace {

size_t NumBitsToNumBytes(size_t size_in_bits) {
  // Round-up division.
  return (size_in_bits + 7) >> 3;
}

}  // namespace

H264VideoSliceHeaderParser::H264VideoSliceHeaderParser() {}
H264VideoSliceHeaderParser::~H264VideoSliceHeaderParser() {}

bool H264VideoSliceHeaderParser::Initialize(
    const std::vector<uint8_t>& decoder_configuration) {
  AVCDecoderConfigurationRecord config;
  RCHECK(config.Parse(decoder_configuration));

  for (size_t i = 0; i < config.nalu_count(); i++) {
    int id;
    const Nalu& nalu = config.nalu(i);
    if (nalu.type() == Nalu::H264_SPS) {
      RCHECK(parser_.ParseSps(nalu, &id) == H264Parser::kOk);
    } else if (nalu.type() == Nalu::H264_PPS) {
      RCHECK(parser_.ParsePps(nalu, &id) == H264Parser::kOk);
    }
  }

  return true;
}

bool H264VideoSliceHeaderParser::ProcessNalu(const Nalu& nalu) {
  int id;
  switch (nalu.type()) {
    case Nalu::H264_SPS:
      return parser_.ParseSps(nalu, &id) == H264Parser::kOk;
    case Nalu::H264_PPS:
      return parser_.ParsePps(nalu, &id) == H264Parser::kOk;
    default:
      return true;
  }
}

int64_t H264VideoSliceHeaderParser::GetHeaderSize(const Nalu& nalu) {
  DCHECK(nalu.is_video_slice());
  H264SliceHeader slice_header;
  if (parser_.ParseSliceHeader(nalu, &slice_header) != H264Parser::kOk)
    return -1;

  return NumBitsToNumBytes(slice_header.header_bit_size);
}

H265VideoSliceHeaderParser::H265VideoSliceHeaderParser() {}
H265VideoSliceHeaderParser::~H265VideoSliceHeaderParser() {}

bool H265VideoSliceHeaderParser::Initialize(
    const std::vector<uint8_t>& decoder_configuration) {
  int id;
  HEVCDecoderConfigurationRecord hevc_config;
  RCHECK(hevc_config.Parse(decoder_configuration));

  for (size_t i = 0; i < hevc_config.nalu_count(); i++) {
    const Nalu& nalu = hevc_config.nalu(i);
    if (nalu.type() == Nalu::H265_SPS) {
      RCHECK(parser_.ParseSps(nalu, &id) == H265Parser::kOk);
    } else if (nalu.type() == Nalu::H265_PPS) {
      RCHECK(parser_.ParsePps(nalu, &id) == H265Parser::kOk);
    } else if (nalu.type() == Nalu::H265_VPS) {
      // Ignore since it does not affect video slice header parsing.
    } else {
      VLOG(1) << "Ignoring decoder configuration Nalu of unknown type "
              << nalu.type();
    }
  }

  return true;
}

bool H265VideoSliceHeaderParser::ProcessNalu(const Nalu& nalu) {
  int id;
  switch (nalu.type()) {
    case Nalu::H265_SPS:
      return parser_.ParseSps(nalu, &id) == H265Parser::kOk;
    case Nalu::H265_PPS:
      return parser_.ParsePps(nalu, &id) == H265Parser::kOk;
    case Nalu::H265_VPS:
      // Ignore since it does not affect video slice header parsing.
      return true;
    default:
      return true;
  }
}

int64_t H265VideoSliceHeaderParser::GetHeaderSize(const Nalu& nalu) {
  DCHECK(nalu.is_video_slice());
  H265SliceHeader slice_header;
  if (parser_.ParseSliceHeader(nalu, &slice_header) != H265Parser::kOk)
    return -1;

  return NumBitsToNumBytes(slice_header.header_bit_size);
}

}  // namespace media
}  // namespace shaka

