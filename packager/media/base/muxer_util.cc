// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/base/muxer_util.h"

#include <inttypes.h>

#include <string>
#include <vector>

#include "packager/base/logging.h"
#include "packager/base/strings/string_number_conversions.h"
#include "packager/base/strings/string_split.h"
#include "packager/base/strings/stringprintf.h"
#include "packager/media/base/video_stream_info.h"

namespace shaka {
namespace {
bool ValidateFormatTag(const std::string& format_tag) {
  DCHECK(!format_tag.empty());
  // Format tag should follow this prototype: %0[width]d.
  if (format_tag.size() > 3 && format_tag[0] == '%' && format_tag[1] == '0' &&
      format_tag[format_tag.size() - 1] == 'd') {
    unsigned out;
    if (base::StringToUint(format_tag.substr(2, format_tag.size() - 3), &out))
      return true;
  }
  LOG(ERROR) << "SegmentTemplate: Format tag should follow this prototype: "
             << "%0[width]d if exist.";
  return false;
}
}  // namespace

namespace media {

bool ValidateSegmentTemplate(const std::string& segment_template) {
  if (segment_template.empty())
    return false;

  std::vector<std::string> splits = base::SplitString(
      segment_template, "$", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);

  // ISO/IEC 23009-1:2012 5.3.9.4.4 Template-based Segment URL construction.
  // Allowed identifiers: $$, $RepresentationID$, $Number$, $Bandwidth$, $Time$.
  // "$" always appears in pairs, so there should be odd number of splits.
  if (splits.size() % 2 == 0) {
    LOG(ERROR) << "SegmentTemplate: '$' should appear in pairs.";
    return false;
  }

  bool has_number = false;
  bool has_time = false;
  // Every second substring in split output should be an identifier.
  for (size_t i = 1; i < splits.size(); i += 2) {
    // Each identifier may be suffixed, within the enclosing ‘$’ characters,
    // with an additional format tag aligned with the printf format tag as
    // defined in IEEE 1003.1-2008 [10] following this prototype: %0[width]d.
    size_t format_pos = splits[i].find('%');
    std::string identifier = splits[i].substr(0, format_pos);
    if (format_pos != std::string::npos) {
      if (!ValidateFormatTag(splits[i].substr(format_pos)))
        return false;
    }

    // TODO(kqyang): Support "RepresentationID".
    if (identifier == "RepresentationID") {
      NOTIMPLEMENTED() << "SegmentTemplate: $RepresentationID$ is not supported "
                          "yet.";
      return false;
    } else if (identifier == "Number") {
      has_number = true;
    } else if (identifier == "Time") {
      has_time = true;
    } else if (identifier == "") {
      if (format_pos != std::string::npos) {
        LOG(ERROR) << "SegmentTemplate: $$ should not have any format tags.";
        return false;
      }
    } else if (identifier != "Bandwidth") {
      LOG(ERROR) << "SegmentTemplate: '$" << splits[i]
                 << "$' is not a valid identifier.";
      return false;
    }
  }
  if (has_number && has_time) {
    LOG(ERROR) << "SegmentTemplate: $Number$ and $Time$ should not co-exist.";
    return false;
  }
  if (!has_number && !has_time) {
    LOG(ERROR) << "SegmentTemplate: $Number$ or $Time$ should exist.";
    return false;
  }
  // Note: The below check is skipped.
  // Strings outside identifiers shall only contain characters that are
  // permitted within URLs according to RFC 1738.
  return true;
}

std::string GetSegmentName(const std::string& segment_template,
                           uint64_t segment_start_time,
                           uint32_t segment_index,
                           uint32_t bandwidth) {
  DCHECK(ValidateSegmentTemplate(segment_template));

  std::vector<std::string> splits = base::SplitString(
      segment_template, "$", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  // "$" always appears in pairs, so there should be odd number of splits.
  DCHECK_EQ(1u, splits.size() % 2);

  std::string segment_name;
  for (size_t i = 0; i < splits.size(); ++i) {
    // Every second substring in split output should be an identifier.
    // Simply copy the non-identifier part.
    if (i % 2 == 0) {
      segment_name += splits[i];
      continue;
    }
    if (splits[i].empty()) {
      // "$$" is an escape sequence, replaced with a single "$".
      segment_name += "$";
      continue;
    }
    size_t format_pos = splits[i].find('%');
    std::string identifier = splits[i].substr(0, format_pos);
    DCHECK(identifier == "Number" || identifier == "Time" ||
           identifier == "Bandwidth");

    std::string format_tag;
    if (format_pos != std::string::npos) {
      format_tag = splits[i].substr(format_pos);
      DCHECK(ValidateFormatTag(format_tag));
      // Replace %d formatting to correctly format uint64_t.
      format_tag = format_tag.substr(0, format_tag.size() - 1) + PRIu64;
    } else {
      // Default format tag "%01d", modified to format uint64_t correctly.
      format_tag = "%01" PRIu64;
    }

    if (identifier == "Number") {
      // SegmentNumber starts from 1.
      segment_name += base::StringPrintf(
          format_tag.c_str(), static_cast<uint64_t>(segment_index + 1));
    } else if (identifier == "Time") {
      segment_name +=
          base::StringPrintf(format_tag.c_str(), segment_start_time);
    } else if (identifier == "Bandwidth") {
      segment_name += base::StringPrintf(format_tag.c_str(),
                                         static_cast<uint64_t>(bandwidth));
    }
  }
  return segment_name;
}

}  // namespace media
}  // namespace shaka
