// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "mpd/base/mpd_builder.h"

#include <string>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "mpd/base/content_protection_element.h"
#include "mpd/base/mpd_utils.h"
#include "mpd/base/xml/xml_node.h"
#include "third_party/libxml/src/include/libxml/tree.h"
#include "third_party/libxml/src/include/libxml/xmlstring.h"

namespace dash_packager {

using xml::XmlNode;
using xml::RepresentationXmlNode;
using xml::AdaptationSetXmlNode;

namespace {

std::string GetMimeType(
    const std::string& prefix,
    MediaInfo::ContainerType container_type) {
  switch (container_type) {
    case MediaInfo::CONTAINER_MP4:
      return prefix + "/mp4";
    case MediaInfo::CONTAINER_MPEG2_TS:
      // NOTE: DASH MPD spec uses lowercase but RFC3555 says uppercase.
      return prefix + "/MP2T";
    case MediaInfo::CONTAINER_WEBM:
      return prefix + "/webm";
    default:
      break;
  }

  // Unsupported container types should be rejected/handled by the caller.
  NOTREACHED() << "Unrecognized container type: " << container_type;
  return std::string();
}

void AddMpdNameSpaceInfo(XmlNode* mpd) {
  DCHECK(mpd);

  static const char kXmlNamespace[] = "urn:mpeg:DASH:schema:MPD:2011";
  mpd->SetStringAttribute("xmlns", kXmlNamespace);
  static const char kXmlNamespaceXsi[] = "http://www.w3.org/2001/XMLSchema-instance";
  mpd->SetStringAttribute("xmlns:xsi", kXmlNamespaceXsi);
  static const char kXmlNamespaceXlink[] = "http://www.w3.org/1999/xlink";
  mpd->SetStringAttribute("xmlns:xlink", kXmlNamespaceXlink);
  static const char kDashSchemaMpd2011[] =
      "urn:mpeg:DASH:schema:MPD:2011 DASH-MPD.xsd";
  mpd->SetStringAttribute("xsi:schemaLocation", kDashSchemaMpd2011);
}

bool IsPeriodNode(xmlNodePtr node) {
  DCHECK(node);
  int kEqual = 0;
  return xmlStrcmp(node->name, reinterpret_cast<const xmlChar*>("Period")) ==
         kEqual;
}

// Find the first <Period> element. This does not recurse down the tree,
// only checks direct children. Returns the pointer to Period element on
// success, otherwise returns false.
// As noted here, we must traverse.
// http://www.xmlsoft.org/tutorial/ar01s04.html
xmlNodePtr FindPeriodNode(XmlNode* xml_node) {
  for (xmlNodePtr node = xml_node->GetRawPtr()->xmlChildrenNode;
       node != NULL;
       node = node->next) {
    if (IsPeriodNode(node))
      return node;
  }

  return NULL;
}

}  // namespace

MpdBuilder::MpdBuilder(MpdType type)
    : type_(type),
      adaptation_sets_deleter_(&adaptation_sets_) {}

MpdBuilder::~MpdBuilder() {}

void MpdBuilder::AddBaseUrl(const std::string& base_url) {
  base::AutoLock scoped_lock(lock_);
  base_urls_.push_back(base_url);
}

AdaptationSet* MpdBuilder::AddAdaptationSet() {
  base::AutoLock scoped_lock(lock_);
  scoped_ptr<AdaptationSet> adaptation_set(new AdaptationSet(
      adaptation_set_counter_.GetNext(), &representation_counter_));

  DCHECK(adaptation_set);
  adaptation_sets_.push_back(adaptation_set.get());
  return adaptation_set.release();
}

bool MpdBuilder::ToString(std::string* output) {
  base::AutoLock scoped_lock(lock_);
  return ToStringImpl(output);
}

bool MpdBuilder::ToStringImpl(std::string* output) {
  xmlInitParser();
  xml::ScopedXmlPtr<xmlDoc>::type doc(GenerateMpd());
  if (!doc.get())
    return false;

  static const int kNiceFormat = 1;
  int doc_str_size = 0;
  xmlChar* doc_str = NULL;
  xmlDocDumpFormatMemoryEnc(
      doc.get(), &doc_str, &doc_str_size, "UTF-8", kNiceFormat);

  output->assign(doc_str, doc_str + doc_str_size);
  xmlFree(doc_str);

  DLOG(INFO) << *output;

  // Cleanup, free the doc then cleanup parser.
  doc.reset();
  xmlCleanupParser();
  return true;
}

xmlDocPtr MpdBuilder::GenerateMpd() {
  // Setup nodes.
  static const char kXmlVersion[] = "1.0";
  xml::ScopedXmlPtr<xmlDoc>::type doc(xmlNewDoc(BAD_CAST kXmlVersion));
  XmlNode mpd("MPD");
  AddMpdNameSpaceInfo(&mpd);

  const float kMinBufferTime = 2.0f;
  mpd.SetStringAttribute("minBufferTime", SecondsToXmlDuration(kMinBufferTime));

  // Iterate thru AdaptationSets and add them to one big Period element.
  XmlNode period("Period");
  std::list<AdaptationSet*>::iterator adaptation_sets_it =
      adaptation_sets_.begin();
  for (; adaptation_sets_it != adaptation_sets_.end(); ++adaptation_sets_it) {
    xml::ScopedXmlPtr<xmlNode>::type child((*adaptation_sets_it)->GetXml());
    if (!child.get() || !period.AddChild(child.Pass()))
      return NULL;
  }

  // Add baseurls to MPD.
  std::list<std::string>::const_iterator base_urls_it = base_urls_.begin();
  for (; base_urls_it != base_urls_.end(); ++base_urls_it) {
    XmlNode base_url("BaseURL");
    base_url.SetContent(*base_urls_it);

    if (!mpd.AddChild(base_url.PassScopedPtr()))
      return NULL;
  }

  if (!mpd.AddChild(period.PassScopedPtr()))
    return NULL;

  switch (type_) {
    case kStatic:
      AddStaticMpdInfo(&mpd);
      break;
    case kDynamic:
      NOTIMPLEMENTED() << "MPD for live is not implemented.";
      break;
    default:
      NOTREACHED() << "Unknown MPD type: " << type_;
      break;
  }

  DCHECK(doc);
  xmlDocSetRootElement(doc.get(), mpd.Release());
  return doc.release();
}

void MpdBuilder::AddStaticMpdInfo(XmlNode* mpd_node) {
  DCHECK(mpd_node);
  DCHECK_EQ(MpdBuilder::kStatic, type_);

  static const char kStaticMpdType[] = "static";
  static const char kStaticMpdProfile[] =
      "urn:mpeg:dash:profile:isoff-on-demand:2011";
  mpd_node->SetStringAttribute("type", kStaticMpdType);
  mpd_node->SetStringAttribute("profiles", kStaticMpdProfile);
  mpd_node->SetStringAttribute(
      "mediaPresentationDuration",
      SecondsToXmlDuration(GetStaticMpdDuration(mpd_node)));
}

float MpdBuilder::GetStaticMpdDuration(XmlNode* mpd_node) {
  DCHECK(mpd_node);
  DCHECK_EQ(MpdBuilder::kStatic, type_);

  xmlNodePtr period_node = FindPeriodNode(mpd_node);
  DCHECK(period_node) << "Period element must be a child of mpd_node.";
  DCHECK(IsPeriodNode(period_node));

  // Attribute mediaPresentationDuration must be present for 'static' MPD. So
  // setting "PT0S" is required even if none of the representaions have duration
  // attribute.
  float max_duration = 0.0f;
  for (xmlNodePtr adaptation_set = xmlFirstElementChild(period_node);
       adaptation_set;
       adaptation_set = xmlNextElementSibling(adaptation_set)) {
    for (xmlNodePtr representation = xmlFirstElementChild(adaptation_set);
         representation;
         representation = xmlNextElementSibling(representation)) {
      float duration = 0.0f;
      if (GetDurationAttribute(representation, &duration)) {
        max_duration = max_duration > duration ? max_duration : duration;

        // 'duration' attribute is there only to help generate MPD, not
        // necessary for MPD, remove the attribute.
        xmlUnsetProp(representation, BAD_CAST "duration");
      }
    }
  }

  return max_duration;
}

AdaptationSet::AdaptationSet(uint32 adaptation_set_id,
                             base::AtomicSequenceNumber* counter)
    : representations_deleter_(&representations_),
      representation_counter_(counter),
      id_(adaptation_set_id) {
  DCHECK(counter);
}

AdaptationSet::~AdaptationSet() {}

Representation* AdaptationSet::AddRepresentation(const MediaInfo& media_info) {
  base::AutoLock scoped_lock(lock_);
  scoped_ptr<Representation> representation(
      new Representation(media_info, representation_counter_->GetNext()));

  if (!representation->Init())
    return NULL;

  representations_.push_back(representation.get());
  return representation.release();
}

void AdaptationSet::AddContentProtectionElement(
    const ContentProtectionElement& content_protection_element) {
  base::AutoLock scoped_lock(lock_);
  content_protection_elements_.push_back(content_protection_element);
  RemoveDuplicateAttributes(&content_protection_elements_.back());
}

// Creates a copy of <AdaptationSet> xml element, iterate thru all the
// <Representation> (child) elements and add them to the copy.
xml::ScopedXmlPtr<xmlNode>::type AdaptationSet::GetXml() {
  base::AutoLock scoped_lock(lock_);
  AdaptationSetXmlNode adaptation_set;

  if (!adaptation_set.AddContentProtectionElements(
           content_protection_elements_)) {
    return xml::ScopedXmlPtr<xmlNode>::type();
  }

  std::list<Representation*>::iterator representation_it =
      representations_.begin();

  for (; representation_it != representations_.end(); ++representation_it) {
    xml::ScopedXmlPtr<xmlNode>::type child((*representation_it)->GetXml());
    if (!child.get() || !adaptation_set.AddChild(child.Pass()))
      return xml::ScopedXmlPtr<xmlNode>::type();
  }

  adaptation_set.SetId(id_);
  return adaptation_set.PassScopedPtr();
}

Representation::Representation(const MediaInfo& media_info, uint32 id)
    : media_info_(media_info), id_(id) {}

Representation::~Representation() {}

bool Representation::Init() {
  if (!HasRequiredMediaInfoFields())
    return false;

  codecs_ = GetCodecs(media_info_);
  if (codecs_.empty()) {
    LOG(ERROR) << "Missing codec info in MediaInfo.";
    return false;
  }

  const bool has_video_info = media_info_.video_info_size() > 0;
  const bool has_audio_info = media_info_.audio_info_size() > 0;

  if (!has_video_info && !has_audio_info) {
    // This is an error. Segment information can be in AdaptationSet, Period, or
    // MPD but the interface does not provide a way to set them.
    // See 5.3.9.1 ISO 23009-1:2012 for segment info.
    LOG(ERROR) << "Representation needs video or audio.";
    return false;
  }

  if (media_info_.container_type() == MediaInfo::CONTAINER_UNKNOWN) {
    LOG(ERROR) << "'container_type' in MediaInfo cannot be CONTAINER_UNKNOWN.";
    return false;
  }

  // Check video and then audio. Usually when there is audio + video, we take
  // video/<type>.
  if (has_video_info) {
    mime_type_ = GetVideoMimeType();
  } else if (has_audio_info) {
    mime_type_ = GetAudioMimeType();
  }

  return true;
}

void Representation::AddContentProtectionElement(
    const ContentProtectionElement& content_protection_element) {
  base::AutoLock scoped_lock(lock_);
  content_protection_elements_.push_back(content_protection_element);
  RemoveDuplicateAttributes(&content_protection_elements_.back());
}

bool Representation::AddNewSegment(uint64 start_time, uint64 duration) {
  base::AutoLock scoped_lock(lock_);
  segment_starttime_duration_pairs_.push_back(
      std::pair<uint64, uint64>(start_time, duration));
  return true;
}

// Uses info in |media_info_| and |content_protection_elements_| to create a
// "Representation" node.
// MPD schema has strict ordering. The following must be done in order.
// AddVideoInfo() (possibly adds FramePacking elements), AddAudioInfo() (Adds
// AudioChannelConfig elements), AddContentProtectionElements*(), and
// AddVODOnlyInfo() (Adds segment info).
xml::ScopedXmlPtr<xmlNode>::type Representation::GetXml() {
  base::AutoLock scoped_lock(lock_);
  DCHECK(!(HasVODOnlyFields(media_info_) && HasLiveOnlyFields(media_info_)));
  DCHECK(media_info_.has_bandwidth());

  RepresentationXmlNode representation;
  // Mandatory fields for Representation.
  representation.SetId(id_);
  representation.SetIntegerAttribute("bandwidth", media_info_.bandwidth());
  representation.SetStringAttribute("codecs", codecs_);
  representation.SetStringAttribute("mimeType", mime_type_);

  const bool has_video_info = media_info_.video_info_size() > 0;
  const bool has_audio_info = media_info_.audio_info_size() > 0;

  if (has_video_info &&
      !representation.AddVideoInfo(media_info_.video_info())) {
    LOG(ERROR) << "Failed to add video info to Representation XML.";
    return xml::ScopedXmlPtr<xmlNode>::type();
  }

  if (has_audio_info &&
      !representation.AddAudioInfo(media_info_.audio_info())) {
    LOG(ERROR) << "Failed to add audio info to Representation XML.";
    return xml::ScopedXmlPtr<xmlNode>::type();
  }

  if (!representation.AddContentProtectionElements(
           content_protection_elements_)) {
    return xml::ScopedXmlPtr<xmlNode>::type();
  }
  if (!representation.AddContentProtectionElementsFromMediaInfo(media_info_))
    return xml::ScopedXmlPtr<xmlNode>::type();

  if (HasVODOnlyFields(media_info_) &&
      !representation.AddVODOnlyInfo(media_info_)) {
    LOG(ERROR) << "Failed to add VOD segment info.";
    return xml::ScopedXmlPtr<xmlNode>::type();
  }

  return representation.PassScopedPtr();
}

bool Representation::HasRequiredMediaInfoFields() {
  if (HasVODOnlyFields(media_info_) && HasLiveOnlyFields(media_info_)) {
    LOG(ERROR) << "MediaInfo cannot have both VOD and Live fields.";
    return false;
  }

  if (!media_info_.has_bandwidth()) {
    LOG(ERROR) << "MediaInfo missing required field: bandwidth.";
    return false;
  }

  if (!media_info_.has_container_type()) {
    LOG(ERROR) << "MediaInfo missing required field: container_type.";
    return false;
  }

  return true;
}

std::string Representation::GetVideoMimeType() const {
  return GetMimeType("video", media_info_.container_type());
}

std::string Representation::GetAudioMimeType() const {
  return GetMimeType("audio", media_info_.container_type());
}

}  // namespace dash_packager
