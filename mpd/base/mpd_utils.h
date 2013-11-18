// Funtions used by MpdBuilder class to generate an MPD file.
#ifndef MPD_BASE_MPD_UTILS_H_
#define MPD_BASE_MPD_UTILS_H_

#include <string>

#include "base/basictypes.h"
#include "mpd/base/media_info.pb.h"
#include "third_party/libxml/src/include/libxml/tree.h"

namespace dash_packager {

class ContentProtectionElement;

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

// TODO(rkuroiwa): This probably needs to change to floating point number.
// Returns "P<seconds>S".
std::string SecondsToXmlDuration(uint32 seconds);

// Tries to get "duration" attribute from |node|. On success |duration| is set.
bool GetDurationAttribute(xmlNodePtr node, uint32* duration);

}  // namespace dash_packager

#endif  // MPD_BASE_MPD_UTILS_H_
