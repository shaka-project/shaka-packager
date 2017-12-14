// Copyright 2015 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/mpd/base/dash_iop_mpd_notifier.h"

#include "packager/base/stl_util.h"
#include "packager/mpd/base/adaptation_set.h"
#include "packager/mpd/base/media_info.pb.h"
#include "packager/mpd/base/mpd_notifier_util.h"
#include "packager/mpd/base/period.h"
#include "packager/mpd/base/representation.h"

namespace {
const bool kContentProtectionInAdaptationSet = true;
}  // namespace

namespace shaka {

DashIopMpdNotifier::DashIopMpdNotifier(const MpdOptions& mpd_options)
    : MpdNotifier(mpd_options),
      output_path_(mpd_options.mpd_params.mpd_output),
      mpd_builder_(new MpdBuilder(mpd_options)) {
  for (const std::string& base_url : mpd_options.mpd_params.base_urls)
    mpd_builder_->AddBaseUrl(base_url);
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
  if (!period_)
    period_ = mpd_builder_->AddPeriod();
  AdaptationSet* adaptation_set = period_->GetOrCreateAdaptationSet(
      media_info, kContentProtectionInAdaptationSet);
  DCHECK(adaptation_set);

  MediaInfo adjusted_media_info(media_info);
  MpdBuilder::MakePathsRelativeToMpd(output_path_, &adjusted_media_info);
  Representation* representation =
      adaptation_set->AddRepresentation(adjusted_media_info);
  if (!representation)
    return false;

  representation_id_to_adaptation_set_[representation->id()] = adaptation_set;

  *container_id = representation->id();
  DCHECK(!ContainsKey(representation_map_, representation->id()));
  representation_map_[representation->id()] = representation;
  return true;
}

bool DashIopMpdNotifier::NotifySampleDuration(uint32_t container_id,
                                              uint32_t sample_duration) {
  base::AutoLock auto_lock(lock_);
  auto it = representation_map_.find(container_id);
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
  auto it = representation_map_.find(container_id);
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
  auto it = representation_map_.find(container_id);
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

}  // namespace shaka
