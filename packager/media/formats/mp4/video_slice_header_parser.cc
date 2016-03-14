// Copyright 2016 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/formats/mp4/video_slice_header_parser.h"

#include "packager/media/filters/avc_decoder_configuration.h"
#include "packager/media/formats/mp4/rcheck.h"

namespace edash_packager {
namespace media {
namespace mp4 {

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

  // Round-up to bytes.
  return (slice_header.header_bit_size - 1) / 8 + 1;
}

}  // namespace mp4
}  // namespace media
}  // namespace edash_packager

