// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/mpd/base/simple_mpd_notifier.h"

#include "packager/base/logging.h"
#include "packager/media/file/file.h"
#include "packager/mpd/base/mpd_builder.h"
#include "packager/mpd/base/mpd_utils.h"

using edash_packager::media::File;

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
  if (content_type == kUnknown)
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

  *container_id = representation->id();

  if (mpd_builder_->type() == MpdBuilder::kStatic)
    return WriteMpdToFile();

  DCHECK(!ContainsKey(representation_map_, representation->id()));
  representation_map_[representation->id()] = representation;
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
  it->second->SetSampleDuration(sample_duration);
  return true;
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
  it->second->AddNewSegment(start_time, duration, size);
  return WriteMpdToFile();
}

bool SimpleMpdNotifier::AddContentProtectionElement(
    uint32_t container_id,
    const ContentProtectionElement& content_protection_element) {
  NOTIMPLEMENTED();
  return false;
}

SimpleMpdNotifier::ContentType SimpleMpdNotifier::GetContentType(
    const MediaInfo& media_info) {
  const bool has_video = media_info.has_video_info();
  const bool has_audio = media_info.has_audio_info();
  const bool has_text = media_info.has_text_info();

  if (MoreThanOneTrue(has_video, has_audio, has_text)) {
    NOTIMPLEMENTED() << "MediaInfo with more than one stream is not supported.";
    return kUnknown;
  }
  if (!AtLeastOneTrue(has_video, has_audio, has_text)) {
    LOG(ERROR) << "MediaInfo should contain one audio, video, or text stream.";
    return kUnknown;
  }
  return has_video ? kVideo : (has_audio ? kAudio : kText);
}

bool SimpleMpdNotifier::WriteMpdToFile() {
  CHECK(!output_path_.empty());

  std::string mpd;
  if (!mpd_builder_->ToString(&mpd)) {
    LOG(ERROR) << "Failed to write MPD to string.";
    return false;
  }

  File* file = File::Open(output_path_.c_str(), "w");
  if (!file) {
    LOG(ERROR) << "Failed to open file for writing: " << output_path_;
    return false;
  }

  const char* mpd_char_ptr = mpd.data();
  size_t mpd_bytes_left = mpd.size();
  while (mpd_bytes_left > 0) {
    int64_t length = file->Write(mpd_char_ptr, mpd_bytes_left);
    if (length <= 0) {
      LOG(ERROR) << "Failed to write to file '" << output_path_ << "' ("
                 << length << ").";
      return false;
    }
    mpd_char_ptr += length;
    mpd_bytes_left -= length;
  }
  return file->Close();
}

}  // namespace edash_packager
