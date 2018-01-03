// Copyright 2015 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/mpd/base/simple_mpd_notifier.h"

#include "packager/base/logging.h"
#include "packager/base/stl_util.h"
#include "packager/mpd/base/adaptation_set.h"
#include "packager/mpd/base/mpd_builder.h"
#include "packager/mpd/base/mpd_notifier_util.h"
#include "packager/mpd/base/mpd_utils.h"
#include "packager/mpd/base/period.h"
#include "packager/mpd/base/representation.h"

namespace shaka {

SimpleMpdNotifier::SimpleMpdNotifier(const MpdOptions& mpd_options)
    : MpdNotifier(mpd_options),
      output_path_(mpd_options.mpd_params.mpd_output),
      mpd_builder_(new MpdBuilder(mpd_options)),
      content_protection_in_adaptation_set_(
          mpd_options.mpd_params.generate_dash_if_iop_compliant_mpd) {
  for (const std::string& base_url : mpd_options.mpd_params.base_urls)
    mpd_builder_->AddBaseUrl(base_url);
}

SimpleMpdNotifier::~SimpleMpdNotifier() {}

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
  if (!period_)
    period_ = mpd_builder_->AddPeriod();
  AdaptationSet* adaptation_set = period_->GetOrCreateAdaptationSet(
      media_info, content_protection_in_adaptation_set_);
  DCHECK(adaptation_set);

  MediaInfo adjusted_media_info(media_info);
  MpdBuilder::MakePathsRelativeToMpd(output_path_, &adjusted_media_info);
  Representation* representation =
      adaptation_set->AddRepresentation(adjusted_media_info);
  if (!representation)
    return false;

  if (content_protection_in_adaptation_set_) {
    // ContentProtection elements are already added to AdaptationSet above.
    // Use RepresentationId to AdaptationSet map to update ContentProtection
    // in AdaptationSet in NotifyEncryptionUpdate.
    representation_id_to_adaptation_set_[representation->id()] = adaptation_set;
  } else {
    AddContentProtectionElements(media_info, representation);
  }

  *container_id = representation->id();
  DCHECK(!ContainsKey(representation_map_, representation->id()));
  representation_map_[representation->id()] = representation;
  return true;
}

bool SimpleMpdNotifier::NotifySampleDuration(uint32_t container_id,
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

bool SimpleMpdNotifier::NotifyNewSegment(uint32_t container_id,
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

bool SimpleMpdNotifier::NotifyCueEvent(uint32_t container_id,
                                       uint64_t timestamp) {
  NOTIMPLEMENTED();
  return false;
}

bool SimpleMpdNotifier::NotifyEncryptionUpdate(
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

  if (content_protection_in_adaptation_set_) {
    AdaptationSet* adaptation_set_for_representation =
        representation_id_to_adaptation_set_[it->second->id()];
    adaptation_set_for_representation->UpdateContentProtectionPssh(
        drm_uuid, Uint8VectorToBase64(new_pssh));
  } else {
    it->second->UpdateContentProtectionPssh(drm_uuid,
                                            Uint8VectorToBase64(new_pssh));
  }
  return true;
}

bool SimpleMpdNotifier::Flush() {
  base::AutoLock auto_lock(lock_);
  return WriteMpdToFile(output_path_, mpd_builder_.get());
}

}  // namespace shaka
