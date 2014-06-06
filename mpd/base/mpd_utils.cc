// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "mpd/base/mpd_utils.h"

#include <set>

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "mpd/base/content_protection_element.h"
#include "mpd/base/xml/scoped_xml_ptr.h"
#include "third_party/libxml/src/include/libxml/tree.h"


namespace {

// Concatenate all the codecs in |repeated_stream_info|.
template <typename RepeatedStreamInfoType>
std::string CodecsString(const RepeatedStreamInfoType& repeated_stream_info) {
  std::string codecs;
  for (int i = 0; i < repeated_stream_info.size(); ++i) {
    codecs.append(repeated_stream_info.Get(i).codec());
    codecs.append(",");
  }

  if (!codecs.empty()) {
    DCHECK_EQ(codecs[codecs.size() - 1], ',');
    codecs.resize(codecs.size() - 1);  // Cut off ',' at the end.
  }

  return codecs;
}

}  // namespace

namespace dash_packager {

bool HasVODOnlyFields(const MediaInfo& media_info) {
  return media_info.has_init_range() || media_info.has_index_range() ||
         media_info.has_media_file_name() ||
         media_info.has_media_duration_seconds();
}

bool HasLiveOnlyFields(const MediaInfo& media_info) {
  return media_info.has_init_segment_name() ||
         media_info.has_segment_template() ||
         media_info.has_segment_duration_seconds();
}

void RemoveDuplicateAttributes(
    ContentProtectionElement* content_protection_element) {
  DCHECK(content_protection_element);
  typedef std::map<std::string, std::string> AttributesMap;

  AttributesMap& attributes = content_protection_element->additional_attributes;
  if (!content_protection_element->value.empty())
    attributes.erase("value");

  if (!content_protection_element->scheme_id_uri.empty())
    attributes.erase("schemeIdUri");
}

std::string GetCodecs(const MediaInfo& media_info) {
  std::string video_codecs;
  if (media_info.video_info_size() > 0)
    video_codecs = CodecsString(media_info.video_info());

  std::string audio_codecs;
  if (media_info.audio_info_size() > 0)
    audio_codecs = CodecsString(media_info.audio_info());

  if (!video_codecs.empty() && !audio_codecs.empty()) {
    return video_codecs + "," + audio_codecs;
  } else if (!video_codecs.empty()) {
    return video_codecs;
  } else if (!audio_codecs.empty()) {
    return audio_codecs;
  }

  return "";
}

std::string SecondsToXmlDuration(float seconds) {
  return "PT" + base::DoubleToString(seconds) + "S";
}

bool GetDurationAttribute(xmlNodePtr node, float* duration) {
  DCHECK(node);
  DCHECK(duration);
  static const char kDuration[] = "duration";
  xml::ScopedXmlPtr<xmlChar>::type duration_value(
      xmlGetProp(node, BAD_CAST kDuration));

  if (!duration_value)
    return false;

  double duration_double_precision = 0.0;
  if (!base::StringToDouble(reinterpret_cast<const char*>(duration_value.get()),
                            &duration_double_precision)) {
    return false;
  }

  *duration = static_cast<float>(duration_double_precision);
  return true;
}

bool MoreThanOneTrue(bool b1, bool b2, bool b3) {
  return (b1 && b2) || (b2 && b3) || (b3 && b1);
}

bool AtLeastOneTrue(bool b1, bool b2, bool b3) { return b1 || b2 || b3; }

bool OnlyOneTrue(bool b1, bool b2, bool b3) {
    return !MoreThanOneTrue(b1, b2, b3) && AtLeastOneTrue(b1, b2, b3);
}

}  // namespace dash_packager
