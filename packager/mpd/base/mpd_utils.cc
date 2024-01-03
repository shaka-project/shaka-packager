// Copyright 2014 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/mpd/base/mpd_utils.h>

#include <absl/flags/flag.h>
#include <absl/log/check.h>
#include <absl/log/log.h>
#include <absl/strings/escaping.h>
#include <absl/strings/numbers.h>
#include <absl/strings/str_format.h>
#include <libxml/tree.h>

#include <packager/macros/logging.h>
#include <packager/media/base/language_utils.h>
#include <packager/media/base/protection_system_specific_info.h>
#include <packager/mpd/base/adaptation_set.h>
#include <packager/mpd/base/content_protection_element.h>
#include <packager/mpd/base/representation.h>
#include <packager/mpd/base/xml/scoped_xml_ptr.h>

ABSL_FLAG(
    bool,
    use_legacy_vp9_codec_string,
    false,
    "Use legacy vp9 codec string 'vp9' if set to true; otherwise new style "
    "vp09.xx.xx.xx... codec string will be used. Default to false as indicated "
    "in https://github.com/shaka-project/shaka-packager/issues/406, all major "
    "browsers and platforms already support the new 'vp09' codec string.");

namespace shaka {
namespace {

bool IsKeyRotationDefaultKeyId(const std::string& key_id) {
  for (char c : key_id) {
    if (c != '\0')
      return false;
  }
  return true;
}

std::string TextCodecString(const MediaInfo& media_info) {
  CHECK(media_info.has_text_info());
  const auto container_type = media_info.container_type();

  // Codecs are not needed when mimeType is "text/*". Having a codec would be
  // redundant.
  if (container_type == MediaInfo::CONTAINER_TEXT) {
    return "";
  }

  // DASH IOP mentions that the codec for ttml in mp4 is stpp, so override
  // the default codec value.
  const std::string& codec = media_info.text_info().codec();
  if (codec == "ttml" && container_type == MediaInfo::CONTAINER_MP4) {
    return "stpp";
  }

  return codec;
}

}  // namespace

bool HasVODOnlyFields(const MediaInfo& media_info) {
  return media_info.has_init_range() || media_info.has_index_range() ||
         media_info.has_media_file_url();
}

bool HasLiveOnlyFields(const MediaInfo& media_info) {
  return media_info.has_init_segment_url() ||
         media_info.has_segment_template_url();
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
  return LanguageToShortestForm(lang);
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
      if (absl::GetFlag(FLAGS_use_legacy_vp9_codec_string)) {
        if (codec == "vp09")
          return "vp9";
      }
    }
    return media_info.video_info().codec();
  }

  if (media_info.has_audio_info())
    return media_info.audio_info().codec();

  if (media_info.has_text_info())
    return TextCodecString(media_info);

  NOTIMPLEMENTED();
  return "";
}

std::string GetBaseCodec(const MediaInfo& media_info) {
  std::string codec;
  if (media_info.has_video_info()) {
    codec = media_info.video_info().codec();
  } else if (media_info.has_audio_info()) {
    codec = media_info.audio_info().codec();
  } else if (media_info.has_text_info()) {
    codec = media_info.text_info().codec();
  }
  // Convert, for example, "mp4a.40.2" to simply "mp4a".
  // "mp4a.40.2" and "mp4a.40.5" can exist in the same AdaptationSet.
  size_t dot = codec.find('.');
  if (dot != std::string::npos) {
    codec.erase(dot);
  }
  return codec;
}

std::string GetAdaptationSetKey(const MediaInfo& media_info,
                                bool ignore_codec) {
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
  if (!ignore_codec) {
    key.append(":");
    key.append(GetBaseCodec(media_info));

    if (GetBaseCodec(media_info).find("dvh") == 0) {
      // Transfer characteristics for Dolby Vision (dvh1 or dvhe) must be PQ
      // irrespective of value present in SPS VUI.
      key.append(":");
      key.append(std::to_string(kTransferFunctionPQ));
    } else if (media_info.video_info().has_transfer_characteristics()) {
      key.append(":");
      key.append(
          std::to_string(media_info.video_info().transfer_characteristics()));
    }
  }
  key.append(":");
  key.append(GetLanguage(media_info));

  // Trick play streams of the same original stream, but possibly with
  // different trick_play_factors, belong to the same trick play AdaptationSet.
  if (media_info.video_info().has_playback_rate()) {
    key.append(":trick_play");
  }

  if (!media_info.dash_accessibilities().empty()) {
    key.append(":accessibility_");
    for (const std::string& accessibility : media_info.dash_accessibilities())
      key.append(accessibility);
  }

  if (!media_info.dash_roles().empty()) {
    key.append(":roles_");
    for (const std::string& role : media_info.dash_roles())
      key.append(role);
  }

  return key;
}

