// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/mpd/base/xml/xml_node.h"

#include <gflags/gflags.h>

#include <limits>
#include <set>

#include "packager/base/logging.h"
#include "packager/base/macros.h"
#include "packager/base/strings/string_number_conversions.h"
#include "packager/base/sys_byteorder.h"
#include "packager/mpd/base/media_info.pb.h"
#include "packager/mpd/base/mpd_utils.h"
#include "packager/mpd/base/segment_info.h"

DEFINE_bool(segment_template_constant_duration,
            false,
            "Generates SegmentTemplate@duration if all segments except the "
            "last one has the same duration if this flag is set to true.");

namespace shaka {

using xml::XmlNode;
typedef MediaInfo::AudioInfo AudioInfo;
typedef MediaInfo::VideoInfo VideoInfo;

namespace {
const char kEC3Codec[] = "ec-3";

std::string RangeToString(const Range& range) {
  return base::Uint64ToString(range.begin()) + "-" +
         base::Uint64ToString(range.end());
}

// Check if segments are continuous and all segments except the last one are of
// the same duration.
bool IsTimelineConstantDuration(const std::list<SegmentInfo>& segment_infos,
                                uint32_t start_number) {
  if (!FLAGS_segment_template_constant_duration)
    return false;

  DCHECK(!segment_infos.empty());
  if (segment_infos.size() > 2)
    return false;

  const SegmentInfo& first_segment = segment_infos.front();
  if (first_segment.start_time != first_segment.duration * (start_number - 1))
    return false;

  if (segment_infos.size() == 1)
    return true;

  const SegmentInfo& last_segment = segment_infos.back();
  if (last_segment.repeat != 0)
    return false;

  const int64_t expected_last_segment_start_time =
      first_segment.start_time +
      first_segment.duration * (first_segment.repeat + 1);
  return expected_last_segment_start_time == last_segment.start_time;
}

bool PopulateSegmentTimeline(const std::list<SegmentInfo>& segment_infos,
                             XmlNode* segment_timeline) {
  for (const SegmentInfo& segment_info : segment_infos) {
    XmlNode s_element("S");
    s_element.SetIntegerAttribute("t", segment_info.start_time);
    s_element.SetIntegerAttribute("d", segment_info.duration);
    if (segment_info.repeat > 0)
      s_element.SetIntegerAttribute("r", segment_info.repeat);

    CHECK(segment_timeline->AddChild(s_element.PassScopedPtr()));
  }

  return true;
}

void CollectNamespaceFromName(const std::string& name,
                              std::set<std::string>* namespaces) {
  const size_t pos = name.find(':');
  if (pos != std::string::npos)
    namespaces->insert(name.substr(0, pos));
}

void TraverseAttrsAndCollectNamespaces(const xmlAttr* attr,
                                       std::set<std::string>* namespaces) {
  for (const xmlAttr* cur_attr = attr; cur_attr; cur_attr = cur_attr->next) {
    CollectNamespaceFromName(reinterpret_cast<const char*>(cur_attr->name),
                             namespaces);
  }
}

void TraverseNodesAndCollectNamespaces(const xmlNode* node,
                                       std::set<std::string>* namespaces) {
  for (const xmlNode* cur_node = node; cur_node; cur_node = cur_node->next) {
    CollectNamespaceFromName(reinterpret_cast<const char*>(cur_node->name),
                             namespaces);

    TraverseNodesAndCollectNamespaces(cur_node->children, namespaces);
    TraverseAttrsAndCollectNamespaces(cur_node->properties, namespaces);
  }
}

}  // namespace

namespace xml {

XmlNode::XmlNode(const char* name) : node_(xmlNewNode(NULL, BAD_CAST name)) {
  DCHECK(name);
  DCHECK(node_);
}

XmlNode::~XmlNode() {}

bool XmlNode::AddChild(scoped_xml_ptr<xmlNode> child) {
  DCHECK(node_);
  DCHECK(child);
  if (!xmlAddChild(node_.get(), child.get()))
    return false;

  // Reaching here means the ownership of |child| transfered to |node_|.
  // Release the pointer so that it doesn't get destructed in this scope.
  ignore_result(child.release());
  return true;
}

bool XmlNode::AddElements(const std::vector<Element>& elements) {
  for (size_t element_index = 0; element_index < elements.size();
       ++element_index) {
    const Element& child_element = elements[element_index];
    XmlNode child_node(child_element.name.c_str());
    for (std::map<std::string, std::string>::const_iterator attribute_it =
             child_element.attributes.begin();
         attribute_it != child_element.attributes.end(); ++attribute_it) {
      child_node.SetStringAttribute(attribute_it->first.c_str(),
                                    attribute_it->second);
    }

    // Note that somehow |SetContent| needs to be called before |AddElements|
    // otherwise the added children will be overwritten by the content.
    child_node.SetContent(child_element.content);

    // Recursively set children for the child.
    if (!child_node.AddElements(child_element.subelements))
      return false;

    if (!xmlAddChild(node_.get(), child_node.GetRawPtr())) {
      LOG(ERROR) << "Failed to set child " << child_element.name
                 << " to parent element "
                 << reinterpret_cast<const char*>(node_->name);
      return false;
    }
    // Reaching here means the ownership of |child_node| transfered to |node_|.
    // Release the pointer so that it doesn't get destructed in this scope.
    ignore_result(child_node.Release());
  }
  return true;
}

void XmlNode::SetStringAttribute(const char* attribute_name,
                                 const std::string& attribute) {
  DCHECK(node_);
  DCHECK(attribute_name);
  xmlSetProp(node_.get(), BAD_CAST attribute_name, BAD_CAST attribute.c_str());
}

void XmlNode::SetIntegerAttribute(const char* attribute_name, uint64_t number) {
  DCHECK(node_);
  DCHECK(attribute_name);
  xmlSetProp(node_.get(),
             BAD_CAST attribute_name,
             BAD_CAST (base::Uint64ToString(number).c_str()));
}

void XmlNode::SetFloatingPointAttribute(const char* attribute_name,
                                        double number) {
  DCHECK(node_);
  DCHECK(attribute_name);
  xmlSetProp(node_.get(), BAD_CAST attribute_name,
             BAD_CAST(base::DoubleToString(number).c_str()));
}

void XmlNode::SetId(uint32_t id) {
  SetIntegerAttribute("id", id);
}

void XmlNode::SetContent(const std::string& content) {
  DCHECK(node_);
  xmlNodeSetContent(node_.get(), BAD_CAST content.c_str());
}

std::set<std::string> XmlNode::ExtractReferencedNamespaces() {
  std::set<std::string> namespaces;
  TraverseNodesAndCollectNamespaces(node_.get(), &namespaces);
  return namespaces;
}

scoped_xml_ptr<xmlNode> XmlNode::PassScopedPtr() {
  DVLOG(2) << "Passing node_.";
  DCHECK(node_);
  return std::move(node_);
}

xmlNodePtr XmlNode::Release() {
  DVLOG(2) << "Releasing node_.";
  DCHECK(node_);
  return node_.release();
}

xmlNodePtr XmlNode::GetRawPtr() {
  return node_.get();
}

RepresentationBaseXmlNode::RepresentationBaseXmlNode(const char* name)
    : XmlNode(name) {}
RepresentationBaseXmlNode::~RepresentationBaseXmlNode() {}

bool RepresentationBaseXmlNode::AddContentProtectionElements(
    const std::list<ContentProtectionElement>& content_protection_elements) {
  std::list<ContentProtectionElement>::const_iterator content_protection_it =
      content_protection_elements.begin();
  for (; content_protection_it != content_protection_elements.end();
       ++content_protection_it) {
    if (!AddContentProtectionElement(*content_protection_it))
      return false;
  }

  return true;
}

void RepresentationBaseXmlNode::AddSupplementalProperty(
    const std::string& scheme_id_uri,
    const std::string& value) {
  XmlNode supplemental_property("SupplementalProperty");
  supplemental_property.SetStringAttribute("schemeIdUri", scheme_id_uri);
  supplemental_property.SetStringAttribute("value", value);
  AddChild(supplemental_property.PassScopedPtr());
}

void RepresentationBaseXmlNode::AddEssentialProperty(
    const std::string& scheme_id_uri,
    const std::string& value) {
  XmlNode essential_property("EssentialProperty");
  essential_property.SetStringAttribute("schemeIdUri", scheme_id_uri);
  essential_property.SetStringAttribute("value", value);
  AddChild(essential_property.PassScopedPtr());
}

bool RepresentationBaseXmlNode::AddContentProtectionElement(
    const ContentProtectionElement& content_protection_element) {
  XmlNode content_protection_node("ContentProtection");

  // @value is an optional attribute.
  if (!content_protection_element.value.empty()) {
    content_protection_node.SetStringAttribute(
        "value", content_protection_element.value);
  }
  content_protection_node.SetStringAttribute(
      "schemeIdUri", content_protection_element.scheme_id_uri);

  typedef std::map<std::string, std::string> AttributesMapType;
  const AttributesMapType& additional_attributes =
      content_protection_element.additional_attributes;

  AttributesMapType::const_iterator attributes_it =
      additional_attributes.begin();
  for (; attributes_it != additional_attributes.end(); ++attributes_it) {
    content_protection_node.SetStringAttribute(attributes_it->first.c_str(),
                                               attributes_it->second);
  }

  if (!content_protection_node.AddElements(
          content_protection_element.subelements)) {
    return false;
  }
  return AddChild(content_protection_node.PassScopedPtr());
}

AdaptationSetXmlNode::AdaptationSetXmlNode()
    : RepresentationBaseXmlNode("AdaptationSet") {}
AdaptationSetXmlNode::~AdaptationSetXmlNode() {}

void AdaptationSetXmlNode::AddRoleElement(const std::string& scheme_id_uri,
                                          const std::string& value) {
  XmlNode role("Role");
  role.SetStringAttribute("schemeIdUri", scheme_id_uri);
  role.SetStringAttribute("value", value);
  AddChild(role.PassScopedPtr());
}

RepresentationXmlNode::RepresentationXmlNode()
    : RepresentationBaseXmlNode("Representation") {}
RepresentationXmlNode::~RepresentationXmlNode() {}

bool RepresentationXmlNode::AddVideoInfo(const VideoInfo& video_info,
                                         bool set_width,
                                         bool set_height,
                                         bool set_frame_rate) {
  if (!video_info.has_width() || !video_info.has_height()) {
    LOG(ERROR) << "Missing width or height for adding a video info.";
    return false;
  }

  if (video_info.has_pixel_width() && video_info.has_pixel_height()) {
    SetStringAttribute("sar", base::IntToString(video_info.pixel_width()) +
                                  ":" +
                                  base::IntToString(video_info.pixel_height()));
  }

  if (set_width)
    SetIntegerAttribute("width", video_info.width());
  if (set_height)
    SetIntegerAttribute("height", video_info.height());
  if (set_frame_rate) {
    SetStringAttribute("frameRate",
                       base::IntToString(video_info.time_scale()) + "/" +
                           base::IntToString(video_info.frame_duration()));
  }

  if (video_info.has_playback_rate()) {
    SetStringAttribute("maxPlayoutRate",
                       base::IntToString(video_info.playback_rate()));
    // Since the trick play stream contains only key frames, there is no coding
    // dependency on the main stream. Simply set the codingDependency to false.
    // TODO(hmchen): propagate this attribute up to the AdaptationSet, since
    // all are set to false.
    SetStringAttribute("codingDependency", "false");
  }
  return true;
}

bool RepresentationXmlNode::AddAudioInfo(const AudioInfo& audio_info) {
  if (!AddAudioChannelInfo(audio_info))
    return false;

  AddAudioSamplingRateInfo(audio_info);
  return true;
}

bool RepresentationXmlNode::AddVODOnlyInfo(const MediaInfo& media_info) {
  if (media_info.has_media_file_url()) {
    XmlNode base_url("BaseURL");
    base_url.SetContent(media_info.media_file_url());

    if (!AddChild(base_url.PassScopedPtr()))
      return false;
  }

  const bool need_segment_base = media_info.has_index_range() ||
                                 media_info.has_init_range() ||
                                 media_info.has_reference_time_scale();

  if (need_segment_base) {
    XmlNode segment_base("SegmentBase");
    if (media_info.has_index_range()) {
      segment_base.SetStringAttribute("indexRange",
                                      RangeToString(media_info.index_range()));
    }

    if (media_info.has_reference_time_scale()) {
      segment_base.SetIntegerAttribute("timescale",
                                       media_info.reference_time_scale());
    }

    if (media_info.has_presentation_time_offset()) {
      segment_base.SetIntegerAttribute("presentationTimeOffset",
                                       media_info.presentation_time_offset());
    }

    if (media_info.has_init_range()) {
      XmlNode initialization("Initialization");
      initialization.SetStringAttribute("range",
                                        RangeToString(media_info.init_range()));

      if (!segment_base.AddChild(initialization.PassScopedPtr()))
        return false;
    }

    if (!AddChild(segment_base.PassScopedPtr()))
      return false;
  }

  return true;
}

bool RepresentationXmlNode::AddLiveOnlyInfo(
    const MediaInfo& media_info,
    const std::list<SegmentInfo>& segment_infos,
    uint32_t start_number) {
  XmlNode segment_template("SegmentTemplate");
  if (media_info.has_reference_time_scale()) {
    segment_template.SetIntegerAttribute("timescale",
                                         media_info.reference_time_scale());
  }

  if (media_info.has_presentation_time_offset()) {
    segment_template.SetIntegerAttribute("presentationTimeOffset",
                                         media_info.presentation_time_offset());
  }

  if (media_info.has_init_segment_url()) {
    segment_template.SetStringAttribute("initialization",
                                        media_info.init_segment_url());
  }

  if (media_info.has_segment_template_url()) {
    segment_template.SetStringAttribute("media",
                                        media_info.segment_template_url());
    segment_template.SetIntegerAttribute("startNumber", start_number);
  }

  if (!segment_infos.empty()) {
    // Don't use SegmentTimeline if all segments except the last one are of
    // the same duration.
    if (IsTimelineConstantDuration(segment_infos, start_number)) {
      segment_template.SetIntegerAttribute("duration",
                                           segment_infos.front().duration);
    } else {
      XmlNode segment_timeline("SegmentTimeline");
      if (!PopulateSegmentTimeline(segment_infos, &segment_timeline) ||
          !segment_template.AddChild(segment_timeline.PassScopedPtr())) {
        return false;
      }
    }
  }
  return AddChild(segment_template.PassScopedPtr());
}

bool RepresentationXmlNode::AddAudioChannelInfo(const AudioInfo& audio_info) {
  std::string audio_channel_config_scheme;
  std::string audio_channel_config_value;

  if (audio_info.codec() == kEC3Codec) {
    // Convert EC3 channel map into string of hexadecimal digits. Spec: DASH-IF
    // Interoperability Points v3.0 9.2.1.2.
    const uint16_t ec3_channel_map =
        base::HostToNet16(audio_info.codec_specific_data().ec3_channel_map());
    audio_channel_config_value =
        base::HexEncode(&ec3_channel_map, sizeof(ec3_channel_map));
    audio_channel_config_scheme =
        "tag:dolby.com,2014:dash:audio_channel_configuration:2011";
  } else {
    audio_channel_config_value = base::UintToString(audio_info.num_channels());
    audio_channel_config_scheme =
        "urn:mpeg:dash:23003:3:audio_channel_configuration:2011";
  }

  XmlNode audio_channel_config("AudioChannelConfiguration");
  audio_channel_config.SetStringAttribute("schemeIdUri",
                                          audio_channel_config_scheme);
  audio_channel_config.SetStringAttribute("value", audio_channel_config_value);

  return AddChild(audio_channel_config.PassScopedPtr());
}

// MPD expects one number for sampling frequency, or if it is a range it should
// be space separated.
void RepresentationXmlNode::AddAudioSamplingRateInfo(
    const AudioInfo& audio_info) {
  if (audio_info.has_sampling_frequency())
    SetIntegerAttribute("audioSamplingRate", audio_info.sampling_frequency());
}

}  // namespace xml
}  // namespace shaka
