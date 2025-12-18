// Copyright 2017 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/mpd/base/period.h>

#include <absl/log/check.h>
#include <absl/log/log.h>

#include <packager/mpd/base/adaptation_set.h>
#include <packager/mpd/base/mpd_options.h>
#include <packager/mpd/base/mpd_utils.h>
#include <packager/mpd/base/xml/xml_node.h>

namespace shaka {
namespace {

const std::string& GetDefaultAudioLanguage(const MpdOptions& mpd_options) {
  return mpd_options.mpd_params.default_language;
}

const std::string& GetDefaultTextLanguage(const MpdOptions& mpd_options) {
  return mpd_options.mpd_params.default_text_language.empty()
             ? mpd_options.mpd_params.default_language
             : mpd_options.mpd_params.default_text_language;
}

AdaptationSet::Role RoleFromString(const std::string& role_str) {
  if (role_str == "caption")
    return AdaptationSet::Role::kRoleCaption;
  if (role_str == "subtitle")
    return AdaptationSet::Role::kRoleSubtitle;
  if (role_str == "main")
    return AdaptationSet::Role::kRoleMain;
  if (role_str == "alternate")
    return AdaptationSet::Role::kRoleAlternate;
  if (role_str == "supplementary")
    return AdaptationSet::Role::kRoleSupplementary;
  if (role_str == "commentary")
    return AdaptationSet::Role::kRoleCommentary;
  if (role_str == "dub")
    return AdaptationSet::Role::kRoleDub;
  if (role_str == "forced-subtitle")
    return AdaptationSet::Role::kRoleForcedSubtitle;
  if (role_str == "karaoke")
    return AdaptationSet::Role::kRoleKaraoke;
  if (role_str == "sign")
    return AdaptationSet::Role::kRoleSign;
  if (role_str == "metadata")
    return AdaptationSet::Role::kRoleMetadata;
  if (role_str == "enhanced-audio-intelligibility")
    return AdaptationSet::Role::kRoleEnhancedAudioIntelligibility;
  if (role_str == "emergency")
    return AdaptationSet::Role::kRoleEmergency;
  if (role_str == "easyreader")
    return AdaptationSet::Role::kRoleEasyreader;
  if (role_str == "description")
    return AdaptationSet::Role::kRoleDescription;
  return AdaptationSet::Role::kRoleUnknown;
}

}  // namespace

Period::Period(uint32_t period_id,
               double start_time_in_seconds,
               const MpdOptions& mpd_options,
               uint32_t* representation_counter)
    : id_(period_id),
      start_time_in_seconds_(start_time_in_seconds),
      mpd_options_(mpd_options),
      representation_counter_(representation_counter) {}

AdaptationSet* Period::GetOrCreateAdaptationSet(
    const MediaInfo& media_info,
    bool content_protection_in_adaptation_set) {
  // Set duration if it is not set. It may be updated later from duration
  // calculated from segments.
  if (duration_seconds_ == 0)
    duration_seconds_ = media_info.media_duration_seconds();

  const std::string key = GetAdaptationSetKey(
      media_info, mpd_options_.mpd_params.allow_codec_switching);

  std::list<AdaptationSet*>& adaptation_sets = adaptation_set_list_map_[key];

  for (AdaptationSet* adaptation_set : adaptation_sets) {
    if (adaptation_set->MatchAdaptationSet(
            media_info, content_protection_in_adaptation_set))
      return adaptation_set;
  }

  // None of the adaptation sets match with the new content protection.
  // Need a new one.
  const std::string language = GetLanguage(media_info);
  std::unique_ptr<AdaptationSet> new_adaptation_set =
      NewAdaptationSet(language, mpd_options_, representation_counter_);
  if (!SetNewAdaptationSetAttributes(language, media_info, adaptation_sets,
                                     content_protection_in_adaptation_set,
                                     new_adaptation_set.get())) {
    return nullptr;
  }

  for (AdaptationSet* adaptation_set : adaptation_sets) {
    if (adaptation_set->SwitchableAdaptationSet(*new_adaptation_set)) {
      adaptation_set->AddAdaptationSetSwitching(new_adaptation_set.get());
      new_adaptation_set->AddAdaptationSetSwitching(adaptation_set);
    }
  }

  AdaptationSet* adaptation_set_ptr = new_adaptation_set.get();
  adaptation_sets.push_back(adaptation_set_ptr);
  adaptation_sets_.emplace_back(std::move(new_adaptation_set));
  return adaptation_set_ptr;
}

std::optional<xml::XmlNode> Period::GetXml(bool output_period_duration) {
  adaptation_sets_.sort(
      [](const std::unique_ptr<AdaptationSet>& adaptation_set_a,
         const std::unique_ptr<AdaptationSet>& adaptation_set_b) {
        auto index_a = adaptation_set_a->SortIndex();
        auto index_b = adaptation_set_b->SortIndex();

        if (!index_a)
          return false;
        if (!index_b)
          return true;
        return index_a < index_b;
      });

  xml::XmlNode period("Period");

  // Required for 'dynamic' MPDs.
  if (!period.SetId(id_))
    return std::nullopt;

  // Required for LL-DASH MPDs.
  if (mpd_options_.mpd_params.low_latency_dash_mode) {
    // Create ServiceDescription element.
    xml::XmlNode service_description_node("ServiceDescription");
    if (!service_description_node.SetIntegerAttribute("id", id_))
      return std::nullopt;

    // Insert Latency into ServiceDescription element.
    xml::XmlNode latency_node("Latency");
    uint64_t target_latency_ms =
        mpd_options_.mpd_params.target_latency_seconds * 1000;
    if (!latency_node.SetIntegerAttribute("target", target_latency_ms))
      return std::nullopt;
    if (!service_description_node.AddChild(std::move(latency_node)))
      return std::nullopt;

    // Insert ServiceDescription into Period element.
    if (!period.AddChild(std::move(service_description_node)))
      return std::nullopt;
  }

  // Iterate thru AdaptationSets and add them to one big Period element.
  // Also force AdaptationSets Id to incremental order, which might not
  // be the case if force_cl_index is used.
  int idx = 0;
  for (auto& adaptation_set : adaptation_sets_) {
    adaptation_set->set_id(idx++);
  }

  for (const auto& adaptation_set : adaptation_sets_) {
    auto child = adaptation_set->GetXml();
    if (!child || !period.AddChild(std::move(*child)))
      return std::nullopt;
  }

  if (output_period_duration) {
    if (!period.SetStringAttribute("duration",
                                   SecondsToXmlDuration(duration_seconds_))) {
      return std::nullopt;
    }
  } else if (mpd_options_.mpd_type == MpdType::kDynamic) {
    if (!period.SetStringAttribute(
            "start", SecondsToXmlDuration(start_time_in_seconds_))) {
      return std::nullopt;
    }
  }
  return period;
}

const std::list<AdaptationSet*> Period::GetAdaptationSets() const {
  std::list<AdaptationSet*> adaptation_sets;
  for (const auto& adaptation_set : adaptation_sets_) {
    adaptation_sets.push_back(adaptation_set.get());
  }
  return adaptation_sets;
}

std::unique_ptr<AdaptationSet> Period::NewAdaptationSet(
    const std::string& language,
    const MpdOptions& options,
    uint32_t* representation_counter) {
  return std::unique_ptr<AdaptationSet>(
      new AdaptationSet(language, options, representation_counter));
}

bool Period::SetNewAdaptationSetAttributes(
    const std::string& language,
    const MediaInfo& media_info,
    const std::list<AdaptationSet*>& adaptation_sets,
    bool content_protection_in_adaptation_set,
    AdaptationSet* new_adaptation_set) {
  if (!media_info.dash_roles().empty()) {
    for (const std::string& role_str : media_info.dash_roles()) {
      AdaptationSet::Role role = RoleFromString(role_str);
      if (role == AdaptationSet::kRoleUnknown) {
        LOG(ERROR) << "Unrecognized role '" << role_str << "'.";
        return false;
      }
      new_adaptation_set->AddRole(role);
    }
  } else if (!language.empty()) {
    const bool is_main_role =
        language == (media_info.has_audio_info()
                         ? GetDefaultAudioLanguage(mpd_options_)
                         : GetDefaultTextLanguage(mpd_options_));
    if (is_main_role)
      new_adaptation_set->AddRole(AdaptationSet::kRoleMain);
  }
  for (const std::string& accessibility : media_info.dash_accessibilities()) {
    size_t pos = accessibility.find('=');
    if (pos == std::string::npos) {
      LOG(ERROR)
          << "Accessibility should be in scheme=value format, but seeing "
          << accessibility;
      return false;
    }
    new_adaptation_set->AddAccessibility(accessibility.substr(0, pos),
                                         accessibility.substr(pos + 1));
  }

  const std::string& codec = GetBaseCodec(media_info);
  new_adaptation_set->set_codec(codec);

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
      std::string trick_play_reference_adaptation_set_key;
      AdaptationSet* trick_play_reference_adaptation_set =
          FindMatchingAdaptationSetForTrickPlay(
              media_info, content_protection_in_adaptation_set,
              &trick_play_reference_adaptation_set_key);
      if (trick_play_reference_adaptation_set) {
        new_adaptation_set->AddTrickPlayReference(
            trick_play_reference_adaptation_set);
      } else {
        trickplay_cache_[trick_play_reference_adaptation_set_key].push_back(
            new_adaptation_set);
      }
    } else {
      std::string trick_play_adaptation_set_key;
      AdaptationSet* trickplay_adaptation_set =
          FindMatchingAdaptationSetForTrickPlay(
              media_info, content_protection_in_adaptation_set,
              &trick_play_adaptation_set_key);
      if (trickplay_adaptation_set) {
        trickplay_adaptation_set->AddTrickPlayReference(new_adaptation_set);
        trickplay_cache_.erase(trick_play_adaptation_set_key);
      }
    }

