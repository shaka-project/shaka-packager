// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/mpd/base/mpd_utils.h"

#include <libxml/tree.h>

#include "packager/base/logging.h"
#include "packager/base/strings/string_number_conversions.h"
#include "packager/mpd/base/xml/scoped_xml_ptr.h"

namespace shaka {
namespace {

std::string TextCodecString(const MediaInfo& media_info) {
  CHECK(media_info.has_text_info());
  const std::string& format = media_info.text_info().format();
  // DASH IOP mentions that the codec for ttml in mp4 is stpp.
  if (format == "ttml" &&
      (media_info.container_type() == MediaInfo::CONTAINER_MP4)) {
    return "stpp";
  }

  // Otherwise codec doesn't need to be specified, e.g. vtt and ttml+xml are
  // obvious from the mime type.
  return "";
}

}  // namespace

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

std::string GetLanguage(const MediaInfo& media_info) {
  std::string lang;
  if (media_info.has_audio_info()) {
    lang = media_info.audio_info().language();
  } else if (media_info.has_text_info()) {
    lang = media_info.text_info().language();
  }
  return lang;
}

std::string GetCodecs(const MediaInfo& media_info) {
  CHECK(OnlyOneTrue(media_info.has_video_info(), media_info.has_audio_info(),
                    media_info.has_text_info()));

  if (media_info.has_video_info()) {
    if (media_info.container_type() == MediaInfo::CONTAINER_WEBM) {
      std::string codec = media_info.video_info().codec().substr(0, 4);
      // media_info.video_info().codec() contains new revised codec string
      // specified by "VPx in ISO BMFF" document, which is not compatible to
      // old codec strings in WebM. Hack it here before all browsers support
      // new codec strings.
      if (codec == "vp08")
        return "vp8";
      if (codec == "vp09")
        return "vp9";
    }
    return media_info.video_info().codec();
  }

  if (media_info.has_audio_info())
    return media_info.audio_info().codec();

  if (media_info.has_text_info())
    return TextCodecString(media_info);

  NOTREACHED();
  return "";
}

std::string GetBaseCodec(const MediaInfo& media_info) {
  std::string codec;
  if (media_info.has_video_info()) {
    codec = media_info.video_info().codec();
  } else if (media_info.has_audio_info()) {
    codec = media_info.audio_info().codec();
  } else if (media_info.has_text_info()) {
    codec = media_info.text_info().format();
  }
  // Convert, for example, "mp4a.40.2" to simply "mp4a".
  // "mp4a.40.2" and "mp4a.40.5" can exist in the same AdaptationSet.
  size_t dot = codec.find('.');
  if (dot != std::string::npos) {
    codec.erase(dot);
  }
  return codec;
}

std::string GetAdaptationSetKey(const MediaInfo& media_info) {
  std::string key;

  if (media_info.has_video_info()) {
    key.append("video:");
  } else if (media_info.has_audio_info()) {
    key.append("audio:");
  } else if (media_info.has_text_info()) {
    key.append(MediaInfo_TextInfo_TextType_Name(media_info.text_info().type()));
    key.append(":");
  } else {
    key.append("unknown:");
  }

  key.append(MediaInfo_ContainerType_Name(media_info.container_type()));
  key.append(":");
  key.append(GetBaseCodec(media_info));
  key.append(":");
  key.append(GetLanguage(media_info));

  return key;
}

std::string SecondsToXmlDuration(double seconds) {
  return "PT" + DoubleToString(seconds) + "S";
}

bool GetDurationAttribute(xmlNodePtr node, float* duration) {
  DCHECK(node);
  DCHECK(duration);
  static const char kDuration[] = "duration";
  xml::scoped_xml_ptr<xmlChar> duration_value(
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

// Implement our own DoubleToString as base::DoubleToString uses third_party
// library dmg_fp.
std::string DoubleToString(double value) {
  std::ostringstream stringstream;
  stringstream << value;
  return stringstream.str();
}

// Coverts binary data into human readable UUID format.
bool HexToUUID(const std::string& data, std::string* uuid_format) {
  DCHECK(uuid_format);
  const size_t kExpectedUUIDSize = 16;
  if (data.size() != kExpectedUUIDSize) {
    LOG(ERROR) << "UUID size is expected to be " << kExpectedUUIDSize
               << " but is " << data.size() << " and the data in hex is "
               << base::HexEncode(data.data(), data.size());
    return false;
  }

  const std::string hex_encoded =
      base::ToLowerASCII(base::HexEncode(data.data(), data.size()));
  DCHECK_EQ(hex_encoded.size(), kExpectedUUIDSize * 2);
  base::StringPiece all(hex_encoded);
  // Note UUID has 5 parts separated with dashes.
  // e.g. 123e4567-e89b-12d3-a456-426655440000
  // These StringPieces have each part.
  base::StringPiece first = all.substr(0, 8);
  base::StringPiece second = all.substr(8, 4);
  base::StringPiece third = all.substr(12, 4);
  base::StringPiece fourth = all.substr(16, 4);
  base::StringPiece fifth = all.substr(20, 12);

  // 32 hexadecimal characters with 4 hyphens.
  const size_t kHumanReadableUUIDSize = 36;
  uuid_format->reserve(kHumanReadableUUIDSize);
  first.CopyToString(uuid_format);
  uuid_format->append("-");
  second.AppendToString(uuid_format);
  uuid_format->append("-");
  third.AppendToString(uuid_format);
  uuid_format->append("-");
  fourth.AppendToString(uuid_format);
  uuid_format->append("-");
  fifth.AppendToString(uuid_format);
  return true;
}

void UpdateContentProtectionPsshHelper(
    const std::string& drm_uuid,
    const std::string& pssh,
    std::list<ContentProtectionElement>* content_protection_elements) {
  const std::string drm_uuid_schemd_id_uri_form = "urn:uuid:" + drm_uuid;
  for (std::list<ContentProtectionElement>::iterator protection =
           content_protection_elements->begin();
       protection != content_protection_elements->end(); ++protection) {
    if (protection->scheme_id_uri != drm_uuid_schemd_id_uri_form) {
      continue;
    }

    for (std::vector<Element>::iterator subelement =
             protection->subelements.begin();
         subelement != protection->subelements.end(); ++subelement) {
      if (subelement->name == kPsshElementName) {
        // For now, we want to remove the PSSH element because some players do
        // not support updating pssh.
        protection->subelements.erase(subelement);

        // TODO(rkuroiwa): Uncomment this and remove the line above when
        // shaka-player supports updating PSSH.
        // subelement->content = pssh;
        return;
      }
    }

    // Reaching here means <cenc:pssh> does not exist under the
    // ContentProtection element. Add it.
    // TODO(rkuroiwa): Uncomment this when shaka-player supports updating PSSH.
    // Element cenc_pssh;
    // cenc_pssh.name = kPsshElementName;
    // cenc_pssh.content = pssh;
    // protection->subelements.push_back(cenc_pssh);
    return;
  }

  // Reaching here means that ContentProtection for the DRM does not exist.
  // Add it.
  ContentProtectionElement content_protection;
  content_protection.scheme_id_uri = drm_uuid_schemd_id_uri_form;
  // TODO(rkuroiwa): Uncomment this when shaka-player supports updating PSSH.
  // Element cenc_pssh;
  // cenc_pssh.name = kPsshElementName;
  // cenc_pssh.content = pssh;
  // content_protection.subelements.push_back(cenc_pssh);
  content_protection_elements->push_back(content_protection);
  return;
}

namespace {
// Helper function. This works because Representation and AdaptationSet both
// have AddContentProtectionElement().
template <typename ContentProtectionParent>
void AddContentProtectionElementsHelperTemplated(
    const MediaInfo& media_info,
    ContentProtectionParent* parent) {
  DCHECK(parent);
  if (!media_info.has_protected_content())
    return;

  const MediaInfo::ProtectedContent& protected_content =
      media_info.protected_content();

  // DASH MPD spec specifies a default ContentProtection element for ISO BMFF
  // (MP4) files.
  const bool is_mp4_container =
      media_info.container_type() == MediaInfo::CONTAINER_MP4;
  std::string key_id_uuid_format;
  if (protected_content.has_default_key_id()) {
    if (!HexToUUID(protected_content.default_key_id(), &key_id_uuid_format)) {
      LOG(ERROR) << "Failed to convert default key ID into UUID format.";
    }
  }

  if (is_mp4_container) {
    ContentProtectionElement mp4_content_protection;
    mp4_content_protection.scheme_id_uri = kEncryptedMp4Scheme;
    mp4_content_protection.value = protected_content.protection_scheme();
    if (!key_id_uuid_format.empty()) {
      mp4_content_protection.additional_attributes["cenc:default_KID"] =
          key_id_uuid_format;
    }

    parent->AddContentProtectionElement(mp4_content_protection);
  }

  for (int i = 0; i < protected_content.content_protection_entry().size();
       ++i) {
    const MediaInfo::ProtectedContent::ContentProtectionEntry& entry =
        protected_content.content_protection_entry(i);
    if (!entry.has_uuid()) {
      LOG(WARNING)
          << "ContentProtectionEntry was specified but no UUID is set for "
          << entry.name_version() << ", skipping.";
      continue;
    }

    ContentProtectionElement drm_content_protection;
    drm_content_protection.scheme_id_uri = "urn:uuid:" + entry.uuid();
    if (entry.has_name_version())
      drm_content_protection.value = entry.name_version();

    if (entry.has_pssh()) {
      std::string base64_encoded_pssh;
      base::Base64Encode(
          base::StringPiece(entry.pssh().data(), entry.pssh().size()),
          &base64_encoded_pssh);
      Element cenc_pssh;
      cenc_pssh.name = kPsshElementName;
      cenc_pssh.content = base64_encoded_pssh;
      drm_content_protection.subelements.push_back(cenc_pssh);
    }

    if (!key_id_uuid_format.empty() && !is_mp4_container) {
      drm_content_protection.additional_attributes["cenc:default_KID"] =
          key_id_uuid_format;
    }

    parent->AddContentProtectionElement(drm_content_protection);
  }

  LOG_IF(WARNING, protected_content.content_protection_entry().size() == 0)
      << "The media is encrypted but no content protection specified.";
}
}  // namespace

void AddContentProtectionElements(const MediaInfo& media_info,
                                  Representation* parent) {
  AddContentProtectionElementsHelperTemplated(media_info, parent);
}

void AddContentProtectionElements(const MediaInfo& media_info,
                                  AdaptationSet* parent) {
  AddContentProtectionElementsHelperTemplated(media_info, parent);
}


}  // namespace shaka
