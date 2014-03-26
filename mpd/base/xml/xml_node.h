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
namespace xml {

// These classes are wrapper classes for XML elements for generating MPD.
// None of the pointer parameters should be NULL. None of the methods are meant
// to be overridden.
class XmlNode {
 public:
  // Makes an XML element with |name|. |name| should not be NULL.
  explicit XmlNode(const char* name);
  virtual ~XmlNode();

  // The ownership transfers to this object. On failure, this method will
  // destruct |child|.
  bool AddChild(ScopedXmlPtr<xmlNode>::type child);

  void SetStringAttribute(const char* attribute_name,
                          const std::string& attribute);

  void SetIntegerAttribute(const char* attribute_name, uint64 number);
  void SetFloatingPointAttribute(const char* attribute_name, double number);

  void SetId(uint32 id);

  // This is like 'innerHTML' setter.
  // This function does not work well with AddChild(). Use either AddChild() or
  // SetContent() when setting the content of this node.
  void SetContent(const std::string& content);

  // Ownership transfer. After calling this method, calling any methods of
  // this object, except the destructor, is undefined.
  ScopedXmlPtr<xmlNode>::type PassScopedPtr();

  // Release the xmlNodePtr of this object. After calling this method, calling
  // any methods of this object, except the destructor, is undefined.
  xmlNodePtr Release();

  xmlNodePtr GetRawPtr();

 private:
  ScopedXmlPtr<xmlNode>::type node_;

  DISALLOW_COPY_AND_ASSIGN(XmlNode);
};

// This corresponds to RepresentationBaseType in MPD. RepresentationBaseType is
// not a concrete element type so this should not get instantiated on its own.
// AdaptationSet and Representation are subtypes of this.
class RepresentationBaseXmlNode : public XmlNode {
 public:
  virtual ~RepresentationBaseXmlNode();
  bool AddContentProtectionElements(
      const std::list<ContentProtectionElement>& content_protection_elements);

  // Return true on success. If content_protections size is 0 in |media_info|,
  // this will return true. Otherwise return false.
  bool AddContentProtectionElementsFromMediaInfo(const MediaInfo& media_info);

 protected:
  explicit RepresentationBaseXmlNode(const char* name);

 private:
  bool AddContentProtectionElement(
      const ContentProtectionElement& content_protection_element);

  DISALLOW_COPY_AND_ASSIGN(RepresentationBaseXmlNode);
};

// AdaptationSetType in MPD.
class AdaptationSetXmlNode : public RepresentationBaseXmlNode {
 public:
  AdaptationSetXmlNode();
  virtual ~AdaptationSetXmlNode();

 private:
  DISALLOW_COPY_AND_ASSIGN(AdaptationSetXmlNode);
};

// RepresentationType in MPD.
class RepresentationXmlNode : public RepresentationBaseXmlNode {
 public:
  typedef ::google::protobuf::RepeatedPtrField<MediaInfo_VideoInfo>
      RepeatedVideoInfo;

  typedef ::google::protobuf::RepeatedPtrField<MediaInfo_AudioInfo>
      RepeatedAudioInfo;

  RepresentationXmlNode();
  virtual ~RepresentationXmlNode();

  // Returns true if successfully set attributes and children elements (if
  // applicable). repeated info of size 0 is valid input.
  bool AddVideoInfo(const RepeatedVideoInfo& repeated_video_info);
  bool AddAudioInfo(const RepeatedAudioInfo& repeated_audio_info);

  // Check MediaInfo protobuf definition for which fields are specific to VOD.
  bool AddVODOnlyInfo(const MediaInfo& media_info);

 private:
  // Add AudioChannelConfiguration elements. This will add multiple
  // AudioChannelConfiguration if |repeated_audio_info| contains multiple
  // distinct channel configs (e.g. 2 channels and 6 channels adds 2 elements).
  bool AddAudioChannelInfo(const RepeatedAudioInfo& repeated_audio_info);

  // Add audioSamplingRate attribute to this element.
  void AddAudioSamplingRateInfo(const RepeatedAudioInfo& repeated_audio_info);

  DISALLOW_COPY_AND_ASSIGN(RepresentationXmlNode);
};

}  // namespace xml
}  // namespace dash_packager
#endif  // MPD_BASE_XML_XML_NODE_H_
