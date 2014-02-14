// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "mpd/base/simple_vod_mpd_notifier.h"

#include "base/stl_util.h"
#include "mpd/base/content_protection_element.h"
#include "mpd/base/media_info.pb.h"

namespace dash_packager {

SimpleVodMpdNotifier::SimpleVodMpdNotifier(MpdBuilder* mpd_builder)
    : mpd_builder_(mpd_builder),
      video_adaptation_set_(NULL),
      audio_adaptation_set_(NULL),
      representation_(NULL) {
  DCHECK(mpd_builder);
}

SimpleVodMpdNotifier::~SimpleVodMpdNotifier() {}

bool SimpleVodMpdNotifier::Init() {
  return true;
}

bool SimpleVodMpdNotifier::NotifyNewContainer(const MediaInfo& media_info,
                                              uint32* container_id) {
  DCHECK(container_id);

  if (media_info.video_info_size() > 0 && media_info.audio_info_size() > 0) {
    LOG(ERROR) << "SimpleVodMpdNotifier cannot handle media container with "
                  "both video and audio";
    return false;
  }

  ContainerType container_type = kVideo;
  if (media_info.video_info_size() > 0) {
    container_type = kVideo;
  } else if (media_info.audio_info_size() > 0) {
    container_type = kAudio;
  } else {
    LOG(ERROR) << "Either video_info or audio_info must be populated.";
    return false;
  }

  if (!AddNewRepresentation(container_type, media_info, container_id))
    return false;

  return mpd_builder_->WriteMpd();
}

bool SimpleVodMpdNotifier::NotifyNewSegment(uint32 container_id,
                                            uint64 start_time,
                                            uint64 duration) {
  DLOG(INFO) << "VOD does not support this operation.";
  return false;
}

bool SimpleVodMpdNotifier::AddContentProtectionElement(
    uint32 container_id,
    const ContentProtectionElement& content_protection_element) {
  if (!ContainsKey(id_to_representation_, container_id))
    return false;

  Representation* representation = id_to_representation_[container_id];

  DCHECK(representation);
  representation->AddContentProtectionElement(content_protection_element);
  return mpd_builder_->WriteMpd();
}

bool SimpleVodMpdNotifier::AddNewRepresentation(ContainerType type,
                                                const MediaInfo& media_info,
                                                uint32* container_id) {
  // Use pointer-pointer to set {video,audio}_adaptation_set_.
  AdaptationSet** adaptation_set_pp = NULL;
  if (type == kVideo) {
    adaptation_set_pp = &video_adaptation_set_;
  } else if (type == kAudio){
    adaptation_set_pp = &audio_adaptation_set_;
  } else {
    NOTREACHED() << "Unknown container type: " << type;
    return false;
  }

  if (!*adaptation_set_pp)
    *adaptation_set_pp = mpd_builder_->AddAdaptationSet();

  AdaptationSet* const adaptation_set = *adaptation_set_pp;
  Representation* new_representation =
      adaptation_set->AddRepresentation(media_info);

  if (!new_representation)
    return false;

  const uint32 representation_id = new_representation->id();
  id_to_representation_[representation_id] = new_representation;
  *container_id = representation_id;

  return true;
}

}  // namespace dash_packager
