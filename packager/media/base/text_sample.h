// Copyright 2017 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_BASE_TEXT_SAMPLE_H_
#define PACKAGER_MEDIA_BASE_TEXT_SAMPLE_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "packager/base/optional.h"

namespace shaka {
namespace media {

enum class TextUnitType {
  /// The units are absolute units in pixels.
  kPixels,
  /// The units are absolute units in number of lines.
  kLines,
  /// The units are relative to some size, in percent (i.e. 0-100).
  kPercent,
};

enum class WritingDirection {
  kHorizontal,
  kVerticalGrowingLeft,
  kVerticalGrowingRight,
};

enum class TextAlignment {
  /// Align the text at the start, based on the Unicode text direction.
  kStart,
  /// Align the text in the center of the box.
  kCenter,
  /// Align the text at the end, based on the Unicode text direction.
  kEnd,
  /// Align the text at the left side (or top for non-horizontal).
  kLeft,
  /// Align the text at the right side (or bottom for non-horizontal).
  kRight,
};

struct TextNumber {
  TextNumber(float value, TextUnitType type) : value(value), type(type) {}

  float value;
  TextUnitType type;
};

struct TextSettings {
  /// The line offset of the cue.  For horizontal cues, this is the vertical
  /// offset.  Percent units are relative to the window.
  base::Optional<TextNumber> line;
  /// The position offset of the cue.  For horizontal cues, this is the
  /// horizontal offset.  Percent units are relative to the window.
  base::Optional<TextNumber> position;
  /// For horizontal cues, this is the width of the area to draw cues.  For
  /// vertical cues, this is the height.  Percent units are relative to the
  /// window.
  base::Optional<TextNumber> width;
  /// For horizontal cues, this is the height of the area to draw cues.  For
  /// vertical cues, this is the width.  Percent units are relative to the
  /// window.
  base::Optional<TextNumber> height;

  /// The region to draw the cue in.
  std::string region;

  /// The direction to draw text.  This is also used to determine how cues are
  /// positioned within the region.
  WritingDirection writing_direction = WritingDirection::kHorizontal;
  /// How to align the text within the cue box.
  TextAlignment text_alignment = TextAlignment::kCenter;
};

struct TextFragmentStyle {
  base::Optional<bool> underline;
  base::Optional<bool> bold;
  base::Optional<bool> italic;
  // The colors could be any string that can be interpreted as
  // a color in TTML (or WebVTT). As a start, the 8 teletext colors are used,
  // i.e. black, red, green, yellow, blue, magenta, cyan, and white
  std::string color;
  std::string backgroundColor;
};

/// Represents a recursive structure of styled blocks of text.  Only one of
/// sub_fragments, body, image, or newline will be set.
struct TextFragment {
  TextFragment() {}
  TextFragment(const TextFragmentStyle& style,
               const std::vector<TextFragment>& sub_fragments)
      : style(style), sub_fragments(sub_fragments) {}
  TextFragment(const TextFragmentStyle& style, const char* body)
      : style(style), body(body) {}
  TextFragment(const TextFragmentStyle& style, const std::string& body)
      : style(style), body(body) {}
  TextFragment(const TextFragmentStyle& style,
               const std::vector<uint8_t>& image)
      : style(style), image(image) {}
  TextFragment(const TextFragmentStyle& style, bool newline)
      : style(style), newline(newline) {}

  TextFragmentStyle style;

  std::vector<TextFragment> sub_fragments;
  std::string body;
  /// PNG image data.
  std::vector<uint8_t> image;
  bool newline = false;

  bool is_empty() const;
};

class TextSample {
 public:
  TextSample(const std::string& id,
             int64_t start_time,
             int64_t end_time,
             const TextSettings& settings,
             const TextFragment& body);

  const std::string& id() const { return id_; }
  int64_t start_time() const { return start_time_; }
  int64_t duration() const { return duration_; }
  const TextSettings& settings() const { return settings_; }
  const TextFragment& body() const { return body_; }
  int64_t EndTime() const;

  int32_t sub_stream_index() const { return sub_stream_index_; }
  void set_sub_stream_index(int32_t idx) { sub_stream_index_ = idx; }

 private:
  // Allow the compiler generated copy constructor and assignment operator
  // intentionally. Since the text data is typically small, the performance
  // impact is minimal.

  const std::string id_;
  const int64_t start_time_ = 0;
  const int64_t duration_ = 0;
  const TextSettings settings_;
  const TextFragment body_;
  int32_t sub_stream_index_ = -1;
};

}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_BASE_TEXT_SAMPLE_H_
