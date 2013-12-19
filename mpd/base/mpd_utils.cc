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
         media_info.has_media_file_name() || media_info.has_media_duration();
}

bool HasLiveOnlyFields(const MediaInfo& media_info) {
  return media_info.has_init_segment_name() ||
         media_info.has_segment_template() || media_info.has_segment_duration();
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

std::string SecondsToXmlDuration(uint32 seconds) {
  return "PT" + base::UintToString(seconds) + "S";
}

bool GetDurationAttribute(xmlNodePtr node, uint32* duration) {
  DCHECK(node);
  DCHECK(duration);
  static const char kDuration[] = "duration";
  xml::ScopedXmlPtr<xmlChar>::type duration_value(
      xmlGetProp(node, BAD_CAST kDuration));

  if (!duration_value)
    return false;

  return base::StringToUint(reinterpret_cast<const char*>(duration_value.get()),
                            duration);
}

}  // namespace dash_packager
