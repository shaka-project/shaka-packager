// Copyright 2017 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/formats/webvtt/webvtt_utils.h"

#include <ctype.h>
#include <inttypes.h>

#include <algorithm>
#include <cmath>
#include <unordered_set>

#include "packager/base/logging.h"
#include "packager/base/strings/string_number_conversions.h"
#include "packager/base/strings/string_util.h"
#include "packager/base/strings/stringprintf.h"

namespace shaka {
namespace media {

namespace {

constexpr const char* kRegionTeletextPrefix = "ttx_";

bool GetTotalMilliseconds(uint64_t hours,
                          uint64_t minutes,
                          uint64_t seconds,
                          uint64_t ms,
                          int64_t* out) {
  DCHECK(out);
  if (minutes > 59 || seconds > 59 || ms > 999) {
    VLOG(1) << "Hours:" << hours << " Minutes:" << minutes
            << " Seconds:" << seconds << " MS:" << ms
            << " shoud have never made it to GetTotalMilliseconds";
    return false;
  }
  *out = 60 * 60 * 1000 * hours + 60 * 1000 * minutes + 1000 * seconds + ms;
  return true;
}

enum class StyleTagKind {
  kUnderline,
  kBold,
  kItalic,
};

std::string GetOpenTag(StyleTagKind tag) {
  switch (tag) {
    case StyleTagKind::kUnderline:
      return "<u>";
    case StyleTagKind::kBold:
      return "<b>";
    case StyleTagKind::kItalic:
      return "<i>";
  }
  return "";  // Not reached, but Windows doesn't like NOTREACHED.
}

std::string GetCloseTag(StyleTagKind tag) {
  switch (tag) {
    case StyleTagKind::kUnderline:
      return "</u>";
    case StyleTagKind::kBold:
      return "</b>";
    case StyleTagKind::kItalic:
      return "</i>";
  }
  return "";  // Not reached, but Windows doesn't like NOTREACHED.
}

bool IsWhitespace(char c) {
  return c == '\t' || c == '\r' || c == '\n' || c == ' ';
}

// Replace consecutive whitespaces with a single whitespace.
std::string CollapseWhitespace(const std::string& data) {
  std::string output;
  output.resize(data.size());
  size_t chars_written = 0;
  bool in_whitespace = false;
  for (char c : data) {
    if (IsWhitespace(c)) {
      if (!in_whitespace) {
        in_whitespace = true;
        output[chars_written++] = ' ';
      }
    } else {
      in_whitespace = false;
      output[chars_written++] = c;
    }
  }
  output.resize(chars_written);
  return output;
}

std::string WriteFragment(const TextFragment& fragment,
                          std::list<StyleTagKind>* tags) {
  std::string ret;
  size_t local_tag_count = 0;
  auto has = [tags](StyleTagKind tag) {
    return std::find(tags->begin(), tags->end(), tag) != tags->end();
  };
  auto push_tag = [tags, &local_tag_count, &has](StyleTagKind tag) {
    if (has(tag)) {
      return std::string();
    }
    tags->push_back(tag);
    local_tag_count++;
    return GetOpenTag(tag);
  };

  if ((fragment.style.underline == false && has(StyleTagKind::kUnderline)) ||
      (fragment.style.bold == false && has(StyleTagKind::kBold)) ||
      (fragment.style.italic == false && has(StyleTagKind::kItalic))) {
    LOG(WARNING) << "WebVTT output doesn't support disabling "
                    "underline/bold/italic within a cue";
  }

  if (fragment.newline) {
    // Newlines represent separate WebVTT cues. So close the existing tags to
    // be nice and re-open them on the new line.
    for (auto it = tags->rbegin(); it != tags->rend(); it++) {
      ret += GetCloseTag(*it);
    }
    ret += "\n";
    for (const auto tag : *tags) {
      ret += GetOpenTag(tag);
    }
  } else {
    if (fragment.style.underline == true) {
      ret += push_tag(StyleTagKind::kUnderline);
    }
    if (fragment.style.bold == true) {
      ret += push_tag(StyleTagKind::kBold);
    }
    if (fragment.style.italic == true) {
      ret += push_tag(StyleTagKind::kItalic);
    }

    if (!fragment.body.empty()) {
      // Replace newlines and consecutive whitespace with a single space.  If
      // the user wanted an explicit newline, they should use the "newline"
      // field.
      ret += CollapseWhitespace(fragment.body);
    } else {
      for (const auto& frag : fragment.sub_fragments) {
        ret += WriteFragment(frag, tags);
      }
    }

    // Pop all the local tags we pushed.
    while (local_tag_count > 0) {
      ret += GetCloseTag(tags->back());
      tags->pop_back();
      local_tag_count--;
    }
  }
  return ret;
}

}  // namespace

bool WebVttTimestampToMs(const base::StringPiece& source, int64_t* out) {
  DCHECK(out);

  if (source.length() < 9) {
    LOG(WARNING) << "Timestamp '" << source << "' is mal-formed";
    return false;
  }

  const size_t minutes_begin = source.length() - 9;
  const size_t seconds_begin = source.length() - 6;
  const size_t milliseconds_begin = source.length() - 3;

  uint64_t hours = 0;
  uint64_t minutes = 0;
  uint64_t seconds = 0;
  uint64_t ms = 0;

  const bool has_hours =
      minutes_begin >= 3 && source[minutes_begin - 1] == ':' &&
      base::StringToUint64(source.substr(0, minutes_begin - 1), &hours);

  if ((minutes_begin == 0 || has_hours) && source[seconds_begin - 1] == ':' &&
      source[milliseconds_begin - 1] == '.' &&
      base::StringToUint64(source.substr(minutes_begin, 2), &minutes) &&
      base::StringToUint64(source.substr(seconds_begin, 2), &seconds) &&
      base::StringToUint64(source.substr(milliseconds_begin, 3), &ms)) {
    return GetTotalMilliseconds(hours, minutes, seconds, ms, out);
  }

  LOG(WARNING) << "Timestamp '" << source << "' is mal-formed";
  return false;
}

std::string MsToWebVttTimestamp(uint64_t ms) {
  uint64_t remaining = ms;

  uint64_t only_ms = remaining % 1000;
  remaining /= 1000;
  uint64_t only_seconds = remaining % 60;
  remaining /= 60;
  uint64_t only_minutes = remaining % 60;
  remaining /= 60;
  uint64_t only_hours = remaining;

  return base::StringPrintf("%02" PRIu64 ":%02" PRIu64 ":%02" PRIu64
                            ".%03" PRIu64,
                            only_hours, only_minutes, only_seconds, only_ms);
}

std::string WebVttSettingsToString(const TextSettings& settings) {
  std::string ret;
  if (!settings.region.empty() &&
      settings.region.find(kRegionTeletextPrefix) != 0) {
    // Don't add teletext ttx_ regions, since accompanied by global line numbers
    ret += " region:";
    ret += settings.region;
  }
  if (settings.line) {
    switch (settings.line->type) {
      case TextUnitType::kPercent:
        ret += " line:";
        ret += base::DoubleToString(settings.line->value);
        ret += "%";
        break;
      case TextUnitType::kLines:
        ret += " line:";
        // The line number should be an integer
        ret += base::DoubleToString(std::round(settings.line->value));
        break;
      case TextUnitType::kPixels:
        LOG(WARNING) << "WebVTT doesn't support pixel line settings";
        break;
    }
  }
  if (settings.position) {
    if (settings.position->type == TextUnitType::kPercent) {
      ret += " position:";
      ret += base::DoubleToString(settings.position->value);
      ret += "%";
    } else {
      LOG(WARNING) << "WebVTT only supports percent position settings";
    }
  }
  if (settings.width) {
    if (settings.width->type == TextUnitType::kPercent) {
      ret += " size:";
      ret += base::DoubleToString(settings.width->value);
      ret += "%";
    } else {
      LOG(WARNING) << "WebVTT only supports percent width settings";
    }
  }
  if (settings.height) {
    LOG(WARNING) << "WebVTT doesn't support cue heights";
  }
  if (settings.writing_direction != WritingDirection::kHorizontal) {
    ret += " direction:";
    if (settings.writing_direction == WritingDirection::kVerticalGrowingLeft) {
      ret += "rl";
    } else {
      ret += "lr";
    }
  }
  switch (settings.text_alignment) {
    case TextAlignment::kStart:
      ret += " align:start";
      break;
    case TextAlignment::kEnd:
      ret += " align:end";
      break;
    case TextAlignment::kLeft:
      ret += " align:left";
      break;
    case TextAlignment::kRight:
      ret += " align:right";
      break;
    case TextAlignment::kCenter:
      ret += " align:center";
      break;
  }

  if (!ret.empty()) {
    DCHECK_EQ(ret[0], ' ');
    ret.erase(0, 1);
  }
  return ret;
}

std::string WebVttFragmentToString(const TextFragment& fragment) {
  std::list<StyleTagKind> tags;
  return WriteFragment(fragment, &tags);
}

std::string WebVttGetPreamble(const TextStreamInfo& stream_info) {
  std::string ret;
  for (const auto& pair : stream_info.regions()) {
    if (!ret.empty()) {
      ret += "\n\n";
    }

    if (pair.second.width.type != TextUnitType::kPercent ||
        pair.second.height.type != TextUnitType::kLines ||
        pair.second.window_anchor_x.type != TextUnitType::kPercent ||
        pair.second.window_anchor_y.type != TextUnitType::kPercent ||
        pair.second.region_anchor_x.type != TextUnitType::kPercent ||
        pair.second.region_anchor_y.type != TextUnitType::kPercent) {
      LOG(WARNING) << "Unsupported unit type in WebVTT region";
      continue;
    }

    base::StringAppendF(
        &ret,
        "REGION\n"
        "id:%s\n"
        "width:%f%%\n"
        "lines:%d\n"
        "viewportanchor:%f%%,%f%%\n"
        "regionanchor:%f%%,%f%%",
        pair.first.c_str(), pair.second.width.value,
        static_cast<int>(pair.second.height.value),
        pair.second.window_anchor_x.value, pair.second.window_anchor_y.value,
        pair.second.region_anchor_x.value, pair.second.region_anchor_y.value);
    if (pair.second.scroll) {
      ret += "\nscroll:up";
    }
  }

  if (!stream_info.css_styles().empty()) {
    if (!ret.empty()) {
      ret += "\n\n";
    }
    ret += "STYLE\n" + stream_info.css_styles();
  }

  return ret;
}

}  // namespace media
}  // namespace shaka
