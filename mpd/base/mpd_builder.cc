#include "mpd/base/mpd_builder.h"

#include <string>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "mpd/base/content_protection_element.h"
#include "mpd/base/mpd_utils.h"
#include "mpd/base/xml/xml_node.h"
#include "third_party/libxml/src/include/libxml/tree.h"

// TODO(rkuroiwa): If performance is a problem work on fine grained locking.
namespace dash_packager {

using xml::XmlNode;
using xml::RepresentationXmlNode;
using xml::AdaptationSetXmlNode;

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

bool MpdBuilder::WriteMpd() {
  base::AutoLock scoped_lock(lock_);
  std::string mpd;
  bool result = ToStringImpl(&mpd);

  // TODO(rkuroiwa): Write to file, after interface change.
  return result;
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

// TODO(rkuroiwa): This function is too big.
xmlDocPtr MpdBuilder::GenerateMpd() {
  // Setup nodes.
  static const char kXmlVersion[] = "1.0";
  xml::ScopedXmlPtr<xmlDoc>::type doc(xmlNewDoc(BAD_CAST kXmlVersion));
  XmlNode mpd("MPD");

  static const char kXmlSchema[] = "http://www.w3.org/2001/XMLSchema-instance";
  mpd.SetStringAttribute("xmlns:xsi", kXmlSchema);

  static const char kDashSchemaMpd2011[] =
      "urn:mpeg:DASH:schema:MPD:2011 DASH-MPD.xsd";
  mpd.SetStringAttribute("xsi:schemaLocation", kDashSchemaMpd2011);

  static const char kMinBufferTimeTwoSeconds[] = "PT2S";
  mpd.SetStringAttribute("minBufferTime", kMinBufferTimeTwoSeconds);

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

uint32 MpdBuilder::GetStaticMpdDuration(XmlNode* mpd_node) {
  DCHECK(mpd_node);
  DCHECK_EQ(MpdBuilder::kStatic, type_);

  xmlNodePtr period_node = xmlFirstElementChild(mpd_node->GetRawPtr());
  DCHECK(period_node);
  DCHECK_NE(strcmp(reinterpret_cast<const char*>(period_node->name), "Period"),
            0);

  // TODO(rkuroiwa): Update this so that the duration for each Representation is
  // (duration / timescale).
  // Attribute mediaPresentationDuration must be present for 'static' MPD. So
  // setting "P0S" is still required if none of the representaions had a
  // duration attribute.
  uint32 max_duration = 0;
  for (xmlNodePtr adaptation_set = xmlFirstElementChild(period_node);
       adaptation_set;
       adaptation_set = xmlNextElementSibling(adaptation_set)) {
    for (xmlNodePtr representation = xmlFirstElementChild(adaptation_set);
         representation;
         representation = xmlNextElementSibling(representation)) {
      uint32 duration = 0;
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
  if (HasVODOnlyFields(media_info) && HasLiveOnlyFields(media_info)) {
    LOG(ERROR) << "MediaInfo cannot have both VOD and Live fields.";
    return NULL;
  }

  if (!media_info.has_bandwidth()) {
    LOG(ERROR) << "MediaInfo missing required bandwidth field.";
    return NULL;
  }

  scoped_ptr<Representation> representation(
      new Representation(media_info, representation_counter_->GetNext()));

  DCHECK(representation);
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

// TODO(rkuroiwa): We don't need to create a node every single time. Make an
// internal copy of this element.
// Uses info in |media_info_| and |content_protection_elements_| to create a
// "Representation" node.
xml::ScopedXmlPtr<xmlNode>::type Representation::GetXml() {
  base::AutoLock scoped_lock(lock_);
  DCHECK(!(HasVODOnlyFields(media_info_) && HasLiveOnlyFields(media_info_)));
  DCHECK(media_info_.has_bandwidth());

  RepresentationXmlNode representation;
  if (!representation.AddContentProtectionElements(
           content_protection_elements_)) {
    return xml::ScopedXmlPtr<xmlNode>::type();
  }

  // Two 'Mandatory' fields for Representation.
  representation.SetId(id_);
  representation.SetNumberAttribute("bandwidth", media_info_.bandwidth());

  const bool has_video_info = media_info_.video_info_size() > 0;
  const bool has_audio_info = media_info_.audio_info_size() > 0;
  if (has_video_info || has_audio_info) {
    const std::string codecs = GetCodecs(media_info_);
    if (!codecs.empty())
      representation.SetStringAttribute("codecs", codecs);

    if (has_video_info) {
      if (!representation.AddVideoInfo(media_info_.video_info()))
        return xml::ScopedXmlPtr<xmlNode>::type();
    }

    if (has_audio_info) {
      if (!representation.AddAudioInfo(media_info_.audio_info()))
        return xml::ScopedXmlPtr<xmlNode>::type();
    }
  }

  // TODO(rkuroiwa): Add TextInfo.
  // TODO(rkuroiwa): Add ContentProtection info.
  if (HasVODOnlyFields(media_info_)) {
    if (!representation.AddVODOnlyInfo(media_info_))
      return xml::ScopedXmlPtr<xmlNode>::type();
  }

  // TODO(rkuroiwa): Handle Live case. Handle data in
  // segment_starttime_duration_pairs_.
  return representation.PassScopedPtr();
}

}  // namespace dash_packager
