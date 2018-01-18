// Copyright 2017 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/mpd/base/period.h"

#include "packager/base/stl_util.h"
#include "packager/mpd/base/adaptation_set.h"
#include "packager/mpd/base/mpd_options.h"
#include "packager/mpd/base/mpd_utils.h"
#include "packager/mpd/base/xml/xml_node.h"

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
  for (const auto& entry : protected_content.content_protection_entry())
    uuids.insert(entry.uuid());
  return uuids;
}

}  // namespace

Period::Period(uint32_t period_id,
               double start_time_in_seconds,
               const MpdOptions& mpd_options,
               base::AtomicSequenceNumber* adaptation_set_counter,
               base::AtomicSequenceNumber* representation_counter)
    : id_(period_id),
      start_time_in_seconds_(start_time_in_seconds),
      mpd_options_(mpd_options),
      adaptation_set_counter_(adaptation_set_counter),
      representation_counter_(representation_counter) {}

AdaptationSet* Period::GetOrCreateAdaptationSet(
    const MediaInfo& media_info,
    bool content_protection_in_adaptation_set) {
  // AdaptationSets with the same key should only differ in ContentProtection,
  // which also means that if |content_protection_in_adaptation_set| is false,
  // there should be at most one entry in |adaptation_sets|.
  const std::string key = GetAdaptationSetKey(media_info);
  std::list<AdaptationSet*>& adaptation_sets = adaptation_set_list_map_[key];
  if (content_protection_in_adaptation_set) {
    for (AdaptationSet* adaptation_set : adaptation_sets) {
      if (protected_adaptation_set_map_.Match(*adaptation_set, media_info))
        return adaptation_set;
    }
  } else {
    if (!adaptation_sets.empty()) {
      DCHECK_EQ(adaptation_sets.size(), 1u);
      return adaptation_sets.front();
    }
  }
  // None of the adaptation sets match with the new content protection.
  // Need a new one.
  std::string language = GetLanguage(media_info);
  std::unique_ptr<AdaptationSet> new_adaptation_set =
      NewAdaptationSet(adaptation_set_counter_->GetNext(), language,
                       mpd_options_, representation_counter_);
  if (!SetNewAdaptationSetAttributes(language, media_info, adaptation_sets,
                                     new_adaptation_set.get())) {
    return nullptr;
  }

  if (content_protection_in_adaptation_set &&
      media_info.has_protected_content()) {
    protected_adaptation_set_map_.Register(*new_adaptation_set, media_info);
    AddContentProtectionElements(media_info, new_adaptation_set.get());

    for (AdaptationSet* adaptation_set : adaptation_sets) {
      if (protected_adaptation_set_map_.Switchable(*adaptation_set,
                                                   *new_adaptation_set)) {
        adaptation_set->AddAdaptationSetSwitching(new_adaptation_set->id());
        new_adaptation_set->AddAdaptationSetSwitching(adaptation_set->id());
      }
    }
  }
  AdaptationSet* adaptation_set_ptr = new_adaptation_set.get();
  adaptation_sets.push_back(adaptation_set_ptr);
  adaptation_set_map_[adaptation_set_ptr->id()] = std::move(new_adaptation_set);
  return adaptation_set_ptr;
}

xml::scoped_xml_ptr<xmlNode> Period::GetXml() {
  xml::XmlNode period("Period");

  // Required for 'dynamic' MPDs.
  period.SetId(id_);
  // Iterate thru AdaptationSets and add them to one big Period element.
  for (const auto& adaptation_set_pair : adaptation_set_map_) {
    xml::scoped_xml_ptr<xmlNode> child(adaptation_set_pair.second->GetXml());
    if (!child || !period.AddChild(std::move(child)))
      return nullptr;
  }

  if (mpd_options_.mpd_type == MpdType::kDynamic ||
      start_time_in_seconds_ != 0) {
    period.SetStringAttribute("start",
                              SecondsToXmlDuration(start_time_in_seconds_));
  }
  return period.PassScopedPtr();
}

const std::list<AdaptationSet*> Period::GetAdaptationSets() const {
  std::list<AdaptationSet*> adaptation_sets;
  for (const auto& adaptation_set_pair : adaptation_set_map_) {
    adaptation_sets.push_back(adaptation_set_pair.second.get());
  }
  return adaptation_sets;
}

