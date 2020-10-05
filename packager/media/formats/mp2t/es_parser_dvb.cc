// Copyright 2020 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/formats/mp2t/es_parser_dvb.h"

#include "packager/media/base/bit_reader.h"
#include "packager/media/base/text_stream_info.h"
#include "packager/media/base/timestamp.h"
#include "packager/media/formats/mp2t/mp2t_common.h"

namespace shaka {
namespace media {
namespace mp2t {

EsParserDvb::EsParserDvb(uint32_t pid,
                         const NewStreamInfoCB& new_stream_info_cb,
                         const EmitTextSampleCB& emit_sample_cb)
    : EsParser(pid),
      new_stream_info_cb_(new_stream_info_cb),
      emit_sample_cb_(emit_sample_cb) {}

EsParserDvb::~EsParserDvb() {}

bool EsParserDvb::Parse(const uint8_t* buf,
                        int size,
                        int64_t pts,
                        int64_t dts) {
  if (!sent_info_) {
    sent_info_ = true;
    std::shared_ptr<StreamInfo> info = std::make_shared<TextStreamInfo>(
        pid(), kMpeg2Timescale, kInfiniteDuration, kCodecText,
        /* codec_string= */ "", /* codec_config= */ "", /* width= */ 0,
        /* height= */ 0, /* language= */ "");
    new_stream_info_cb_.Run(info);
  }

  // TODO: Handle buffering and multiple reads?  All content so far has been
  // a whole segment, so it may not be needed.
  return ParseInternal(buf, size, pts);
}

bool EsParserDvb::Flush() {
  for (auto& pair : parsers_) {
    std::vector<std::shared_ptr<TextSample>> samples;
    RCHECK(pair.second.Flush(&samples));

    for (auto sample : samples)
      emit_sample_cb_.Run(sample);
  }
  return true;
}

void EsParserDvb::Reset() {
  parsers_.clear();
}

bool EsParserDvb::ParseInternal(const uint8_t* data, size_t size, int64_t pts) {
  // See EN 300 743 Table 3.
  BitReader reader(data, size);
  int data_identifier;
  int subtitle_stream_id;
  RCHECK(reader.ReadBits(8, &data_identifier));
  RCHECK(reader.ReadBits(8, &subtitle_stream_id));
  RCHECK(data_identifier == 0x20);
  RCHECK(subtitle_stream_id == 0);

  int temp;
  while (reader.ReadBits(8, &temp) && temp == 0xf) {
    DvbSubSegmentType segment_type;
    uint16_t page_id;
    size_t segment_length;
    RCHECK(reader.ReadBits(8, &segment_type));
    RCHECK(reader.ReadBits(16, &page_id));
    RCHECK(reader.ReadBits(16, &segment_length));
    RCHECK(reader.bits_available() > segment_length * 8);

    const uint8_t* payload = data + (size - reader.bits_available() / 8);
    std::vector<std::shared_ptr<TextSample>> samples;
    RCHECK(parsers_[page_id].Parse(segment_type, pts, payload, segment_length,
                                   &samples));
    for (auto sample : samples)
      emit_sample_cb_.Run(sample);

    RCHECK(reader.SkipBytes(segment_length));
  }
  return temp == 0xff;
}

}  // namespace mp2t
}  // namespace media
}  // namespace shaka
