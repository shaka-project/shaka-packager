// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/mpd/base/simple_mpd_notifier.h"

#include "packager/base/logging.h"
#include "packager/mpd/base/mpd_builder.h"
#include "packager/mpd/base/mpd_notifier_util.h"
#include "packager/mpd/base/mpd_utils.h"

namespace edash_packager {

SimpleMpdNotifier::SimpleMpdNotifier(DashProfile dash_profile,
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

SimpleMpdNotifier::~SimpleMpdNotifier() {
}

bool SimpleMpdNotifier::Init() {
  return true;
}

bool SimpleMpdNotifier::NotifyNewContainer(const MediaInfo& media_info,
                                           uint32_t* container_id) {
  DCHECK(container_id);

  ContentType content_type = GetContentType(media_info);
  if (content_type == kContentTypeUnknown)
    return false;

  base::AutoLock auto_lock(lock_);
  // TODO(kqyang): Consider adding a new method MpdBuilder::AddRepresentation.
  // Most of the codes here can be moved inside.
  std::string lang;
  if (media_info.has_audio_info()) {
    lang = media_info.audio_info().language();
  }
  AdaptationSet** adaptation_set = &adaptation_set_map_[content_type][lang];
  if (*adaptation_set == NULL)
    *adaptation_set = mpd_builder_->AddAdaptationSet(lang);

  DCHECK(*adaptation_set);
  MediaInfo adjusted_media_info(media_info);
  MpdBuilder::MakePathsRelativeToMpd(output_path_, &adjusted_media_info);
  Representation* representation =
      (*adaptation_set)->AddRepresentation(adjusted_media_info);
  if (representation == NULL)
    return false;

  // For SimpleMpdNotifier, just put it in Representation. It should still
  // generate a valid MPD.
  AddContentProtectionElements(media_info, representation);
  *container_id = representation->id();
  DCHECK(!ContainsKey(representation_map_, representation->id()));
  representation_map_[representation->id()] = representation;

  if (mpd_builder_->type() == MpdBuilder::kStatic)
    return WriteMpdToFile(output_path_, mpd_builder_.get());
  return true;
}

bool SimpleMpdNotifier::NotifySampleDuration(uint32_t container_id,
                                             uint32_t sample_duration) {
  base::AutoLock auto_lock(lock_);
  RepresentationMap::iterator it = representation_map_.find(container_id);
  if (it == representation_map_.end()) {
    LOG(ERROR) << "Unexpected container_id: " << container_id;
    return false;
  }
  // This sets the right frameRate for Representation or AdaptationSet, so
  // write out the new MPD.
  it->second->SetSampleDuration(sample_duration);
  return WriteMpdToFile(output_path_, mpd_builder_.get());
}

bool SimpleMpdNotifier::NotifyNewSegment(uint32_t container_id,
                                         uint64_t start_time,
                                         uint64_t duration,
                                         uint64_t size) {
  base::AutoLock auto_lock(lock_);
  RepresentationMap::iterator it = representation_map_.find(container_id);
  if (it == representation_map_.end()) {
    LOG(ERROR) << "Unexpected container_id: " << container_id;
    return false;
  }
  // For live, the timeline and segmentAlignment gets updated. For VOD,
  // subsegmentAlignment gets updated. So write out the MPD.
  it->second->AddNewSegment(start_time, duration, size);
  return WriteMpdToFile(output_path_, mpd_builder_.get());
}

bool SimpleMpdNotifier::AddContentProtectionElement(
    uint32_t container_id,
    const ContentProtectionElement& content_protection_element) {
  base::AutoLock auto_lock(lock_);
  RepresentationMap::iterator it = representation_map_.find(container_id);
  if (it == representation_map_.end()) {
    LOG(ERROR) << "Unexpected container_id: " << container_id;
    return false;
  }
  it->second->AddContentProtectionElement(content_protection_element);
  return WriteMpdToFile(output_path_, mpd_builder_.get());
}

}  // namespace edash_packager
