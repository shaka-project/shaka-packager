// Copyright 2016 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/formats/mp4/video_slice_header_parser.h"

#include "packager/media/base/rcheck.h"
#include "packager/media/filters/avc_decoder_configuration.h"
#include "packager/media/filters/hevc_decoder_configuration.h"

namespace shaka {
namespace media {
namespace mp4 {

namespace {

int NumBitsToNumBytes(int size_in_bits) {
  // Round-up division.
  DCHECK_GE(size_in_bits, 0);
  return (size_in_bits - 1) / 8 + 1;
}

}  // namespace

H264VideoSliceHeaderParser::H264VideoSliceHeaderParser() {}
H264VideoSliceHeaderParser::~H264VideoSliceHeaderParser() {}

bool H264VideoSliceHeaderParser::Initialize(
    const std::vector<uint8_t>& decoder_configuration) {
  AVCDecoderConfiguration config;
  RCHECK(config.Parse(decoder_configuration));

  for (size_t i = 0; i < config.nalu_count(); i++) {
    int id;
    const Nalu& nalu = config.nalu(i);
    if (nalu.type() == Nalu::H264_SPS) {
      RCHECK(parser_.ParseSps(nalu, &id) == H264Parser::kOk);
    } else {
      DCHECK_EQ(Nalu::H264_PPS, nalu.type());
      RCHECK(parser_.ParsePps(nalu, &id) == H264Parser::kOk);
    }
  }

  return true;
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
  HEVCDecoderConfiguration hevc_config;
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

int64_t H265VideoSliceHeaderParser::GetHeaderSize(const Nalu& nalu) {
  DCHECK(nalu.is_video_slice());
  H265SliceHeader slice_header;
  if (parser_.ParseSliceHeader(nalu, &slice_header) != H265Parser::kOk)
    return -1;

  return NumBitsToNumBytes(slice_header.header_bit_size);
}

}  // namespace mp4
}  // namespace media
}  // namespace shaka

