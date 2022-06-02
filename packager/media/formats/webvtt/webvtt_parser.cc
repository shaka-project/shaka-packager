// Copyright 2017 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/formats/webvtt/webvtt_parser.h"

#include "packager/base/logging.h"
#include "packager/base/strings/string_number_conversions.h"
#include "packager/base/strings/string_split.h"
#include "packager/base/strings/string_util.h"
#include "packager/media/base/text_stream_info.h"
#include "packager/media/formats/webvtt/webvtt_utils.h"

namespace shaka {
namespace media {
namespace {

const uint64_t kStreamIndex = 0;

std::string BlockToString(const std::string* block, size_t size) {
  std::string out = " --- BLOCK START ---\n";

  for (size_t i = 0; i < size; i++) {
    out.append("    ");
    out.append(block[i]);
    out.append("\n");
  }

  out.append(" --- BLOCK END ---");

  return out;
}

// Comments are just blocks that are preceded by a blank line, start with the
// word "NOTE" (followed by a space or newline), and end at the first blank
// line.
// SOURCE: https://www.w3.org/TR/webvtt1
bool IsLikelyNote(const std::string& line) {
  return line == "NOTE" ||
         base::StartsWith(line, "NOTE ", base::CompareCase::SENSITIVE) ||
         base::StartsWith(line, "NOTE\t", base::CompareCase::SENSITIVE);
}

// As cue time is the only part of a WEBVTT file that is allowed to have
// "-->" appear, then if the given line contains it, we can safely assume
// that the line is likely to be a cue time.
bool IsLikelyCueTiming(const std::string& line) {
  return line.find("-->") != std::string::npos;
}

// A WebVTT cue identifier is any sequence of one or more characters not
// containing the substring "-->" (U+002D HYPHEN-MINUS, U+002D HYPHEN-MINUS,
// U+003E GREATER-THAN SIGN), nor containing any U+000A LINE FEED (LF)
// characters or U+000D CARRIAGE RETURN (CR) characters.
// SOURCE: https://www.w3.org/TR/webvtt1/#webvtt-cue-identifier
bool MaybeCueId(const std::string& line) {
  return line.find("-->") == std::string::npos;
}

// Check to see if the block is likely a style block. Style blocks are
// identified as any block that starts with a line that only contains
// "STYLE".
// SOURCE: https://w3c.github.io/webvtt/#styling
bool IsLikelyStyle(const std::string& line) {
  return base::TrimWhitespaceASCII(line, base::TRIM_TRAILING) == "STYLE";
}

// Check to see if the block is likely a region block. Region blocks are
// identified as any block that starts with a line that only contains
// "REGION".
// SOURCE: https://w3c.github.io/webvtt/#webvtt-region
bool IsLikelyRegion(const std::string& line) {
  return base::TrimWhitespaceASCII(line, base::TRIM_TRAILING) == "REGION";
}

bool ParsePercent(const std::string& str, float* value) {
  // https://www.w3.org/TR/webvtt1/#webvtt-percentage
  // E.g. "4%" or "1.5%"
  if (str[str.size() - 1] != '%') {
    return false;
  }

  double temp;
  if (!base::StringToDouble(str.substr(0, str.size() - 1), &temp) ||
      temp > 100) {
    return false;
  }
  *value = temp;
  return true;
}

bool ParseDoublePercent(const std::string& str, float* a, float* b) {
  auto percents = base::SplitString(str, ",", base::TRIM_WHITESPACE,
                                    base::SPLIT_WANT_NONEMPTY);
  if (percents.size() != 2) {
    return false;
  }
  float temp_a, temp_b;
  if (!ParsePercent(percents[0], &temp_a) ||
      !ParsePercent(percents[1], &temp_b)) {
    return false;
  }
  *a = temp_a;
  *b = temp_b;
  return true;
}

void ParseSettings(const std::string& id,
                   const std::string& value,
                   TextSettings* settings) {
  // https://www.w3.org/TR/webvtt1/#ref-for-parse-the-webvtt-cue-settings-1
  if (id == "region") {
    settings->region = value;
  } else if (id == "vertical") {
    if (value == "rl") {
      settings->writing_direction = WritingDirection::kVerticalGrowingLeft;
    } else if (value == "lr") {
      settings->writing_direction = WritingDirection::kVerticalGrowingRight;
    } else {
      LOG(WARNING) << "Invalid WebVTT vertical setting: " << value;
    }
  } else if (id == "line") {
    const auto pos = value.find(',');
    const std::string line = value.substr(0, pos);
    const std::string align =
        pos != std::string::npos ? value.substr(pos + 1) : "";
    if (pos != std::string::npos) {
      LOG(WARNING) << "WebVTT line alignment isn't supported";
    }

    if (!line.empty() && line[line.size() - 1] == '%') {
      float temp;
      if (!ParsePercent(line, &temp)) {
        LOG(WARNING) << "Invalid WebVTT line: " << value;
        return;
      }
      settings->line.emplace(temp, TextUnitType::kPercent);
    } else {
      double temp;
      if (!base::StringToDouble(line, &temp)) {
        LOG(WARNING) << "Invalid WebVTT line: " << value;
        return;
      }
      settings->line.emplace(temp, TextUnitType::kLines);
    }
  } else if (id == "position") {
    const auto pos = value.find(',');
    const std::string position = value.substr(0, pos);
    const std::string align =
        pos != std::string::npos ? value.substr(pos + 1) : "";
    if (pos != std::string::npos) {
      LOG(WARNING) << "WebVTT position alignment isn't supported";
    }

    float temp;
    if (ParsePercent(position, &temp)) {
      settings->position.emplace(temp, TextUnitType::kPercent);
    } else {
      LOG(WARNING) << "Invalid WebVTT position: " << value;
    }
  } else if (id == "size") {
    float temp;
    if (ParsePercent(value, &temp)) {
      settings->width.emplace(temp, TextUnitType::kPercent);
    } else {
      LOG(WARNING) << "Invalid WebVTT size: " << value;
    }
  } else if (id == "align") {
    if (value == "start") {
      settings->text_alignment = TextAlignment::kStart;
    } else if (value == "center" || value == "middle") {
      settings->text_alignment = TextAlignment::kCenter;
    } else if (value == "end") {
      settings->text_alignment = TextAlignment::kEnd;
    } else if (value == "left") {
      settings->text_alignment = TextAlignment::kLeft;
    } else if (value == "right") {
      settings->text_alignment = TextAlignment::kRight;
    } else {
      LOG(WARNING) << "Invalid WebVTT align: " << value;
    }
  } else {
    LOG(WARNING) << "Unknown WebVTT setting: " << id;
  }
}

}  // namespace

WebVttParser::WebVttParser() {}

void WebVttParser::Init(const InitCB& init_cb,
                        const NewMediaSampleCB& new_media_sample_cb,
                        const NewTextSampleCB& new_text_sample_cb,
                        KeySource* decryption_key_source) {
  DCHECK(init_cb_.is_null());
  DCHECK(!init_cb.is_null());
  DCHECK(!new_text_sample_cb.is_null());
  DCHECK(!decryption_key_source) << "Encrypted WebVTT not supported";

  init_cb_ = init_cb;
  new_text_sample_cb_ = new_text_sample_cb;
}

bool WebVttParser::Flush() {
  reader_.Flush();
  return Parse();
}

bool WebVttParser::Parse(const uint8_t* buf, int size) {
  reader_.PushData(buf, size);
  return Parse();
}

bool WebVttParser::Parse() {
  if (!initialized_) {
    std::vector<std::string> block;
    if (!reader_.Next(&block)) {
      return true;
    }

    // Check the header. It is possible for a 0xFEFF BOM to come before the
    // header text.
    if (block.size() != 1) {
      LOG(ERROR) << "Failed to read WEBVTT header - "
                 << "block size should be 1 but was " << block.size() << ".";
      return false;
    }
    if (block[0] != "WEBVTT" && block[0] != "\xEF\xBB\xBFWEBVTT") {
      LOG(ERROR) << "Failed to read WEBVTT header - should be WEBVTT but was "
                 << block[0];
      return false;
    }
    initialized_ = true;
  }

  std::vector<std::string> block;
  while (reader_.Next(&block)) {
    if (!ParseBlock(block))
      return false;
  }
  return true;
}

bool WebVttParser::ParseBlock(const std::vector<std::string>& block) {
  // NOTE
  if (IsLikelyNote(block[0])) {
    // We can safely ignore the whole block.
    return true;
  }

  // STYLE
  if (IsLikelyStyle(block[0])) {
    if (saw_cue_) {
      LOG(WARNING)
          << "Found style block after seeing cue. Ignoring style block";
    } else {
      for (size_t i = 1; i < block.size(); i++) {
        if (!css_styles_.empty())
          css_styles_ += "\n";
        css_styles_ += block[i];
      }
    }
    return true;
  }

  // REGION
  if (IsLikelyRegion(block[0])) {
    if (saw_cue_) {
      LOG(WARNING)
          << "Found region block after seeing cue. Ignoring region block";
      return true;
    } else {
      return ParseRegion(block);
    }
  }

  // CUE with ID
  if (block.size() >= 2 && MaybeCueId(block[0]) &&
      IsLikelyCueTiming(block[1]) && ParseCueWithId(block)) {
    saw_cue_ = true;
    return true;
  }

  // CUE with no ID
  if (IsLikelyCueTiming(block[0]) && ParseCueWithNoId(block)) {
    saw_cue_ = true;
    return true;
  }

  LOG(ERROR) << "Failed to determine block classification:\n"
             << BlockToString(block.data(), block.size());
  return false;
}

bool WebVttParser::ParseRegion(const std::vector<std::string>& block) {
  TextRegion region;
  std::string region_id;
  // Fill in defaults.  Some may already be this, but set them anyway.
  // See https://www.w3.org/TR/webvtt1/#regions
  region.width.value = 100;
  region.width.type = TextUnitType::kPercent;
  region.height.value = 3;
  region.height.type = TextUnitType::kLines;
  region.window_anchor_x.value = 0;
  region.window_anchor_x.type = TextUnitType::kPercent;
  region.window_anchor_y.value = 100;
  region.window_anchor_y.type = TextUnitType::kPercent;
  region.region_anchor_x.value = 0;
  region.region_anchor_x.type = TextUnitType::kPercent;
  region.region_anchor_y.value = 100;
  region.region_anchor_y.type = TextUnitType::kPercent;

  bool first = true;
  for (const auto& line : block) {
    // First line is "REGION", skip.
    if (first) {
      first = false;
      continue;
    }

    base::StringPairs pairs;
    if (!base::SplitStringIntoKeyValuePairs(line, ':', ' ', &pairs)) {
      LOG(ERROR) << "Invalid WebVTT settings: " << line;
      return false;
    }
    for (const auto& pair : pairs) {
      const std::string& value = pair.second;
      if (pair.first == "id") {
        if (value.find("-->") != std::string::npos) {
          LOG(ERROR) << "Invalid WebVTT REGION ID: " << value;
          return false;
        }
        if (regions_.find(value) != regions_.end()) {
          LOG(ERROR) << "Duplicate WebVTT REGION: " << value;
          return false;
        }
        region_id = value;
      } else if (pair.first == "width") {
        if (!ParsePercent(value, &region.width.value)) {
          LOG(ERROR) << "Invalid WebVTT REGION width: " << value;
          return false;
        }
      } else if (pair.first == "lines") {
        unsigned int temp;
        if (!base::StringToUint(value, &temp)) {
          LOG(ERROR) << "Invalid WebVTT REGION lines: " << value;
          return false;
        }
        region.height.value = temp;
      } else if (pair.first == "regionanchor") {
        if (!ParseDoublePercent(value, &region.region_anchor_x.value,
                                &region.region_anchor_y.value)) {
          LOG(ERROR) << "Invalid WebVTT REGION regionanchor: " << value;
          return false;
        }
      } else if (pair.first == "viewportanchor") {
        if (!ParseDoublePercent(value, &region.window_anchor_x.value,
                                &region.window_anchor_y.value)) {
          LOG(ERROR) << "Invalid WebVTT REGION windowanchor: " << value;
          return false;
        }
      } else if (pair.first == "scroll") {
        if (value != "up") {
          LOG(ERROR) << "Invalid WebVTT REGION scroll: " << value;
          return false;
        }
        region.scroll = true;
      } else {
        LOG(ERROR) << "Unknown WebVTT REGION setting: " << pair.first;
        return false;
      }
    }
  }
  if (region_id.empty()) {
    LOG(ERROR) << "WebVTT REGION id is required";
    return false;
  }
  regions_.insert(std::make_pair(region_id, std::move(region)));
  return true;
}

bool WebVttParser::ParseCueWithNoId(const std::vector<std::string>& block) {
  return ParseCue("", block.data(), block.size());
}

bool WebVttParser::ParseCueWithId(const std::vector<std::string>& block) {
  return ParseCue(block[0], block.data() + 1, block.size() - 1);
}

bool WebVttParser::ParseCue(const std::string& id,
                            const std::string* block,
                            size_t block_size) {
  const std::vector<std::string> time_and_style = base::SplitString(
      block[0], " ", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  int64_t start_time = 0;
  int64_t end_time = 0;

  const bool parsed_time =
      time_and_style.size() >= 3 && time_and_style[1] == "-->" &&
      WebVttTimestampToMs(time_and_style[0], &start_time) &&
      WebVttTimestampToMs(time_and_style[2], &end_time);

  if (!parsed_time) {
    LOG(ERROR) << "Could not parse start time, -->, and end time from "
               << block[0];
    return false;
  }

  if (!stream_info_dispatched_)
    DispatchTextStreamInfo();

  // According to the WebVTT spec end time must be greater than the start time
  // of the cue. Since we are seeing content with invalid times in the field, we
  // are going to drop the cue instead of failing to package.
  //
  // For more context see:
  //   - https://www.w3.org/TR/webvtt1/#webvtt-cue-timings
  //   - https://github.com/shaka-project/shaka-packager/issues/335
  //   - https://github.com/shaka-project/shaka-packager/issues/425
  //
  // Print a warning so that those packaging content can know that their
  // content is not spec compliant.
  if (end_time <= start_time) {
    LOG(WARNING) << "WebVTT input is not spec compliant. Start time ("
                 << start_time << ") should be less than end time (" << end_time
                 << "). Skipping webvtt cue:"
                 << BlockToString(block, block_size);
    return true;
  }

  TextSettings settings;
  for (size_t i = 3; i < time_and_style.size(); i++) {
    const auto pos = time_and_style[i].find(':');
    if (pos == std::string::npos) {
      continue;
    }

    const std::string key = time_and_style[i].substr(0, pos);
    const std::string value = time_and_style[i].substr(pos + 1);
    ParseSettings(key, value, &settings);
  }

  // The rest of the block is the payload.
  // TODO: Parse tags to support <b>, <i>, etc.
  TextFragment body;
  TextFragmentStyle no_styles;
  for (size_t i = 1; i < block_size; i++) {
    if (i > 1 && i != block_size) {
      body.sub_fragments.emplace_back(no_styles, /* newline= */ true);
    }
    body.sub_fragments.emplace_back(no_styles, block[i]);
  }

  const auto sample =
      std::make_shared<TextSample>(id, start_time, end_time, settings, body);
  return new_text_sample_cb_.Run(kStreamIndex, sample);
}

void WebVttParser::DispatchTextStreamInfo() {
  stream_info_dispatched_ = true;

  const int kTrackId = 0;
  // The resolution of timings are in milliseconds.
  const int kTimescale = 1000;
  // The duration passed here is not very important. Also the whole file
  // must be read before determining the real duration which doesn't
  // work nicely with the current demuxer.
  const int kDuration = 0;
  const char kWebVttCodecString[] = "wvtt";
  const int64_t kNoWidth = 0;
  const int64_t kNoHeight = 0;
  // The language of the stream will be overwritten by the Demuxer later.
  const char kNoLanguage[] = "";

  const auto stream = std::make_shared<TextStreamInfo>(
      kTrackId, kTimescale, kDuration, kCodecWebVtt, kWebVttCodecString, "",
      kNoWidth, kNoHeight, kNoLanguage);
  stream->set_css_styles(css_styles_);
  for (const auto& pair : regions_)
    stream->AddRegion(pair.first, pair.second);

  std::vector<std::shared_ptr<StreamInfo>> streams{stream};
  init_cb_.Run(streams);
}

}  // namespace media
}  // namespace shaka
