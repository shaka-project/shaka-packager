// Copyright 2020 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/formats/mp2t/es_parser_teletext.h"

#include "packager/media/base/bit_reader.h"
#include "packager/media/base/text_stream_info.h"
#include "packager/media/base/timestamp.h"
#include "packager/media/formats/mp2t/es_parser_teletext_tables.h"
#include "packager/media/formats/mp2t/mp2t_common.h"

namespace shaka {
namespace media {
namespace mp2t {

namespace {

const uint8_t EBU_TELETEXT_WITH_SUBTITLING = 0x03;

template <typename T>
constexpr T bit(T value, const size_t bit_pos) {
  return (value >> bit_pos) & 0x1;
}

uint8_t ReadHamming(BitReader& reader) {
  uint8_t bits;
  RCHECK(reader.ReadBits(8, &bits));
  return HAMMING_8_4[bits];
}

bool Hamming_24_18(const uint32_t value, uint32_t& out_result) {
  uint32_t result = value;

  uint8_t test = 0;
  for (uint8_t i = 0; i < 23; i++) {
    test ^= ((result >> i) & 0x01) * (i + 33);
  }
  test ^= ((result >> 23) & 0x01) * 32;

  if ((test & 0x1f) != 0x1f) {
    if ((test & 0x20) == 0x20) {
      return false;
    }
    result ^= 1 << (30 - test);
  }

  out_result = (result & 0x000004) >> 2 | (result & 0x000070) >> 3 |
               (result & 0x007f00) >> 4 | (result & 0x7f0000) >> 5;
  return true;
}

bool ParseSubtitlingDescriptor(
    const uint8_t* descriptor,
    const size_t size,
    std::unordered_map<uint16_t, std::string>& result) {
  BitReader reader(descriptor, size);
  RCHECK(reader.SkipBits(8));

  size_t data_size;
  RCHECK(reader.ReadBits(8, &data_size));
  RCHECK(data_size + 2 <= size);

  for (size_t i = 0; i < data_size; i += 8) {
    uint32_t lang_code;
    RCHECK(reader.ReadBits(24, &lang_code));
    uint8_t teletext_type;
    RCHECK(reader.ReadBits(5, &teletext_type));
    uint8_t magazine_number;
    RCHECK(reader.ReadBits(3, &magazine_number));
    if (magazine_number == 0) {
      magazine_number = 8;
    }

    uint8_t page_tenths;
    RCHECK(reader.ReadBits(4, &page_tenths));
    uint8_t page_digit;
    RCHECK(reader.ReadBits(4, &page_digit));
    const uint8_t page_number = page_tenths * 10 + page_digit;

    std::string lang(3, '\0');
    lang[0] = static_cast<char>((lang_code >> 16) & 0xff);
    lang[1] = static_cast<char>((lang_code >> 8) & 0xff);
    lang[2] = static_cast<char>((lang_code >> 0) & 0xff);

    const uint16_t index = magazine_number * 100 + page_number;
    result.emplace(index, std::move(lang));
  }

  return true;
}

std::string RemoveTrailingSpaces(const std::string& input) {
  const auto index = input.find_last_not_of(' ');
  if (index == std::string::npos) {
    return "";
  }
  return input.substr(0, index + 1);
}

}  // namespace

EsParserTeletext::EsParserTeletext(const uint32_t pid,
                                   const NewStreamInfoCB& new_stream_info_cb,
                                   const EmitTextSampleCB& emit_sample_cb,
                                   const uint8_t* descriptor,
                                   const size_t descriptor_length)
    : EsParser(pid),
      new_stream_info_cb_(new_stream_info_cb),
      emit_sample_cb_(emit_sample_cb),
      magazine_(0),
      page_number_(0),
      charset_code_(0),
      current_charset_{},
      last_pts_(0) {
  if (!ParseSubtitlingDescriptor(descriptor, descriptor_length, languages_)) {
    LOG(ERROR) << "Unable to parse teletext_descriptor";
  }
  UpdateCharset();
}

bool EsParserTeletext::Parse(const uint8_t* buf,
                             int size,
                             int64_t pts,
                             int64_t dts) {
  if (!sent_info_) {
    sent_info_ = true;
    auto info = std::make_shared<TextStreamInfo>(pid(), kMpeg2Timescale,
                                                 kInfiniteDuration, kCodecText,
                                                 "", "", 0, 0, "");
    for (const auto& pair : languages_) {
      info->AddSubStream(pair.first, {pair.second});
    }

    new_stream_info_cb_.Run(info);
  }

  return ParseInternal(buf, size, pts);
}

bool EsParserTeletext::Flush() {
  std::vector<uint16_t> keys;
  for (const auto& entry : page_state_) {
    keys.push_back(entry.first);
  }

  for (const auto key : keys) {
    SendPending(key, last_pts_);
  }

  return true;
}

void EsParserTeletext::Reset() {
  page_state_.clear();
  magazine_ = 0;
  page_number_ = 0;
  sent_info_ = false;
  charset_code_ = 0;
  UpdateCharset();
}

bool EsParserTeletext::ParseInternal(const uint8_t* data,
                                     const size_t size,
                                     const int64_t pts) {
  BitReader reader(data, size);
  RCHECK(reader.SkipBits(8));
  std::vector<std::string> lines;

  while (reader.bits_available()) {
    uint8_t data_unit_id;
    RCHECK(reader.ReadBits(8, &data_unit_id));

    uint8_t data_unit_length;
    RCHECK(reader.ReadBits(8, &data_unit_length));

    if (data_unit_length != 44) {
      LOG(ERROR) << "Bad Teletext data length";
      break;
    }

    if (data_unit_id != EBU_TELETEXT_WITH_SUBTITLING) {
      RCHECK(reader.SkipBytes(44));
      continue;
    }

    RCHECK(reader.SkipBits(16));

    uint16_t address_bits;
    RCHECK(reader.ReadBits(16, &address_bits));

    uint8_t magazine = bit(address_bits, 14) + 2 * bit(address_bits, 12) +
                       4 * bit(address_bits, 10);

    if (magazine == 0) {
      magazine = 8;
    }

    const uint8_t packet_nr =
        (bit(address_bits, 8) + 2 * bit(address_bits, 6) +
         4 * bit(address_bits, 4) + 8 * bit(address_bits, 2) +
         16 * bit(address_bits, 0));
    const uint8_t* data_block = reader.current_byte_ptr();
    RCHECK(reader.SkipBytes(40));

    std::string display_text;
    if (ParseDataBlock(pts, data_block, packet_nr, magazine, display_text)) {
      lines.emplace_back(std::move(display_text));
    }
  }

  if (lines.empty()) {
    return true;
  }

  const uint16_t index = magazine_ * 100 + page_number_;
  auto page_state_itr = page_state_.find(index);
  if (page_state_itr == page_state_.end()) {
    page_state_.emplace(index, TextBlock{std::move(lines), {}, last_pts_});

  } else {
    for (auto& line : lines) {
      auto& page_state_lines = page_state_itr->second.lines;
      page_state_lines.emplace_back(std::move(line));
    }
    lines.clear();
  }

  return true;
}

bool EsParserTeletext::ParseDataBlock(const int64_t pts,
                                      const uint8_t* data_block,
                                      const uint8_t packet_nr,
                                      const uint8_t magazine,
                                      std::string& display_text) {
  if (packet_nr == 0) {
    last_pts_ = pts;
    BitReader reader(data_block, 32);

    const uint8_t page_number_units = ReadHamming(reader);
    const uint8_t page_number_tens = ReadHamming(reader);
    const uint8_t page_number = 10 * page_number_tens + page_number_units;

    const uint16_t index = magazine * 100 + page_number;
    SendPending(index, pts);

    page_number_ = page_number;
    magazine_ = magazine;
    if (page_number == 0xFF) {
      return false;
    }

    RCHECK(reader.SkipBits(40));
    const uint8_t subcode_c11_c14 = ReadHamming(reader);
    const uint8_t charset_code = subcode_c11_c14 >> 1;
    if (charset_code != charset_code_) {
      charset_code_ = charset_code;
      UpdateCharset();
    }

    return false;

  } else if (packet_nr == 26) {
    ParsePacket26(data_block);
    return false;

  } else if (packet_nr > 26) {
    return false;
  }

  display_text = BuildText(data_block, packet_nr);
  return true;
}

void EsParserTeletext::UpdateCharset() {
  memcpy(current_charset_, CHARSET_G0_LATIN, 96 * 3);
  switch (charset_code_) {
    case CHARSET_PORTUGUESE_SPANISH:
      for (size_t i = 0; i < 13; ++i) {
        const size_t position = NATIONAL_CHAR_INDEX_G0[i];
        memcpy(current_charset_[position], PORTUGUESE_SPANISH[i], 3);
      }
      break;
    default:
      break;
  }
}

void EsParserTeletext::SendPending(const uint16_t index, const int64_t pts) {
  auto page_state_itr = page_state_.find(index);

  if (page_state_itr == page_state_.end() ||
      page_state_itr->second.lines.empty()) {
    return;
  }

  const auto& pending_lines = page_state_itr->second.lines;
  const auto pending_pts = page_state_itr->second.pts;

  TextFragmentStyle text_fragment_style;
  TextSettings text_settings;
  std::shared_ptr<TextSample> text_sample;

  if (pending_lines.size() == 1) {
    TextFragment text_fragment(text_fragment_style, pending_lines[0].c_str());
    text_sample = std::make_shared<TextSample>("", pending_pts, pts,
                                               text_settings, text_fragment);

  } else {
    std::vector<TextFragment> sub_fragments;
    for (const auto& line : pending_lines) {
      sub_fragments.emplace_back(text_fragment_style, line.c_str());
      sub_fragments.emplace_back(text_fragment_style, true);
    }
    sub_fragments.pop_back();
    TextFragment text_fragment(text_fragment_style, sub_fragments);
    text_sample = std::make_shared<TextSample>("", pending_pts, pts,
                                               text_settings, text_fragment);
  }

  text_sample->set_sub_stream_index(index);
  emit_sample_cb_.Run(text_sample);

  page_state_.erase(index);
}

std::string EsParserTeletext::BuildText(const uint8_t* data_block,
                                        const uint8_t row) const {
  const size_t payload_len = 40;
  std::string next_string;
  next_string.reserve(payload_len * 2);
  bool leading_spaces = true;

  const uint16_t index = magazine_ * 100 + page_number_;
  const auto page_state_itr = page_state_.find(index);

  const std::unordered_map<uint8_t, std::string>* column_replacement_map =
      nullptr;
  if (page_state_itr != page_state_.cend()) {
    const auto row_itr =
        page_state_itr->second.packet_26_replacements.find(row);
    if (row_itr != page_state_itr->second.packet_26_replacements.cend()) {
      column_replacement_map = &(row_itr->second);
    }
  }

  for (size_t i = 0; i < payload_len; ++i) {
    if (column_replacement_map) {
      const auto column_itr = column_replacement_map->find(i);
      if (column_itr != column_replacement_map->cend()) {
        next_string.append(column_itr->second);
        leading_spaces = false;
        continue;
      }
    }

    char next_char = static_cast<char>(BITREVERSE_8[data_block[i]] & 0x7f);

    if (next_char < 32 || next_char > 127) {
      next_char = 0x20;
    }

    if (leading_spaces) {
      if (next_char == 0x20) {
        continue;
      }
      leading_spaces = false;
    }

    switch (next_char) {
      case '&':
        next_string.append("&amp;");
        break;
      case '<':
        next_string.append("&lt;");
        break;
      default: {
        const std::string replacement(current_charset_[next_char - 0x20]);
        next_string.append(replacement);
      } break;
    }
  }

  return RemoveTrailingSpaces(next_string);
}

void EsParserTeletext::ParsePacket26(const uint8_t* data_block) {
  const uint16_t index = magazine_ * 100 + page_number_;
  auto page_state_itr = page_state_.find(index);
  if (page_state_itr == page_state_.end()) {
    page_state_.emplace(index, TextBlock{{}, {}, last_pts_});
  }
  auto& replacement_map = page_state_[index].packet_26_replacements;

  uint8_t row = 0;
  const size_t payload_len = 40;

  std::vector<uint32_t> x26_triplets;
  x26_triplets.reserve(13);
  for (uint8_t i = 1; i < payload_len; i += 3) {
    const uint32_t bytes = (BITREVERSE_8[data_block[i + 2]] << 16) |
                           (BITREVERSE_8[data_block[i + 1]] << 8) |
                           BITREVERSE_8[data_block[i]];
    uint32_t triplet;
    if (Hamming_24_18(bytes, triplet)) {
      x26_triplets.emplace_back(triplet);
    }
  }

  for (const auto triplet : x26_triplets) {
    const uint8_t mode = (triplet & 0x7c0) >> 6;
    const uint8_t address = triplet & 0x3f;
    const uint8_t row_address_group = (address >= 0x28) && (address <= 0x3f);

    if ((mode == 0x4) && (row_address_group == 0x1)) {
      row = address - 0x28;
      if (row == 0x0) {
        row = 0x18;
      }
    }

    if (mode >= 0x11 && mode <= 0x1f && row_address_group == 0x1) {
      break;
    }

    const uint8_t data = (triplet & 0x3f800) >> 11;

    if (mode == 0x0f && row_address_group == 0x0 && data > 0x1f) {
      SetPacket26ReplacementString(
          replacement_map, row, address,
          reinterpret_cast<const char*>(CHARSET_G2_LATIN[data - 0x20]));
    }

    if (mode == 0x10 && row_address_group == 0x0 && data == 0x40) {
      SetPacket26ReplacementString(replacement_map, row, address, "@");
    }

    if (mode < 0x11 || mode > 0x1f || row_address_group != 0x0) {
      continue;
    }

    if (data >= 0x41 && data <= 0x5a) {
      SetPacket26ReplacementString(
          replacement_map, row, address,
          reinterpret_cast<const char*>(
              G2_LATIN_ACCENTS[mode - 0x11][data - 0x41]));

    } else if (data >= 0x61 && data <= 0x7a) {
      SetPacket26ReplacementString(
          replacement_map, row, address,
          reinterpret_cast<const char*>(
              G2_LATIN_ACCENTS[mode - 0x11][data - 0x47]));

    } else if ((data & 0x7f) >= 0x20) {
      SetPacket26ReplacementString(replacement_map, row, address,
                                   reinterpret_cast<const char*>(
                                       CHARSET_G0_LATIN[(data & 0x7f) - 0x20]));
    }
  }
}

void EsParserTeletext::SetPacket26ReplacementString(
    RowColReplacementMap& replacement_map,
    const uint8_t row,
    const uint8_t column,
    std::string&& replacement_string) {
  auto replacement_map_itr = replacement_map.find(row);
  if (replacement_map_itr == replacement_map.cend()) {
    replacement_map.emplace(row, std::unordered_map<uint8_t, std::string>{});
  }
  auto& column_map = replacement_map[row];
  column_map.emplace(column, std::move(replacement_string));
}

}  // namespace mp2t
}  // namespace media
}  // namespace shaka