    // Set transfer characteristics.
    // https://dashif.org/docs/DASH-IF-IOP-v4.3.pdf - 4.2.5.1
    // ISO/IEC 23001-8 MPEG systems technologies — Part 8: Coding-independent
    // code points. https://en.wikipedia.org/wiki/Coding-independent_code_points
    // - Common CCIP values.
    // Dolby vision:
    // https://professionalsupport.dolby.com/s/article/How-to-signal-Dolby-Vision-in-MPEG-DASH
    // Transfer characteristics for Dolby Vision (dvh1 or dvhe) must be PQ
    // irrespective of value present in SPS VUI.
    if (new_adaptation_set->codec().find("dvh") == 0) {
      new_adaptation_set->set_transfer_characteristics(kTransferFunctionPQ);
    } else if (media_info.video_info().has_transfer_characteristics()) {
      new_adaptation_set->set_transfer_characteristics(
          media_info.video_info().transfer_characteristics());
    }

    new_adaptation_set->set_matrix_coefficients(
        media_info.video_info().matrix_coefficients());
    new_adaptation_set->set_color_primaries(
        media_info.video_info().color_primaries());
  } else if (media_info.has_audio_info()) {
    if (codec == "mp4a" || codec == "ac-3" || codec == "ec-3" ||
        codec == "ac-4") {
      if (mpd_options_.dash_profile == DashProfile::kLive) {
        new_adaptation_set->ForceStartwithSAP(1);
      } else if (mpd_options_.dash_profile == DashProfile::kOnDemand) {
        new_adaptation_set->ForceSubsegmentStartswithSAP(1);
      }
    }
  } else if (media_info.has_text_info()) {
    // IOP requires all AdaptationSets to have (sub)segmentAlignment set to
    // true, so carelessly set it to true.
    // In practice it doesn't really make sense to adapt between text tracks.
    new_adaptation_set->ForceSetSegmentAlignment(true);
  }

  if (content_protection_in_adaptation_set &&
      media_info.has_protected_content()) {
    new_adaptation_set->set_protected_content(media_info);
    AddContentProtectionElements(media_info, new_adaptation_set);
  }

  return true;
}

