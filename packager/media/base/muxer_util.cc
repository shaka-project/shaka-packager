// Copyright 2014 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/media/base/muxer_util.h>

#include <cinttypes>
#include <string>
#include <vector>

#include <absl/log/check.h>
#include <absl/log/log.h>
#include <absl/strings/numbers.h>
#include <absl/strings/str_format.h>
#include <absl/strings/str_split.h>

#include <packager/media/base/video_stream_info.h>

namespace shaka {
namespace {
Status ValidateFormatTag(const std::string& format_tag) {
  if (format_tag.empty()) {
    return Status(error::INVALID_ARGUMENT, "Format tag should not be empty");
  }

  // Format tag should follow this prototype: %0[width]d.
  if (format_tag.size() > 3 && format_tag[0] == '%' && format_tag[1] == '0' &&
      format_tag[format_tag.size() - 1] == 'd') {
    unsigned out;
    if (absl::SimpleAtoi(format_tag.substr(2, format_tag.size() - 3), &out)) {
      return Status::OK;
    }
  }

  return Status(
      error::INVALID_ARGUMENT,
      "Format tag should follow this prototype: %0[width]d if exist.");
}
}  // namespace

namespace media {

Status ValidateSegmentTemplate(const std::string& segment_template) {
  if (segment_template.empty()) {
    return Status(error::INVALID_ARGUMENT,
                  "Segment template should not be empty.");
  }

  std::vector<std::string> splits = absl::StrSplit(segment_template, "$");

  // ISO/IEC 23009-1:2012 5.3.9.4.4 Template-based Segment URL construction.
  // Allowed identifiers: $$, $RepresentationID$, $Number$, $Bandwidth$, $Time$.
  // "$" always appears in pairs, so there should be odd number of splits.
  if (splits.size() % 2 == 0) {
    return Status(error::INVALID_ARGUMENT,
                  "In segment templates, '$' should appear in pairs.");
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
      Status tag_check = ValidateFormatTag(splits[i].substr(format_pos));
      if (!tag_check.ok()) {
        return tag_check;
      }
    }

    // TODO(kqyang): Support "RepresentationID".
    if (identifier == "RepresentationID") {
      return Status(
          error::UNIMPLEMENTED,
          "Segment template flag $RepresentationID$ is not supported yet.");
    } else if (identifier == "Number") {
      has_number = true;
    } else if (identifier == "Time") {
      has_time = true;
    } else if (identifier == "") {
      if (format_pos != std::string::npos) {
        return Status(error::INVALID_ARGUMENT,
                      "'$$' should not have any format tags.");
      }
    } else if (identifier != "Bandwidth") {
      return Status(error::INVALID_ARGUMENT,
                    "'$" + splits[i] + "$' is not a valid identifier.");
    }
  }
  if (has_number && has_time) {
    return Status(
        error::INVALID_ARGUMENT,
        "In segment templates $Number$ and $Time$ should not co-exist.");
  }
  if (!has_number && !has_time) {
    return Status(error::INVALID_ARGUMENT,
                  "In segment templates $Number$ or $Time$ should exist.");
  }
  // Note: The below check is skipped.
  // Strings outside identifiers shall only contain characters that are
  // permitted within URLs according to RFC 1738.
  return Status::OK;
}

std::string GetSegmentName(const std::string& segment_template,
                           int64_t segment_start_time,
                           uint32_t segment_index,
                           uint32_t bandwidth) {
  DCHECK_EQ(Status::OK, ValidateSegmentTemplate(segment_template));

  std::vector<std::string> splits = absl::StrSplit(segment_template, "$");
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
      DCHECK_EQ(Status::OK, ValidateFormatTag(format_tag));
      // Replace %d formatting to correctly format uint64_t.
      format_tag = format_tag.substr(0, format_tag.size() - 1) + PRIu64;
    } else {
      // Default format tag "%01d", modified to format uint64_t correctly.
      format_tag = "%01" PRIu64;
    }

    // absl::StrFormat requires compile-time constants for format strings.
    // If you don't have that, you build the formatter using this interface
    // instead.
    std::vector<absl::FormatArg> format_args;
    absl::UntypedFormatSpec format(format_tag);
    if (identifier == "Number") {
      // SegmentNumber starts from 1.
      format_args.emplace_back(static_cast<uint64_t>(segment_index + 1));
    } else if (identifier == "Time") {
      format_args.emplace_back(static_cast<uint64_t>(segment_start_time));
    } else if (identifier == "Bandwidth") {
      format_args.emplace_back(static_cast<uint64_t>(bandwidth));
    }

    std::string format_output;
    if (absl::FormatUntyped(&format_output, format, format_args)) {
      segment_name += format_output;
    }
  }
  return segment_name;
}

}  // namespace media
}  // namespace shaka
