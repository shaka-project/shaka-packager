// Copyright 2020 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/media/formats/mp2t/es_parser_dvb.h>

#include <packager/media/base/bit_reader.h>
#include <packager/media/base/text_stream_info.h>
#include <packager/media/base/timestamp.h>
#include <packager/media/formats/mp2t/mp2t_common.h>

namespace shaka {
namespace media {
namespace mp2t {

namespace {

bool ParseSubtitlingDescriptor(
    const uint8_t* descriptor,
    size_t size,
    std::unordered_map<uint16_t, std::string>* langs) {
  // See ETSI EN 300 468 Section 6.2.41.
  BitReader reader(descriptor, size);
  size_t data_size;
  RCHECK(reader.SkipBits(8));  // descriptor_tag
  RCHECK(reader.ReadBits(8, &data_size));
  RCHECK(data_size + 2 <= size);
  for (size_t i = 0; i < data_size; i += 8) {
    uint32_t lang_code;
    uint16_t page;
    RCHECK(reader.ReadBits(24, &lang_code));
    RCHECK(reader.SkipBits(8));  // subtitling_type
    RCHECK(reader.ReadBits(16, &page));
    RCHECK(reader.SkipBits(16));  // ancillary_page_id

    // The lang code is a ISO 639-2 code coded in Latin-1.
    std::string lang(3, '\0');
    lang[0] = (lang_code >> 16) & 0xff;
    lang[1] = (lang_code >> 8) & 0xff;
    lang[2] = (lang_code >> 0) & 0xff;
    langs->emplace(page, std::move(lang));
  }
  return true;
}

}  // namespace

EsParserDvb::EsParserDvb(uint32_t pid,
                         const NewStreamInfoCB& new_stream_info_cb,
                         const EmitTextSampleCB& emit_sample_cb,
                         const uint8_t* descriptor,
                         size_t descriptor_length)
    : EsParser(pid),
      new_stream_info_cb_(new_stream_info_cb),
      emit_sample_cb_(emit_sample_cb) {
  if (!ParseSubtitlingDescriptor(descriptor, descriptor_length, &languages_)) {
    LOG(WARNING) << "Error parsing subtitling descriptor";
  }
}

EsParserDvb::~EsParserDvb() {}

bool EsParserDvb::Parse(const uint8_t* buf,
                        int size,
                        int64_t pts,
                        int64_t dts) {
  if (!sent_info_) {
    sent_info_ = true;
    std::shared_ptr<TextStreamInfo> info = std::make_shared<TextStreamInfo>(
        pid(), kMpeg2Timescale, kInfiniteDuration, kCodecText,
        /* codec_string= */ "", /* codec_config= */ "", /* width= */ 0,
        /* height= */ 0, /* language= */ "");
    for (const auto& pair : languages_) {
      info->AddSubStream(pair.first, {pair.second});
    }

    new_stream_info_cb_(info);
  }

  // TODO: Handle buffering and multiple reads?  All content so far has been
  // a whole segment, so it may not be needed.
  return ParseInternal(buf, size, pts);
}

bool EsParserDvb::Flush() {
  for (auto& pair : parsers_) {
    std::vector<std::shared_ptr<TextSample>> samples;
    RCHECK(pair.second.Flush(&samples));

    for (auto sample : samples) {
      sample->set_sub_stream_index(pair.first);
      emit_sample_cb_(sample);
    }
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
    for (auto sample : samples) {
      sample->set_sub_stream_index(page_id);
      emit_sample_cb_(sample);
    }

    RCHECK(reader.SkipBytes(segment_length));
  }
  return temp == 0xff;
}

}  // namespace mp2t
}  // namespace media
}  // namespace shaka
