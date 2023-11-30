// Copyright 2014 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd
//
// Funtions used by MpdBuilder class to generate an MPD file.

#ifndef MPD_BASE_MPD_UTILS_H_
#define MPD_BASE_MPD_UTILS_H_

#include <cstdint>
#include <list>
#include <string>

#include <libxml/tree.h>

namespace shaka {

class AdaptationSet;
class MediaInfo;
class Representation;
struct ContentProtectionElement;
struct SegmentInfo;

const char kEncryptedMp4Scheme[] = "urn:mpeg:dash:mp4protection:2011";
const char kPsshElementName[] = "cenc:pssh";
const char kMsproElementName[] = "mspr:pro";
const uint32_t kTransferFunctionPQ = 16;

bool HasVODOnlyFields(const MediaInfo& media_info);

bool HasLiveOnlyFields(const MediaInfo& media_info);

// If |content_protection_element| has 'value' or 'schemeIdUri' set but it's
// also in the map, then this removes them from the map.
// |content_protection_element| cannot be NULL.
void RemoveDuplicateAttributes(
    ContentProtectionElement* content_protection_element);

// Returns a language in ISO-639 shortest form. May be blank for video.
std::string GetLanguage(const MediaInfo& media_info);

// Returns a 'codecs' string that has all the video and audio codecs joined with
// comma.
std::string GetCodecs(const MediaInfo& media_info);

// Returns a codec string without variants. For example, "mp4a" instead of
// "mp4a.40.2". May return a format for text streams.
std::string GetBaseCodec(const MediaInfo& media_info);

// Returns a key made from the characteristics that separate AdaptationSets.
std::string GetAdaptationSetKey(const MediaInfo& media_info, bool ignore_codec);

std::string FloatToXmlString(double number);

std::string SecondsToXmlDuration(double seconds);

// Tries to get "duration" attribute from |node|. On success |duration| is set.
bool GetDurationAttribute(xmlNodePtr node, float* duration);

bool MoreThanOneTrue(bool b1, bool b2, bool b3);
bool AtLeastOneTrue(bool b1, bool b2, bool b3);
bool OnlyOneTrue(bool b1, bool b2, bool b3);

/// Converts hex data to UUID format. Hex data must be size 16.
/// @param data input hex data.
/// @param uuid_format is the UUID format of the input.
bool HexToUUID(const std::string& data, std::string* uuid_format);

// Update the <cenc:pssh> element for |drm_uuid| ContentProtection element.
// If the element does not exist, this will add one.
void UpdateContentProtectionPsshHelper(
    const std::string& drm_uuid,
    const std::string& pssh,
    std::list<ContentProtectionElement>* content_protection_elements);

/// Adds <ContentProtection> elements specified by @a media_info to
/// @a adaptation_set.
/// Note that this will add the elements as direct chlidren of AdaptationSet.
/// @param media_info may or may not have protected_content field.
/// @param adaptation_set is the parent element that owns the ContentProtection
///        elements.
void AddContentProtectionElements(const MediaInfo& media_info,
                                  Representation* parent);

/// Adds <ContentProtection> elements specified by @a media_info to
/// @a representation.
/// @param media_info may or may not have protected_content field.
/// @param representation is the parent element that owns the ContentProtection
///        elements.
void AddContentProtectionElements(const MediaInfo& media_info,
                                  AdaptationSet* parent);

}  // namespace shaka

#endif  // MPD_BASE_MPD_UTILS_H_