AdaptationSet* Period::FindMatchingAdaptationSetForTrickPlay(
    const MediaInfo& media_info,
    bool content_protection_in_adaptation_set,
    std::string* adaptation_set_key) {
  std::list<AdaptationSet*>* adaptation_sets = nullptr;
  const bool is_trickplay_adaptation_set =
      media_info.video_info().has_playback_rate();
  if (is_trickplay_adaptation_set) {
    *adaptation_set_key = GetAdaptationSetKeyForTrickPlay(media_info);
    if (adaptation_set_list_map_.find(*adaptation_set_key) ==
        adaptation_set_list_map_.end())
      return nullptr;
    adaptation_sets = &adaptation_set_list_map_[*adaptation_set_key];
  } else {
    *adaptation_set_key = GetAdaptationSetKey(
        media_info, mpd_options_.mpd_params.allow_codec_switching);
    if (trickplay_cache_.find(*adaptation_set_key) == trickplay_cache_.end())
      return nullptr;
    adaptation_sets = &trickplay_cache_[*adaptation_set_key];
  }
  for (AdaptationSet* adaptation_set : *adaptation_sets) {
    if (adaptation_set->MatchAdaptationSet(
            media_info, content_protection_in_adaptation_set))
      return adaptation_set;
  }

  return nullptr;
}

std::string Period::GetAdaptationSetKeyForTrickPlay(
    const MediaInfo& media_info) {
  MediaInfo media_info_no_trickplay = media_info;
  media_info_no_trickplay.mutable_video_info()->clear_playback_rate();
  return GetAdaptationSetKey(media_info_no_trickplay,
                             mpd_options_.mpd_params.allow_codec_switching);
}

Period::~Period() {
  if (!trickplay_cache_.empty()) {
    LOG(WARNING) << "Trickplay adaptation set did not get a valid adaptation "
                    "set match. Please check the command line options.";
  }
}

}  // namespace shaka
