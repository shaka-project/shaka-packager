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

#include <string>

namespace edash_packager {

class MediaInfo;
struct ContentProtectionElement;
struct SegmentInfo;

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

}  // namespace edash_packager

#endif  // MPD_BASE_MPD_UTILS_H_