std::unique_ptr<AdaptationSet> Period::NewAdaptationSet(
    uint32_t adaptation_set_id,
    const std::string& language,
    const MpdOptions& options,
    base::AtomicSequenceNumber* representation_counter) {
  return std::unique_ptr<AdaptationSet>(new AdaptationSet(
      adaptation_set_id, language, options, representation_counter));
}

bool Period::SetNewAdaptationSetAttributes(
    const std::string& language,
    const MediaInfo& media_info,
    const std::list<AdaptationSet*>& adaptation_sets,
    AdaptationSet* new_adaptation_set) {
  if (!language.empty() && language == mpd_options_.mpd_params.default_language)
    new_adaptation_set->AddRole(AdaptationSet::kRoleMain);

  if (media_info.has_video_info()) {
    // Because 'language' is ignored for videos, |adaptation_sets| must have
    // all the video AdaptationSets.
    if (adaptation_sets.size() > 1) {
      new_adaptation_set->AddRole(AdaptationSet::kRoleMain);
    } else if (adaptation_sets.size() == 1) {
      (*adaptation_sets.begin())->AddRole(AdaptationSet::kRoleMain);
      new_adaptation_set->AddRole(AdaptationSet::kRoleMain);
    }

    if (media_info.video_info().has_playback_rate()) {
      uint32_t trick_play_reference_id = 0;
      if (!FindOriginalAdaptationSetForTrickPlay(media_info,
                                                 &trick_play_reference_id)) {
        LOG(ERROR) << "Failed to find main adaptation set for trick play.";
        return false;
      }
      DCHECK_NE(new_adaptation_set->id(), trick_play_reference_id);
      new_adaptation_set->AddTrickPlayReferenceId(trick_play_reference_id);
    }
  } else if (media_info.has_text_info()) {
    // IOP requires all AdaptationSets to have (sub)segmentAlignment set to
    // true, so carelessly set it to true.
    // In practice it doesn't really make sense to adapt between text tracks.
    new_adaptation_set->ForceSetSegmentAlignment(true);
  }
  return true;
}

bool Period::FindOriginalAdaptationSetForTrickPlay(
    const MediaInfo& media_info,
    uint32_t* main_adaptation_set_id) {
  MediaInfo media_info_no_trickplay = media_info;
  media_info_no_trickplay.mutable_video_info()->clear_playback_rate();

  std::string key = GetAdaptationSetKey(media_info_no_trickplay);
  const std::list<AdaptationSet*>& adaptation_sets =
      adaptation_set_list_map_[key];
  for (AdaptationSet* adaptation_set : adaptation_sets) {
    if (protected_adaptation_set_map_.Match(*adaptation_set, media_info)) {
      *main_adaptation_set_id = adaptation_set->id();
      return true;
    }
  }
  return false;
}

void Period::ProtectedAdaptationSetMap::Register(
    const AdaptationSet& adaptation_set,
    const MediaInfo& media_info) {
  DCHECK(!ContainsKey(protected_content_map_, adaptation_set.id()));
  protected_content_map_[adaptation_set.id()] = media_info.protected_content();
}

bool Period::ProtectedAdaptationSetMap::Match(
    const AdaptationSet& adaptation_set,
    const MediaInfo& media_info) {
  const auto protected_content_it =
      protected_content_map_.find(adaptation_set.id());
  // If the AdaptationSet ID is not registered in the map, then it is clear
  // content.
  if (protected_content_it == protected_content_map_.end())
    return !media_info.has_protected_content();
  if (!media_info.has_protected_content())
    return false;
  return ProtectedContentEq(protected_content_it->second,
                            media_info.protected_content());
}

bool Period::ProtectedAdaptationSetMap::Switchable(
    const AdaptationSet& adaptation_set_a,
    const AdaptationSet& adaptation_set_b) {
  const auto protected_content_it_a =
      protected_content_map_.find(adaptation_set_a.id());
  const auto protected_content_it_b =
      protected_content_map_.find(adaptation_set_b.id());

  if (protected_content_it_a == protected_content_map_.end())
    return protected_content_it_b == protected_content_map_.end();
  if (protected_content_it_b == protected_content_map_.end())
    return false;
  // Get all the UUIDs of the AdaptationSet. If another AdaptationSet has the
  // same UUIDs then those are switchable.
  return GetUUIDs(protected_content_it_a->second) ==
         GetUUIDs(protected_content_it_b->second);
}

}  // namespace shaka
