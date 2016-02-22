// Copyright 2016 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/formats/mp4/video_slice_header_parser.h"

#include "packager/media/formats/mp4/rcheck.h"
#include "packager/media/base/buffer_reader.h"

namespace edash_packager {
namespace media {
namespace mp4 {

H264VideoSliceHeaderParser::H264VideoSliceHeaderParser() {}
H264VideoSliceHeaderParser::~H264VideoSliceHeaderParser() {}

bool H264VideoSliceHeaderParser::Initialize(
    const std::vector<uint8_t>& decoder_configuration) {
  // See ISO 14496-15 sec 5.3.3.1.2
  BufferReader reader(decoder_configuration.data(),
                      decoder_configuration.size());
  RCHECK(reader.SkipBytes(5));

  uint8_t sps_count;
  RCHECK(reader.Read1(&sps_count));
  sps_count = sps_count & 0x1f;

  for (size_t i = 0; i < sps_count; i++) {
    uint16_t size;
    RCHECK(reader.Read2(&size));
    const uint8_t* data = reader.data() + reader.pos();
    RCHECK(reader.SkipBytes(size));

    int id;
    Nalu nalu;
    RCHECK(nalu.InitializeFromH264(data, size));
    RCHECK(parser_.ParseSPS(nalu, &id) == H264Parser::kOk);
  }

  uint8_t pps_count;
  RCHECK(reader.Read1(&pps_count));
  for (size_t i = 0; i < pps_count; i++) {
    uint16_t size;
    RCHECK(reader.Read2(&size));
    const uint8_t* data = reader.data() + reader.pos();
    RCHECK(reader.SkipBytes(size));

    int id;
    Nalu nalu;
    RCHECK(nalu.InitializeFromH264(data, size));
    RCHECK(parser_.ParsePPS(nalu, &id) == H264Parser::kOk);
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

