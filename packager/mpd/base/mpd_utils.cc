// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/mpd/base/mpd_utils.h"

#include <libxml/tree.h>

#include "packager/base/logging.h"
#include "packager/base/strings/string_number_conversions.h"
#include "packager/mpd/base/content_protection_element.h"
#include "packager/mpd/base/media_info.pb.h"
#include "packager/mpd/base/xml/scoped_xml_ptr.h"

namespace edash_packager {

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
  std::string video_codec;
  if (media_info.has_video_info())
    video_codec = media_info.video_info().codec();

  std::string audio_codec;
  if (media_info.has_audio_info())
    audio_codec = media_info.audio_info().codec();

  if (!video_codec.empty() && !audio_codec.empty()) {
    return video_codec + "," + audio_codec;
  } else if (!video_codec.empty()) {
    return video_codec;
  } else if (!audio_codec.empty()) {
    return audio_codec;
  }

  return "";
}

std::string SecondsToXmlDuration(double seconds) {
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

}  // namespace edash_packager
