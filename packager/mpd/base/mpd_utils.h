// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd
//
// Funtions used by MpdBuilder class to generate an MPD file.

#ifndef MPD_BASE_MPD_UTILS_H_
#define MPD_BASE_MPD_UTILS_H_

#include <libxml/tree.h>

#include <list>
#include <string>

#include "packager/base/base64.h"
#include "packager/base/strings/string_util.h"
#include "packager/mpd/base/content_protection_element.h"
#include "packager/mpd/base/media_info.pb.h"
#include "packager/mpd/base/mpd_builder.h"

namespace edash_packager {

class MediaInfo;
struct ContentProtectionElement;
struct SegmentInfo;

const char kEncryptedMp4Scheme[] = "urn:mpeg:dash:mp4protection:2011";
const char kPsshElementName[] = "cenc:pssh";
const char kEncryptedMp4Value[] = "cenc";

bool HasVODOnlyFields(const MediaInfo& media_info);

bool HasLiveOnlyFields(const MediaInfo& media_info);

// If |content_protection_element| has 'value' or 'schemeIdUri' set but it's
// also in the map, then this removes them from the map.
// |content_protection_element| cannot be NULL.
void RemoveDuplicateAttributes(
    ContentProtectionElement* content_protection_element);

// Returns a 'codecs' string that has all the video and audio codecs joined with
// comma.
std::string GetCodecs(const MediaInfo& media_info);

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

// Update the <cenc:pssh> element for MP4 specific ContentProtection element.
// If the element does not exist, this will add one.
void UpdateContentProtectionPsshHelper(
    const std::string& pssh,
    std::list<ContentProtectionElement>* conetent_protection_elements);

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

}  // namespace edash_packager

#endif  // MPD_BASE_MPD_UTILS_H_