std::string FloatToXmlString(double number) {
  // Keep up to microsecond accuracy but trim trailing 0s
  std::string formatted = absl::StrFormat("%.6g", number);
  size_t decimalPos = formatted.find('.');
  if (decimalPos != std::string::npos) {
    size_t lastNonZeroPos = formatted.find_last_not_of('0');
    if (lastNonZeroPos >= decimalPos) {
      formatted.erase(lastNonZeroPos + 1);
    }
    if (formatted.back() == '.') {
      formatted.pop_back();
    }
  }

  return formatted;
}

std::string SecondsToXmlDuration(double seconds) {
  // Chrome internally uses time accurate to microseconds, which is implemented
  // per MSE spec (https://www.w3.org/TR/media-source/).
  // We need a string formatter that has at least microseconds accuracy for a
  // normal video (with duration up to 3 hours). FloatToXmlString
  // implementation meets the requirement.
  return absl::StrFormat("PT%sS", FloatToXmlString(seconds));
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
  if (!absl::SimpleAtod(reinterpret_cast<const char*>(duration_value.get()),
                        &duration_double_precision)) {
    return false;
  }

  *duration = static_cast<float>(duration_double_precision);
  return true;
}

bool MoreThanOneTrue(bool b1, bool b2, bool b3) {
  return (b1 && b2) || (b2 && b3) || (b3 && b1);
}

bool AtLeastOneTrue(bool b1, bool b2, bool b3) {
  return b1 || b2 || b3;
}

bool OnlyOneTrue(bool b1, bool b2, bool b3) {
  return !MoreThanOneTrue(b1, b2, b3) && AtLeastOneTrue(b1, b2, b3);
}

