// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd
//
// Classes to wrap XML operations. XmlNode is a generic wrapper class for
// XmlNode in libxml2. There are also MPD XML specific classes as well.

#ifndef MPD_BASE_XML_XML_NODE_H_
#define MPD_BASE_XML_XML_NODE_H_

#include <list>

#include "base/basictypes.h"
#include "mpd/base/content_protection_element.h"
#include "mpd/base/media_info.pb.h"
#include "mpd/base/xml/scoped_xml_ptr.h"
#include "third_party/libxml/src/include/libxml/tree.h"

namespace dash_packager {

struct SegmentInfo;

namespace xml {

/// These classes are wrapper classes for XML elements for generating MPD.
/// None of the pointer parameters should be NULL. None of the methods are meant
/// to be overridden.
class XmlNode {
 public:
  /// Make an XML element.
  /// @param name is the name of the element, which should not be NULL.
  explicit XmlNode(const char* name);
  virtual ~XmlNode();

  /// Add a child element to this element.
  /// @param child is a xmlNode to add as a child for this element. Ownership
  ///        of the child node is transferred.
  /// @return true on success, false otherwise.
  bool AddChild(ScopedXmlPtr<xmlNode>::type child);

  /// Set a string attribute.
  /// @param attribute_name The name (lhs) of the attribute.
  /// @param attribute The value (rhs) of the attribute.
  void SetStringAttribute(const char* attribute_name,
                          const std::string& attribute);

  /// Sets an interger attribute.
  /// @param attribute_name The name (lhs) of the attribute.
  /// @param number The value (rhs) of the attribute.
  void SetIntegerAttribute(const char* attribute_name, uint64 number);

  /// Set a floating point number attribute.
  /// @param attribute_name is the name of the attribute to set.
  /// @param number is the value (rhs) of the attribute.
  void SetFloatingPointAttribute(const char* attribute_name, double number);

  /// Sets 'id=@a id' attribute.
  /// @param id is the ID for this element.
  void SetId(uint32 id);

  /// Set the contents of an XML element using a string.
  /// Note: This function does not work well with AddChild(). Use either
  /// AddChild() or SetContent() when setting the content of this node.
  /// @param content is a string containing the text-encoded child elements to
  ///        be added to the element.
  void SetContent(const std::string& content);

  /// Transfer the ownership of the xmlNodePtr. After calling this method, the
  /// behavior of any methods, except the destructor, is undefined.
  /// @return The resource of this object.
  ScopedXmlPtr<xmlNode>::type PassScopedPtr();

  /// Release the xmlNodePtr of this object. After calling this method, the
  /// behavior of any methods, except the destructor, is undefined.
  xmlNodePtr Release();

  /// @return Raw pointer to the element.
  xmlNodePtr GetRawPtr();

 private:
  ScopedXmlPtr<xmlNode>::type node_;

  DISALLOW_COPY_AND_ASSIGN(XmlNode);
};

/// This corresponds to RepresentationBaseType in MPD. RepresentationBaseType is
/// not a concrete element type so this should not get instantiated on its own.
/// AdaptationSet and Representation are subtypes of this.
class RepresentationBaseXmlNode : public XmlNode {
 public:
  virtual ~RepresentationBaseXmlNode();
  bool AddContentProtectionElements(
      const std::list<ContentProtectionElement>& content_protection_elements);

  /// Add a ContentProtection elements to this element.
  /// @param media_info is a MediaInfo containing the ContentProtection
  ///        elements to add.
  /// @return true on success, false otherwise.
  bool AddContentProtectionElementsFromMediaInfo(const MediaInfo& media_info);

 protected:
  explicit RepresentationBaseXmlNode(const char* name);

 private:
  bool AddContentProtectionElement(
      const ContentProtectionElement& content_protection_element);

  DISALLOW_COPY_AND_ASSIGN(RepresentationBaseXmlNode);
};

/// AdaptationSetType specified in MPD.
class AdaptationSetXmlNode : public RepresentationBaseXmlNode {
 public:
  AdaptationSetXmlNode();
  virtual ~AdaptationSetXmlNode();

 private:
  DISALLOW_COPY_AND_ASSIGN(AdaptationSetXmlNode);
};

/// RepresentationType in MPD.
class RepresentationXmlNode : public RepresentationBaseXmlNode {
 public:
  typedef ::google::protobuf::RepeatedPtrField<MediaInfo_VideoInfo>
      RepeatedVideoInfo;

  typedef ::google::protobuf::RepeatedPtrField<MediaInfo_AudioInfo>
      RepeatedAudioInfo;

  RepresentationXmlNode();
  virtual ~RepresentationXmlNode();

  /// Adds video metadata to the MPD.
  /// @param repeated_video_info constains the VideoInfos for Representation.
  ///        Input of size 0 is valid.
  /// @return true if successfully set attributes and children elements (if
  ///         applicable), false otherwise.
  bool AddVideoInfo(const RepeatedVideoInfo& repeated_video_info);

  /// Adds audio metadata to the MPD.
  /// @param repeated_audio_info constains the AudioInfos for Representation.
  ///        Input of size 0 is valid.
  /// @return true if successfully set attributes and children elements (if
  ///         applicable), false otherwise.
  bool AddAudioInfo(const RepeatedAudioInfo& repeated_audio_info);

  /// Adds fields that are specific to VOD. This ignores @a media_info fields for
  /// Live.
  /// @param media_info is a MediaInfo with VOD information.
  /// @return true on success, false otherwise.
  bool AddVODOnlyInfo(const MediaInfo& media_info);

  bool AddLiveOnlyInfo(const MediaInfo& media_info,
                       const std::list<SegmentInfo>& segment_infos);

 private:
  // Add AudioChannelConfiguration elements. This will add multiple
  // AudioChannelConfiguration if @a repeated_audio_info contains multiple
  // distinct channel configs (e.g. 2 channels and 6 channels adds 2 elements).
  bool AddAudioChannelInfo(const RepeatedAudioInfo& repeated_audio_info);

  // Add audioSamplingRate attribute to this element.
  void AddAudioSamplingRateInfo(const RepeatedAudioInfo& repeated_audio_info);

  DISALLOW_COPY_AND_ASSIGN(RepresentationXmlNode);
};

}  // namespace xml
}  // namespace dash_packager
#endif  // MPD_BASE_XML_XML_NODE_H_
