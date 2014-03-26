// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd
//
// This file contains the MpdBuilder, AdaptationSet, and Representation class
// declarations.
// http://goo.gl/UrsSlF

#ifndef MPD_BASE_MPD_BUILDER_H_
#define MPD_BASE_MPD_BUILDER_H_

#include <list>
#include <string>

#include "base/atomic_sequence_num.h"
#include "base/basictypes.h"
#include "base/stl_util.h"
#include "base/synchronization/lock.h"
#include "mpd/base/content_protection_element.h"
#include "mpd/base/media_info.pb.h"
#include "mpd/base/mpd_utils.h"
#include "mpd/base/xml/scoped_xml_ptr.h"

namespace dash_packager {

class AdaptationSet;
class Representation;

namespace xml {

class XmlNode;

}  // namespace xml

class MpdBuilder {
 public:
  enum MpdType {
    kStatic = 0,
    kDynamic
  };

  explicit MpdBuilder(MpdType type);
  ~MpdBuilder();

  void AddBaseUrl(const std::string& base_url);

  // The returned pointer is owned by this object.
  AdaptationSet* AddAdaptationSet();

  // This will write to stdout until File interface is defined.
  bool WriteMpd();
  bool ToString(std::string* output);

 private:
  bool ToStringImpl(std::string* output);

  // Returns the document pointer to the MPD. This must be freed by the caller
  // using appropriate xmlDocPtr freeing function.
  // On failure, this returns NULL.
  xmlDocPtr GenerateMpd();

  // Adds 'static' MPD attributes and elements to |mpd_node|. This assumes that
  // the first child element is a Period element.
  void AddStaticMpdInfo(xml::XmlNode* mpd_node);
  float GetStaticMpdDuration(xml::XmlNode* mpd_node);

  MpdType type_;
  std::list<AdaptationSet*> adaptation_sets_;
  ::STLElementDeleter<std::list<AdaptationSet*> > adaptation_sets_deleter_;

  std::list<std::string> base_urls_;

  base::Lock lock_;
  base::AtomicSequenceNumber adaptation_set_counter_;
  base::AtomicSequenceNumber representation_counter_;

  DISALLOW_COPY_AND_ASSIGN(MpdBuilder);
};

class AdaptationSet {
 public:
  AdaptationSet(uint32 adaptation_set_id,
                base::AtomicSequenceNumber* representation_counter);
  ~AdaptationSet();

  // The returned pointer is owned by this object.
  Representation* AddRepresentation(const MediaInfo& media_info);

  // If |element| has {value, schemeIdUri} set and has
  // {“value”, “schemeIdUri”} as key for additional_attributes,
  // then the former is used.
  void AddContentProtectionElement(const ContentProtectionElement& element);

  // Makes a copy of AdaptationSet xml element with its child elements, which
  // are Representation elements. On success this returns non-NULL ScopedXmlPtr,
  // otherwise returns NULL ScopedXmlPtr.
  xml::ScopedXmlPtr<xmlNode>::type GetXml();

  // Must be unique in the Period.
  uint32 id() const {
    return id_;
  }

 private:
  std::list<ContentProtectionElement> content_protection_elements_;
  std::list<Representation*> representations_;
  ::STLElementDeleter<std::list<Representation*> > representations_deleter_;

  base::Lock lock_;

  base::AtomicSequenceNumber* const representation_counter_;

  const uint32 id_;

  DISALLOW_COPY_AND_ASSIGN(AdaptationSet);
};

// In |media_info|, ContentProtectionXml::{schemeIdUri,value} takes precedence
// over schemeIdUri and value specified in ContentProtectionXml::attributes.
class Representation {
 public:
  Representation(const MediaInfo& media_info, uint32 representation_id);
  ~Representation();

  bool Init();

  // If |element| has {value, schemeIdUri} set and has
  // {“value”, “schemeIdUri”} as key for additional_attributes,
  // then the former is used.
  void AddContentProtectionElement(const ContentProtectionElement& element);

  bool AddNewSegment(uint64 start_time, uint64 duration);

  // Makes a copy of the current XML. Note that this is a copy. The caller is
  // responsible for cleaning up the allocated resource.
  xml::ScopedXmlPtr<xmlNode>::type GetXml();

  // Must be unique amongst other Representations in the MPD file.
  // As the MPD spec says.
  uint32 id() const {
    return id_;
  }

 private:
  // Returns whether |media_info_| has required fields to generate a valid
  // Representation. Returns true on success, otherwise returns false.
  bool HasRequiredMediaInfoFields();

  // Note: Because 'mimeType' is a required field for a valid MPD, these return
  // strings.
  std::string GetVideoMimeType() const;
  std::string GetAudioMimeType() const;

  MediaInfo media_info_;
  std::list<ContentProtectionElement> content_protection_elements_;
  std::list<std::pair<uint64, uint64> > segment_starttime_duration_pairs_;

  base::Lock lock_;

  const uint32 id_;
  std::string mime_type_;
  std::string codecs_;

  DISALLOW_COPY_AND_ASSIGN(Representation);
};

}  // namespace dash_packager

#endif  // MPD_BASE_MPD_BUILDER_H_
