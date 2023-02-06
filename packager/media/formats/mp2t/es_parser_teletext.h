// Copyright 2020 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_FORMATS_MP2T_ES_PARSER_TELETEXT_H_
#define PACKAGER_MEDIA_FORMATS_MP2T_ES_PARSER_TELETEXT_H_

#include <unistd.h>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "packager/base/callback.h"
#include "packager/media/formats/mp2t/es_parser.h"

namespace shaka {
namespace media {
namespace mp2t {

class EsParserTeletext : public EsParser {
 public:
  EsParserTeletext(const uint32_t pid,
                   const NewStreamInfoCB& new_stream_info_cb,
                   const EmitTextSampleCB& emit_sample_cb,
                   const uint8_t* descriptor,
                   const size_t descriptor_length);

  EsParserTeletext(const EsParserTeletext&) = delete;
  EsParserTeletext& operator=(const EsParserTeletext&) = delete;

  bool Parse(const uint8_t* buf, int size, int64_t pts, int64_t dts) override;
  bool Flush() override;
  void Reset() override;

 private:
  struct TextBlock {
    std::vector<std::string> lines;
    int64_t pts;
  };

  bool ParseInternal(const uint8_t* data, const size_t size, const int64_t pts);
  bool ParseDataBlock(const int64_t pts,
                      const uint8_t* data_block,
                      const uint8_t packet_nr,
                      const uint8_t magazine,
                      std::string& display_text);
  void UpdateCharset();
  void SendPending(const uint16_t index, const int64_t pts);
  std::string BuildText(const uint8_t* data_block) const;

  NewStreamInfoCB new_stream_info_cb_;
  EmitTextSampleCB emit_sample_cb_;

  std::unordered_map<uint16_t, std::string> languages_;
  bool sent_info_ = false;
  uint8_t magazine_;
  uint8_t page_number_;
  std::unordered_map<uint16_t, TextBlock> page_state_;
  uint8_t charset_code_;
  char current_charset_[96][3];
  int64_t last_pts_;
};

}  // namespace mp2t
}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_FORMATS_MP2T_ES_PARSER_TELETEXT_H_