// Coverts binary data into human readable UUID format.
bool HexToUUID(const std::string& data, std::string* uuid_format) {
  DCHECK(uuid_format);
  const size_t kExpectedUUIDSize = 16;
  if (data.size() != kExpectedUUIDSize) {
    LOG(ERROR) << "UUID size is expected to be " << kExpectedUUIDSize
               << " but is " << data.size() << " and the data in hex is "
               << absl::BytesToHexString(data);
    return false;
  }

  const std::string hex_encoded =
      absl::AsciiStrToLower(absl::BytesToHexString(data));
  DCHECK_EQ(hex_encoded.size(), kExpectedUUIDSize * 2);
  std::string_view all(hex_encoded);
  // Note UUID has 5 parts separated with dashes.
  // e.g. 123e4567-e89b-12d3-a456-426655440000
  // These StringPieces have each part.
  std::string_view first = all.substr(0, 8);
  std::string_view second = all.substr(8, 4);
  std::string_view third = all.substr(12, 4);
  std::string_view fourth = all.substr(16, 4);
  std::string_view fifth = all.substr(20, 12);

  // 32 hexadecimal characters with 4 hyphens.
  const size_t kHumanReadableUUIDSize = 36;
  uuid_format->reserve(kHumanReadableUUIDSize);
  absl::StrAppendFormat(uuid_format, "%s-%s-%s-%s-%s", first, second, third,
                        fourth, fifth);
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

// UUID for Marlin Adaptive Streaming Specification – Simple Profile from
// https://dashif.org/identifiers/content_protection/.
const char kMarlinUUID[] = "5e629af5-38da-4063-8977-97ffbd9902d4";
// String representation of media::kFairPlaySystemId.
const char kFairPlayUUID[] = "94ce86fb-07ff-4f43-adb8-93d2fa968ca2";
// String representation of media::kPlayReadySystemId.
const char kPlayReadyUUID[] = "9a04f079-9840-4286-ab92-e65be0885f95";
// It is RECOMMENDED to include the @value attribute with name and version
// "MSPR 2.0". See
// https://docs.microsoft.com/en-us/playready/specifications/mpeg-dash-playready#221-general.
const char kContentProtectionValueMSPR20[] = "MSPR 2.0";

Element GenerateMarlinContentIds(const std::string& key_id) {
  // See https://github.com/shaka-project/shaka-packager/issues/381 for details.
  static const char kMarlinContentIdName[] = "mas:MarlinContentId";
  static const char kMarlinContentIdPrefix[] = "urn:marlin:kid:";
  static const char kMarlinContentIdsName[] = "mas:MarlinContentIds";

  Element marlin_content_id;
  marlin_content_id.name = kMarlinContentIdName;
  marlin_content_id.content =
      kMarlinContentIdPrefix +
      absl::AsciiStrToLower(absl::BytesToHexString(key_id));

  Element marlin_content_ids;
  marlin_content_ids.name = kMarlinContentIdsName;
  marlin_content_ids.subelements.push_back(marlin_content_id);

  return marlin_content_ids;
}

Element GenerateCencPsshElement(const std::string& pssh) {
  std::string base64_encoded_pssh;
  absl::Base64Escape(std::string_view(pssh.data(), pssh.size()),
                     &base64_encoded_pssh);
  Element cenc_pssh;
  cenc_pssh.name = kPsshElementName;
  cenc_pssh.content = base64_encoded_pssh;
  return cenc_pssh;
}

// Extract MS PlayReady Object from given PSSH
// and encode it in base64.
Element GenerateMsprProElement(const std::string& pssh) {
  std::unique_ptr<media::PsshBoxBuilder> b =
    media::PsshBoxBuilder::ParseFromBox(
        reinterpret_cast<const uint8_t*>(pssh.data()),
        pssh.size()
    );

  const std::vector<uint8_t> *p_pssh = &b->pssh_data();
  std::string base64_encoded_mspr;
  absl::Base64Escape(
      std::string_view(reinterpret_cast<const char*>(p_pssh->data()),
                       p_pssh->size()),
      &base64_encoded_mspr);
  Element mspr_pro;
  mspr_pro.name = kMsproElementName;
  mspr_pro.content = base64_encoded_mspr;
  return mspr_pro;
}

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
  if (protected_content.has_default_key_id() &&
      !IsKeyRotationDefaultKeyId(protected_content.default_key_id())) {
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

  for (const auto& entry : protected_content.content_protection_entry()) {
    if (!entry.has_uuid()) {
      LOG(WARNING)
          << "ContentProtectionEntry was specified but no UUID is set for "
          << entry.name_version() << ", skipping.";
      continue;
    }

    ContentProtectionElement drm_content_protection;

    if (entry.has_name_version())
      drm_content_protection.value = entry.name_version();

    if (entry.uuid() == kFairPlayUUID) {
      VLOG(1) << "Skipping FairPlay ContentProtection element as FairPlay does "
                 "not support DASH signaling.";
      continue;
    } else if (entry.uuid() == kMarlinUUID) {
      // Marlin requires its uuid to be in upper case. See #525 for details.
      drm_content_protection.scheme_id_uri =
          "urn:uuid:" + absl::AsciiStrToUpper(entry.uuid());
      drm_content_protection.subelements.push_back(
          GenerateMarlinContentIds(protected_content.default_key_id()));
    } else {
      drm_content_protection.scheme_id_uri = "urn:uuid:" + entry.uuid();
      if (!entry.pssh().empty()) {
        drm_content_protection.subelements.push_back(
            GenerateCencPsshElement(entry.pssh()));
        if(entry.uuid() == kPlayReadyUUID && protected_content.include_mspr_pro()) {
          drm_content_protection.subelements.push_back(
              GenerateMsprProElement(entry.pssh()));
          drm_content_protection.value = kContentProtectionValueMSPR20;
        }
      }
    }

    if (!key_id_uuid_format.empty() && !is_mp4_container) {
      drm_content_protection.additional_attributes["cenc:default_KID"] =
          key_id_uuid_format;
    }

    parent->AddContentProtectionElement(drm_content_protection);
  }

  if (protected_content.content_protection_entry().size() == 0) {
    VLOG(1) << "The media is encrypted but no content protection specified "
            << "(can happen with key rotation).";
  }
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
