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

/// This class generates DASH MPDs (Media Presentation Descriptions).
class MpdBuilder {
 public:
  enum MpdType {
    kStatic = 0,
    kDynamic
  };

  /// Constructs MpdBuilder.
  /// @param type indicates whether the MPD should be for VOD or live content
  ///        (kStatic for VOD profile, or kDynamic for live profile).
  explicit MpdBuilder(MpdType type);
  ~MpdBuilder();

  /// Add <BaseURL> entry to the MPD.
  /// @param base_url URL for <BaseURL> entry.
  void AddBaseUrl(const std::string& base_url);

  /// Adds <AdaptationSet> to the MPD.
  /// @return The new adaptation set, which is owned by this instance.
  AdaptationSet* AddAdaptationSet();

  /// Writes the MPD to the given string.
  /// @param[out] output is an output string where the MPD gets written.
  /// @return true on success, false otherwise.
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

/// AdaptationSet class provides methods to add Representations and
/// <ContentProtection> elements to the AdaptationSet element.
class AdaptationSet {
 public:
  /// @param adaptation_set_id is an ID number for this AdaptationSet.
  /// @param representation_counter is a Counter for assigning ID numbers to
  ///                               Representation. It can not be NULL.
  AdaptationSet(uint32 adaptation_set_id,
                base::AtomicSequenceNumber* representation_counter);
  ~AdaptationSet();

  /// Create a Representation instance using @a media_info.
  /// @param media_info is a MediaInfo object used to initialize the returned
  ///        Representation instance.
  /// @return On success, returns a pointer to Representation. Otherwise returns
  ///         NULL. The returned pointer is owned by the AdaptationSet instance.
  Representation* AddRepresentation(const MediaInfo& media_info);

  /// Add a ContenProtection element to the adaptation set.
  /// @param element contains the ContentProtection element contents.
  ///        If @a element has {value, schemeIdUri} set and has
  ///        {“value”, “schemeIdUri”} as key for @a additional_attributes,
  ///        then the former is used.
  void AddContentProtectionElement(const ContentProtectionElement& element);

  /// Makes a copy of AdaptationSet xml element with its child Representation
  /// and ContentProtection elements.
  /// @return On success returns a non-NULL ScopedXmlPtr. Otherwise returns a
  ///         NULL ScopedXmlPtr.
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

/// Representation class contains references to a single media stream, as
/// well as optional ContentProtection elements for that stream.
class Representation {
 public:
  /// @param media_info is a MediaInfo containing information on the media.
  /// @param representation_id is the numeric ID for the <Representation>.
  Representation(const MediaInfo& media_info, uint32 representation_id);
  ~Representation();

  /// Tries to initialize the instance. If this does not succeed, the instance
  /// should not be used.
  /// @return true on success, false otherwise.
  bool Init();

  /// Add a ContenProtection element to the representation.
  /// @param element contains the ContentProtection element contents.
  ///        If @a element has {value, schemeIdUri} set and has
  ///        {“value”, “schemeIdUri”} as key for @a additional_attributes,
  ///        then the former is used.
  void AddContentProtectionElement(const ContentProtectionElement& element);

  /// Add a media segment to the representation.
  /// @param start_time is the start time for the segment, in units of the
  ///        stream's time scale.
  /// @param duration is the duration of the segment, in units of the stream's
  ///        time scale.
  /// @return true on success, false otherwise.
  bool AddNewSegment(uint64 start_time, uint64 duration);

  /// @return Copy of <Representation>.
  xml::ScopedXmlPtr<xmlNode>::type GetXml();

  /// @return ID number for <Representation>.
  uint32 id() const {
    return id_;
  }

 private:
  // Returns true if |media_info_| has required fields to generate a valid
  // Representation. Otherwise returns false.
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
