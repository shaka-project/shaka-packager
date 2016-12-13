// Copyright 2015 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/mpd/base/dash_iop_mpd_notifier.h"

#include "packager/base/stl_util.h"
#include "packager/mpd/base/media_info.pb.h"
#include "packager/mpd/base/mpd_notifier_util.h"
#include "packager/mpd/base/mpd_utils.h"

namespace shaka {

namespace {

// The easiest way to check whether two protobufs are equal, is to compare the
// serialized version.
bool ProtectedContentEq(
    const MediaInfo::ProtectedContent& content_protection1,
    const MediaInfo::ProtectedContent& content_protection2) {
  return content_protection1.SerializeAsString() ==
         content_protection2.SerializeAsString();
}

std::set<std::string> GetUUIDs(
    const MediaInfo::ProtectedContent& protected_content) {
  std::set<std::string> uuids;
  for (int i = 0; i < protected_content.content_protection_entry().size();
       ++i) {
    const MediaInfo::ProtectedContent::ContentProtectionEntry& entry =
        protected_content.content_protection_entry(i);
    uuids.insert(entry.uuid());
  }
  return uuids;
}

}  // namespace

DashIopMpdNotifier::DashIopMpdNotifier(
    DashProfile dash_profile,
    const MpdOptions& mpd_options,
    const std::vector<std::string>& base_urls,
    const std::string& output_path)
    : MpdNotifier(dash_profile),
      output_path_(output_path),
      mpd_builder_(new MpdBuilder(dash_profile == kLiveProfile
                                      ? MpdBuilder::kDynamic
                                      : MpdBuilder::kStatic,
                                  mpd_options)) {
  DCHECK(dash_profile == kLiveProfile || dash_profile == kOnDemandProfile);
  for (size_t i = 0; i < base_urls.size(); ++i)
    mpd_builder_->AddBaseUrl(base_urls[i]);
}

DashIopMpdNotifier::~DashIopMpdNotifier() {}

bool DashIopMpdNotifier::Init() {
  return true;
}

bool DashIopMpdNotifier::NotifyNewContainer(const MediaInfo& media_info,
                                            uint32_t* container_id) {
  DCHECK(container_id);

  ContentType content_type = GetContentType(media_info);
  if (content_type == kContentTypeUnknown)
    return false;

  base::AutoLock auto_lock(lock_);
  const std::string key = GetAdaptationSetKey(media_info);
  AdaptationSet* adaptation_set = GetAdaptationSetForMediaInfo(key, media_info);
  DCHECK(adaptation_set);
  if (media_info.has_text_info()) {
    // IOP requires all AdaptationSets to have (sub)segmentAlignment set to
    // true, so carelessly set it to true.
    // In practice it doesn't really make sense to adapt between text tracks.
    adaptation_set->ForceSetSegmentAlignment(true);
  }

  MediaInfo adjusted_media_info(media_info);
  MpdBuilder::MakePathsRelativeToMpd(output_path_, &adjusted_media_info);
  Representation* representation =
      adaptation_set->AddRepresentation(adjusted_media_info);
  if (!representation)
    return false;

  representation_id_to_adaptation_set_[representation->id()] = adaptation_set;

  SetAdaptationSetSwitching(key, adaptation_set);

  *container_id = representation->id();
  DCHECK(!ContainsKey(representation_map_, representation->id()));
  representation_map_[representation->id()] = representation;
  return true;
}

bool DashIopMpdNotifier::NotifySampleDuration(uint32_t container_id,
                                              uint32_t sample_duration) {
  base::AutoLock auto_lock(lock_);
  RepresentationMap::iterator it = representation_map_.find(container_id);
  if (it == representation_map_.end()) {
    LOG(ERROR) << "Unexpected container_id: " << container_id;
    return false;
  }
  it->second->SetSampleDuration(sample_duration);
  return true;
}

bool DashIopMpdNotifier::NotifyNewSegment(uint32_t container_id,
                                          uint64_t start_time,
                                          uint64_t duration,
                                          uint64_t size) {
  base::AutoLock auto_lock(lock_);
  RepresentationMap::iterator it = representation_map_.find(container_id);
  if (it == representation_map_.end()) {
    LOG(ERROR) << "Unexpected container_id: " << container_id;
    return false;
  }
  it->second->AddNewSegment(start_time, duration, size);
  return true;
}

bool DashIopMpdNotifier::NotifyEncryptionUpdate(
    uint32_t container_id,
    const std::string& drm_uuid,
    const std::vector<uint8_t>& new_key_id,
    const std::vector<uint8_t>& new_pssh) {
  base::AutoLock auto_lock(lock_);
  RepresentationMap::iterator it = representation_map_.find(container_id);
  if (it == representation_map_.end()) {
    LOG(ERROR) << "Unexpected container_id: " << container_id;
    return false;
  }

  AdaptationSet* adaptation_set_for_representation =
      representation_id_to_adaptation_set_[it->second->id()];
  adaptation_set_for_representation->UpdateContentProtectionPssh(
      drm_uuid, Uint8VectorToBase64(new_pssh));
  return true;
}

bool DashIopMpdNotifier::AddContentProtectionElement(
    uint32_t container_id,
    const ContentProtectionElement& content_protection_element) {
  // Intentionally not implemented because if a Representation gets a new
  // <ContentProtection> element, then it might require moving the
  // Representation out of the AdaptationSet. There's no logic to do that
  // yet.
  return false;
}

bool DashIopMpdNotifier::Flush() {
  base::AutoLock auto_lock(lock_);
  return WriteMpdToFile(output_path_, mpd_builder_.get());
}

AdaptationSet* DashIopMpdNotifier::GetAdaptationSetForMediaInfo(
    const std::string& key,
    const MediaInfo& media_info) {
  std::list<AdaptationSet*>& adaptation_sets = adaptation_set_list_map_[key];
  if (adaptation_sets.empty())
    return NewAdaptationSet(media_info, &adaptation_sets);

  const bool has_protected_content = media_info.has_protected_content();

  for (std::list<AdaptationSet*>::const_iterator adaptation_set_it =
           adaptation_sets.begin();
       adaptation_set_it != adaptation_sets.end(); ++adaptation_set_it) {
    ProtectedContentMap::const_iterator protected_content_it =
        protected_content_map_.find((*adaptation_set_it)->id());

    // If the AdaptationSet ID is not registered in the map, then it is clear
    // content (or encrypted but doesn't need <ContentProtection> element
    // possibly because the player knows how to handle it).
    if (protected_content_it == protected_content_map_.end()) {
      // Can reuse the AdaptationSet without content protection.
      if (!has_protected_content)
        return *adaptation_set_it;
      continue;
    }

    if (ProtectedContentEq(protected_content_it->second,
                           media_info.protected_content())) {
      // Content protection info matches. Reuse the AdaptationSet.
      return *adaptation_set_it;
    }
  }

  // None of the adaptation sets match with the new content protection.
  // Need a new one.
  return NewAdaptationSet(media_info, &adaptation_sets);
}

// Get all the UUIDs of the AdaptationSet. If another AdaptationSet has the
// same UUIDs then those are switchable.
void DashIopMpdNotifier::SetAdaptationSetSwitching(
    const std::string& key,
    AdaptationSet* adaptation_set) {
  // This adaptation set is already visited.
  if (!adaptation_set->adaptation_set_switching_ids().empty())
    return;

  ProtectedContentMap::const_iterator protected_content_it =
      protected_content_map_.find(adaptation_set->id());
  // Clear contents should be in one AdaptationSet and may not be switchable
  // with encrypted contents.
  if (protected_content_it == protected_content_map_.end()) {
    DVLOG(1) << "No content protection set for AdaptationSet@id="
             << adaptation_set->id();
    return;
  }

  // Get all the UUIDs of the ContentProtections in AdaptationSet.
  std::set<std::string> adaptation_set_uuids =
      GetUUIDs(protected_content_it->second);

  std::list<AdaptationSet*>& same_type_adapatation_sets =
      adaptation_set_list_map_[key];
  DCHECK(!same_type_adapatation_sets.empty())
      << "same_type_adapatation_sets should not be null, it should at least "
         "contain adaptation_set";

  for (std::list<AdaptationSet*>::iterator adaptation_set_it =
           same_type_adapatation_sets.begin();
       adaptation_set_it != same_type_adapatation_sets.end();
       ++adaptation_set_it) {
    const uint32_t loop_adaptation_set_id = (*adaptation_set_it)->id();
    if (loop_adaptation_set_id == adaptation_set->id() ||
        !ContainsKey(protected_content_map_, loop_adaptation_set_id)) {
      continue;
    }

    const MediaInfo::ProtectedContent& loop_protected_content =
        protected_content_map_[loop_adaptation_set_id];
    if (static_cast<int>(adaptation_set_uuids.size()) !=
        loop_protected_content.content_protection_entry().size()) {
      // Different number of UUIDs, may not be switchable.
      continue;
    }

    if (adaptation_set_uuids == GetUUIDs(loop_protected_content)) {
      AdaptationSet& uuid_match_adaptation_set = **adaptation_set_it;
      // They match. These AdaptationSets are switchable.
      uuid_match_adaptation_set.AddAdaptationSetSwitching(adaptation_set->id());
      adaptation_set->AddAdaptationSetSwitching(uuid_match_adaptation_set.id());
    }
  }
}

AdaptationSet* DashIopMpdNotifier::NewAdaptationSet(
    const MediaInfo& media_info,
    std::list<AdaptationSet*>* adaptation_sets) {
  std::string language = GetLanguage(media_info);
  AdaptationSet* new_adaptation_set = mpd_builder_->AddAdaptationSet(language);
  if (media_info.has_protected_content()) {
    DCHECK(!ContainsKey(protected_content_map_, new_adaptation_set->id()));
    protected_content_map_[new_adaptation_set->id()] =
        media_info.protected_content();
    AddContentProtectionElements(media_info, new_adaptation_set);
  }
  adaptation_sets->push_back(new_adaptation_set);

  if (media_info.has_video_info()) {
    // Because 'lang' is ignored for videos, |adaptation_sets| must have
    // all the video AdaptationSets.
    if (adaptation_sets->size() > 2) {
      new_adaptation_set->AddRole(AdaptationSet::kRoleMain);
    } else if (adaptation_sets->size() == 2) {
      // Set "main" Role for both AdaptatoinSets.
      (*adaptation_sets->begin())->AddRole(AdaptationSet::kRoleMain);
      new_adaptation_set->AddRole(AdaptationSet::kRoleMain);
    }
  }
  return new_adaptation_set;
}

}  // namespace shaka
