// Copyright 2015 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/mpd/base/simple_mpd_notifier.h"

#include <gflags/gflags.h>

#include "packager/base/logging.h"
#include "packager/base/stl_util.h"
#include "packager/mpd/base/adaptation_set.h"
#include "packager/mpd/base/mpd_builder.h"
#include "packager/mpd/base/mpd_notifier_util.h"
#include "packager/mpd/base/mpd_utils.h"
#include "packager/mpd/base/period.h"
#include "packager/mpd/base/representation.h"

DEFINE_int32(
    pto_adjustment,
    -1,
    "There could be rounding errors in MSE which could cut the first key frame "
    "of the representation and thus cut all the frames until the next key "
    "frame, which then leads to a big gap in presentation timeline which "
    "stalls playback. A small back off may be necessary to compensate for the "
    "possible rounding error. It should not cause any playback issues if it is "
    "small enough. The workaround can be removed once the problem is handled "
    "in all players.");

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

  MediaInfo adjusted_media_info(media_info);
  MpdBuilder::MakePathsRelativeToMpd(output_path_, &adjusted_media_info);
  const Representation* kNoOriginalRepresentation = nullptr;
  const double kPeriodStartTimeSeconds = 0.0;

  base::AutoLock auto_lock(lock_);
  const Representation* representation = AddRepresentationToPeriod(
      adjusted_media_info, kNoOriginalRepresentation, kPeriodStartTimeSeconds);
  if (!representation)
    return false;
  *container_id = representation->id();
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
  base::AutoLock auto_lock(lock_);
  auto it = representation_map_.find(container_id);
  if (it == representation_map_.end()) {
    LOG(ERROR) << "Unexpected container_id: " << container_id;
    return false;
  }
  Representation* original_representation = it->second;
  AdaptationSet* original_adaptation_set =
      representation_id_to_adaptation_set_[container_id];

  const MediaInfo& media_info = original_representation->GetMediaInfo();
  const double period_start_time_seconds =
      static_cast<double>(timestamp) / media_info.reference_time_scale();
  const Representation* new_representation = AddRepresentationToPeriod(
      media_info, original_representation, period_start_time_seconds);
  if (!new_representation)
    return false;

  // TODO(kqyang): Pass the ID to GetOrCreateAdaptationSet instead?
  AdaptationSet* new_adaptation_set =
      representation_id_to_adaptation_set_[container_id];
  DCHECK(new_adaptation_set);
  new_adaptation_set->set_id(original_adaptation_set->id());
  return true;
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

Representation* SimpleMpdNotifier::AddRepresentationToPeriod(
    const MediaInfo& media_info,
    const Representation* original_representation,
    double period_start_time_seconds) {
  Period* period = mpd_builder_->GetOrCreatePeriod(period_start_time_seconds);
  DCHECK(period);

  AdaptationSet* adaptation_set = period->GetOrCreateAdaptationSet(
      media_info, content_protection_in_adaptation_set_);
  DCHECK(adaptation_set);

  Representation* representation = nullptr;
  if (original_representation) {
    uint64_t presentation_time_offset =
        period->start_time_in_seconds() * media_info.reference_time_scale();
    if (presentation_time_offset > 0) {
      presentation_time_offset += FLAGS_pto_adjustment;
    }
    representation = adaptation_set->CopyRepresentationWithTimeOffset(
        *original_representation, presentation_time_offset);
  } else {
    representation = adaptation_set->AddRepresentation(media_info);
  }
  if (!representation)
    return nullptr;

  if (content_protection_in_adaptation_set_) {
    // ContentProtection elements are already added to AdaptationSet above.
    // Use RepresentationId to AdaptationSet map to update ContentProtection
    // in AdaptationSet in NotifyEncryptionUpdate.
    representation_id_to_adaptation_set_[representation->id()] = adaptation_set;
  } else {
    AddContentProtectionElements(media_info, representation);
  }
  representation_map_[representation->id()] = representation;
  return representation;
}

}  // namespace shaka
