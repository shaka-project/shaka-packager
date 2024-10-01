// Copyright 2020 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/formats/mp2t/es_parser_teletext.h"

#include "packager/media/base/bit_reader.h"
#include "packager/media/base/timestamp.h"
#include "packager/media/formats/mp2t/es_parser_teletext_tables.h"
#include "packager/media/formats/mp2t/mp2t_common.h"

namespace shaka {
namespace media {
namespace mp2t {

namespace {

constexpr const char* kRegionTeletextPrefix = "ttx_";

const uint8_t EBU_TELETEXT_WITH_SUBTITLING = 0x03;
const int kPayloadSize = 40;
const int kNumTriplets = 13;

template <typename T>
constexpr T bit(T value, const size_t bit_pos) {
  return (value >> bit_pos) & 0x1;
}

uint8_t ReadHamming(BitReader& reader) {
  uint8_t bits;
  RCHECK(reader.ReadBits(8, &bits));
  return TELETEXT_HAMMING_8_4[bits];
}

bool Hamming_24_18(const uint32_t value, uint32_t& out_result) {
  uint32_t result = value;

  uint8_t test = 0;
  for (uint8_t i = 0; i < 23; i++) {
    test ^= ((result >> i) & 0x01) * (i + 0x21);
  }
  test ^= ((result >> 0x17) & 0x01) * 0x20;

  if ((test & 0x1f) != 0x1f) {
    if ((test & 0x20) == 0x20) {
      return false;
    }
    result ^= 1 << (0x1e - test);
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
    uint8_t ignored_teletext_type;
    RCHECK(reader.ReadBits(5, &ignored_teletext_type));
    uint8_t magazine_number;
    RCHECK(reader.ReadBits(3, &magazine_number));
    if (magazine_number == 0) {
      magazine_number = 8;
    }

    uint8_t page_number_tens;
    RCHECK(reader.ReadBits(4, &page_number_tens));
    uint8_t page_number_units;
    RCHECK(reader.ReadBits(4, &page_number_units));
    const uint8_t page_number = page_number_tens * 10 + page_number_units;

    std::string lang(3, '\0');
    lang[0] = static_cast<char>((lang_code >> 16) & 0xff);
    lang[1] = static_cast<char>((lang_code >> 8) & 0xff);
    lang[2] = static_cast<char>((lang_code >> 0) & 0xff);

    const uint16_t index = magazine_number * 100 + page_number;
    result.emplace(index, std::move(lang));
  }

  return true;
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
      last_pts_(-1),
      last_end_pts_(-1),
      inside_sample_(false) {
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
    SendCueEnd(key, last_pts_);
  }

  return true;
}

void EsParserTeletext::Reset() {
  page_state_.clear();
  magazine_ = 0;
  page_number_ = 0;
  sent_info_ = false;
  charset_code_ = 0;
  inside_sample_ = false;
  UpdateCharset();
}

bool EsParserTeletext::ParseInternal(const uint8_t* data,
                                     const size_t size,
                                     const int64_t pts) {
  BitReader reader(data, size);
  RCHECK(reader.SkipBits(8));
  std::vector<TextRow> rows;

  while (reader.bits_available()) {
    uint8_t data_unit_id;
    RCHECK(reader.ReadBits(8, &data_unit_id));

    uint8_t data_unit_length;
    RCHECK(reader.ReadBits(8, &data_unit_length));

    if (data_unit_id != EBU_TELETEXT_WITH_SUBTITLING) {
      RCHECK(reader.SkipBytes(data_unit_length));
      continue;
    }

    if (data_unit_length != 44) {
      // Teletext data unit length is always 44 bytes
      LOG(ERROR) << "Bad Teletext data length";
      break;
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

    TextRow row;
    if (ParseDataBlock(pts, data_block, packet_nr, magazine, row)) {
      // LOG(INFO) << "pts=" << pts << " row=" << row.row_number
      //           << " text=" << row.fragment.body;
      rows.emplace_back(std::move(row));
    }
  }

  const uint16_t index = magazine_ * 100 + page_number_;
  if (rows.empty()) {
    SendCueEnd(index, last_pts_);
    return true;
  }

  auto page_state_itr = page_state_.find(index);
  if (page_state_itr == page_state_.end()) {
    page_state_.emplace(index, TextBlock{std::move(rows), {}, last_pts_});
  } else {
    for (auto& row : rows) {
      auto& page_state_lines = page_state_itr->second.rows;
      page_state_lines.emplace_back(std::move(row));
    }
    rows.clear();
  }
  SendStartedCue(index);
  return true;
}

bool EsParserTeletext::ParseDataBlock(const int64_t pts,
                                      const uint8_t* data_block,
                                      const uint8_t packet_nr,
                                      const uint8_t magazine,
                                      TextRow& row) {
  if (packet_nr == 0) {
    BitReader reader(data_block, 32);

    const uint8_t page_number_units = ReadHamming(reader);
    const uint8_t page_number_tens = ReadHamming(reader);
    if (page_number_units == 0xf || page_number_tens == 0xf) {
      RCHECK(reader.SkipBits(40));
      return false;
    }
    const uint8_t page_number = 10 * page_number_tens + page_number_units;
    const uint16_t index = magazine * 100 + page_number;

    last_pts_ = pts;  // This should ideally be done for each index.

    SendCueEnd(index, pts);

    page_number_ = page_number;
    magazine_ = magazine;

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

  inside_sample_ = true;
  const uint16_t index = magazine_ * 100 + page_number_;
  const auto page_state_itr = page_state_.find(index);
  if (page_state_itr != page_state_.cend()) {
    if (page_state_itr->second.rows.empty()) {
      const auto old_pts = page_state_itr->second.pts;
      if (pts != old_pts) {
        page_state_itr->second.pts = pts;
      }
    }
  }
  row = BuildRow(data_block, packet_nr);
  return true;
}

void EsParserTeletext::UpdateCharset() {
  memcpy(current_charset_, TELETEXT_CHARSET_G0_LATIN, sizeof(TELETEXT_CHARSET_G0_LATIN));
  if (charset_code_ > 7) {
    return;
  }
  const auto teletext_national_subset =
      static_cast<TELETEXT_NATIONAL_SUBSET>(charset_code_);

  switch (teletext_national_subset) {
    case TELETEXT_NATIONAL_SUBSET::ENGLISH:
      UpdateNationalSubset(TELETEXT_NATIONAL_SUBSET_ENGLISH);
      break;
    case TELETEXT_NATIONAL_SUBSET::FRENCH:
      UpdateNationalSubset(TELETEXT_NATIONAL_SUBSET_FRENCH);
      break;
    case TELETEXT_NATIONAL_SUBSET::SWEDISH_FINNISH_HUNGARIAN:
      UpdateNationalSubset(TELETEXT_NATIONAL_SUBSET_SWEDISH_FINNISH_HUNGARIAN);
      break;
    case TELETEXT_NATIONAL_SUBSET::CZECH_SLOVAK:
      UpdateNationalSubset(TELETEXT_NATIONAL_SUBSET_CZECH_SLOVAK);
      break;
    case TELETEXT_NATIONAL_SUBSET::GERMAN:
      UpdateNationalSubset(TELETEXT_NATIONAL_SUBSET_GERMAN);
      break;
    case TELETEXT_NATIONAL_SUBSET::PORTUGUESE_SPANISH:
      UpdateNationalSubset(TELETEXT_NATIONAL_SUBSET_PORTUGUESE_SPANISH);
      break;
    case TELETEXT_NATIONAL_SUBSET::ITALIAN:
      UpdateNationalSubset(TELETEXT_NATIONAL_SUBSET_ITALIAN);
      break;
    case TELETEXT_NATIONAL_SUBSET::NONE:
    default:
      break;
  }
}

// SendCueStart emits a text sample with body and ttx_cue_duration_placeholder
// since the duration is not yet known. More importantly, the role of the
// sample is set to kCueWithoutEnd.
void EsParserTeletext::SendStartedCue(const uint16_t index) {
  auto page_state_itr = page_state_.find(index);

  if (page_state_itr == page_state_.end()) {
    return;
  }

  if (page_state_itr->second.rows.empty()) {
    page_state_.erase(index);
    return;
  }

  inside_sample_ = true;

  const auto& pending_rows = page_state_itr->second.rows;
  const auto pts_start = page_state_itr->second.pts;
  const auto pts_end = pts_start + ttx_cue_duration_placeholder;

  TextSettings text_settings;
  std::shared_ptr<TextSample> text_sample;
  std::vector<TextFragment> sub_fragments;

  if (pending_rows.size() == 1) {
    // This is a single line of formatted text.
    // Propagate row number/2 and alignment
    const float line_nr = float(pending_rows[0].row_number) / 2.0;
    text_settings.line = TextNumber(line_nr, TextUnitType::kLines);
    text_settings.region = kRegionTeletextPrefix + std::to_string(int(line_nr));
    text_settings.text_alignment = pending_rows[0].alignment;
    text_sample = std::make_shared<TextSample>(
        "", pts_start, pts_end, text_settings, pending_rows[0].fragment,
        TextSampleRole::kCueWithoutEnd);
    text_sample->set_sub_stream_index(index);
    // LOG(INFO) << "send 1 row pts=" << pts_start;
    emit_sample_cb_.Run(text_sample);
    page_state_itr->second.rows.clear();  // Remove row, but keep pkt26 replacements
    // LOG(INFO) << "erased page_state single-row index=" << index;
    inside_sample_ = false;
    return;
  } else {
    int32_t latest_row_nr = -1;
    bool last_double_height = false;
    bool new_sample = true;
    for (const auto& row : pending_rows) {
      int row_nr = row.row_number;
      bool double_height = row.double_height;
      int row_step = last_double_height ? 2 : 1;
      if (latest_row_nr != -1) {  // Not the first row
        if (row_nr != latest_row_nr + row_step) {
          // Send what has been collected since not adjacent
          text_sample = std::make_shared<TextSample>(
              "", pts_start, pts_end, text_settings,
              TextFragment({}, sub_fragments), TextSampleRole::kCueWithoutEnd);
          text_sample->set_sub_stream_index(index);
          // LOG(INFO) << "send non-adjacent pts=" << pts_start;;
          emit_sample_cb_.Run(text_sample);
          new_sample = true;
        } else {
          // Add a newline and the next row to the current sample
          sub_fragments.push_back(TextFragment({}, true));
          sub_fragments.push_back(row.fragment);
          new_sample = false;
        }
      }
      if (new_sample) {
        const float line_nr = float(row.row_number) / 2.0;
        text_settings.line = TextNumber(line_nr, TextUnitType::kLines);
        text_settings.region =
            kRegionTeletextPrefix + std::to_string(int(line_nr));
        text_settings.text_alignment = row.alignment;
        sub_fragments.clear();
        sub_fragments.push_back(row.fragment);
      }
      last_double_height = double_height;
      latest_row_nr = row_nr;
    }
  }

  text_sample = std::make_shared<TextSample>(
      "", pts_start, pts_end, text_settings, TextFragment({}, sub_fragments),
      TextSampleRole::kCueWithoutEnd);
  text_sample->set_sub_stream_index(index);
  // LOG(INFO) << "send final cue pts=" << pts_start;
  emit_sample_cb_.Run(text_sample);

  page_state_itr->second.rows.clear();
  // LOG(INFO) << "clear rows, but keep packet26 page_state index=" << index;
}

// SendCueEnd emits a text sample with role kCueEnd to signal no data/cue end.
void EsParserTeletext::SendCueEnd(const uint16_t index, const int64_t pts_end) {
  if (last_pts_ == -1) {
    last_pts_ = pts_end;
    return;
  }
  if (pts_end == last_end_pts_) {
    return;
  }

  TextSettings text_settings;
  auto end_sample = std::make_shared<TextSample>(
      "", pts_end, pts_end, text_settings, TextFragment({}, ""),
      TextSampleRole::kCueEnd);
  end_sample->set_sub_stream_index(index);
  // LOG(INFO) << "for index=" << index << " send cue end at pts=" << pts_end;
  emit_sample_cb_.Run(end_sample);
  last_pts_ = pts_end;
  last_end_pts_ = pts_end;
  inside_sample_ = false;
}

// BuildRow builds a row with alignment information.
EsParserTeletext::TextRow EsParserTeletext::BuildRow(const uint8_t* data_block,
                                                     const uint8_t row) const {
  std::string next_string;
  next_string.reserve(kPayloadSize * 2);

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

  int32_t start_pos = 0;
  int32_t end_pos = 0;
  bool double_height = false;
  TextFragmentStyle text_style = TextFragmentStyle();
  text_style.color = "white";
  text_style.backgroundColor = "black";
  // A typical 40 character line looks like:
  // doubleHeight, [color] spaces, Start, Start, text, End End, spaces
  for (size_t i = 0; i < kPayloadSize; ++i) {
    if (column_replacement_map) {
      const auto column_itr = column_replacement_map->find(i);
      if (column_itr != column_replacement_map->cend()) {
        next_string.append(column_itr->second);
        continue;
      }
    }

    char next_char =
        static_cast<char>(TELETEXT_BITREVERSE_8[data_block[i]] & 0x7f);

    if (next_char < 0x20) {
      // Here are control characters, which are not printable.
      // These include colors, double-height, flashing, etc.
      // We only handle one-foreground color and double-height.
      switch (next_char) {
        case 0x0:  // Alpha Black (not included in Level 1.5)
          // color = ColorBlack
          break;
        case 0x1:
          text_style.color = "red";
          break;
        case 0x2:
          text_style.color = "green";
          break;
        case 0x3:
          text_style.color = "yellow";
          break;
        case 0x4:
          text_style.color = "blue";
          break;
        case 0x5:
          text_style.color = "magenta";
          break;
        case 0x6:
          text_style.color = "cyan";
          break;
        case 0x7:
          text_style.color = "white";
          break;
        case 0x08:  // Flash (not handled)
          break;
        case 0x09:  // Steady (not handled)
          break;
        case 0xa:  // End Box
          end_pos = i - 1;
          break;
        case 0xb:  // Start Box, typically twice due to double height
          start_pos = i + 1;
          continue;  // Do not propagate as a space
        case 0xc:  // Normal size
          break;
        case 0xd:  // Double height, typically always used
          double_height = true;
          break;
        case 0x1c:  // Black background (not handled)
          break;
        case 0x1d:  // Set background color from text color.
          text_style.backgroundColor = text_style.color;
          text_style.color = "black";  // Avoid having same as background
          break;
        default:
          // Rest of codes below 0x20 are not part of Level 1.5 or related to
          // mosaic graphics (non-text)
          break;
      }
      next_char =
          0x20;  // These characters result in a space if between start and end
    }
    if (start_pos == 0 || end_pos != 0) {  // Not between start and end
      continue;
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
  if (end_pos == 0) {
    end_pos = kPayloadSize - 1;
  }

  // Using start_pos and end_pos we approximated alignment of text
  // depending on the number of spaces to the left and right of the text.
  auto left_right_diff = start_pos - (kPayloadSize - 1 - end_pos);
  TextAlignment alignment;
  if (left_right_diff > 4) {
    alignment = TextAlignment::kRight;
  } else if (left_right_diff < -4) {
    alignment = TextAlignment::kLeft;
  } else {
    alignment = TextAlignment::kCenter;
  }
  const auto text_row = TextRow(
      {alignment, row, double_height, {TextFragment(text_style, next_string)}});

  return text_row;
}

void EsParserTeletext::ParsePacket26(const uint8_t* data_block) {
  const uint16_t index = magazine_ * 100 + page_number_;
  auto page_state_itr = page_state_.find(index);
  if (page_state_itr == page_state_.end()) {
    // LOG(INFO) << "packet26 create TextBlock pts=" << last_pts_;
    page_state_.emplace(index, TextBlock{{}, {}, last_pts_});
  }
  auto& replacement_map = page_state_[index].packet_26_replacements;

  uint8_t row = 0;

  std::vector<uint32_t> x26_triplets;
  x26_triplets.reserve(kNumTriplets);
  for (uint8_t i = 1; i < kPayloadSize; i += 3) {
    const uint32_t bytes = (TELETEXT_BITREVERSE_8[data_block[i + 2]] << 16) |
                           (TELETEXT_BITREVERSE_8[data_block[i + 1]] << 8) |
                           TELETEXT_BITREVERSE_8[data_block[i]];
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
      SetPacket26ReplacementString(replacement_map, row, address,
                                   reinterpret_cast<const char*>(
                                       TELETEXT_CHARSET_G2_LATIN[data - 0x20]));
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
              TELETEXT_G2_LATIN_ACCENTS[mode - 0x11][data - 0x41]));

    } else if (data >= 0x61 && data <= 0x7a) {
      SetPacket26ReplacementString(
          replacement_map, row, address,
          reinterpret_cast<const char*>(
              TELETEXT_G2_LATIN_ACCENTS[mode - 0x11][data - 0x47]));

    } else if ((data & 0x7f) >= 0x20) {
      SetPacket26ReplacementString(
          replacement_map, row, address,
          reinterpret_cast<const char*>(
              TELETEXT_CHARSET_G0_LATIN[(data & 0x7f) - 0x20]));
    }
  }
}

void EsParserTeletext::UpdateNationalSubset(
    const uint8_t national_subset[13][3]) {
  for (size_t i = 0; i < 13; ++i) {
    const size_t position = TELETEXT_NATIONAL_CHAR_INDEX_G0[i];
    memcpy(current_charset_[position], national_subset[i], 3);
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
