// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "mpd/base/simple_mpd_notifier.h"

#include "base/logging.h"
#include "media/file/file.h"
#include "mpd/base/mpd_builder.h"
#include "mpd/base/mpd_utils.h"

using media::File;

namespace {
bool MoreThanOneTrue(bool b1, bool b2, bool b3) {
  return (b1 && b2) || (b2 && b3) || (b3 && b1);
}

bool AtLeastOneTrue(bool b1, bool b2, bool b3) {
  return (b1 || b2 || b3);
}
}  // namespace

namespace dash_packager {

SimpleMpdNotifier::SimpleMpdNotifier(const std::vector<std::string>& base_urls,
                                     const std::string& output_path)
    : base_urls_(base_urls), output_path_(output_path) {
}

SimpleMpdNotifier::~SimpleMpdNotifier() {
}

bool SimpleMpdNotifier::Init() {
  return true;
}

bool SimpleMpdNotifier::NotifyNewContainer(const MediaInfo& media_info,
                                           uint32* container_id) {
  DCHECK(container_id);

  ContentType content_type = GetContentType(media_info);
  if (content_type == kUnknown)
    return false;

  // Determine whether the media_info is for VOD or live.
  bool has_vod_only_fields = HasVODOnlyFields(media_info);
  bool has_live_only_fields = HasLiveOnlyFields(media_info);
  if (has_vod_only_fields && has_live_only_fields) {
    LOG(ERROR) << "MediaInfo with both VOD fields and Live fields is invalid.";
    return false;
  }
  if (!has_vod_only_fields && !has_live_only_fields) {
    LOG(ERROR) << "MediaInfo should contain either VOD fields or Live fields.";
    return false;
  }
  MpdBuilder::MpdType mpd_type =
      has_vod_only_fields ? MpdBuilder::kStatic : MpdBuilder::kDynamic;

  base::AutoLock auto_lock(lock_);
  // TODO(kqyang): Consider adding a new method MpdBuilder::AddRepresentation.
  // Most of the codes here can be moved inside.
  if (!mpd_builder_) {
    mpd_builder_.reset(new MpdBuilder(mpd_type));
    for (size_t i = 0; i < base_urls_.size(); ++i)
      mpd_builder_->AddBaseUrl(base_urls_[i]);
  } else {
    if (mpd_builder_->type() != mpd_type) {
      LOG(ERROR) << "Expecting MediaInfo with '"
                 << (has_vod_only_fields ? "Live" : "VOD") << "' fields.";
      return false;
    }
  }

  AdaptationSet** adaptation_set = &adaptation_set_map_[content_type];
  if (*adaptation_set == NULL)
    *adaptation_set = mpd_builder_->AddAdaptationSet();

  DCHECK(*adaptation_set);
  Representation* representation =
      (*adaptation_set)->AddRepresentation(media_info);
  if (representation == NULL)
    return false;

  *container_id = representation->id();

  if (has_vod_only_fields)
    return WriteMpdToFile();

  DCHECK(!ContainsKey(representation_map_, representation->id()));
  representation_map_[representation->id()] = representation;
  return true;
}

bool SimpleMpdNotifier::NotifyNewSegment(uint32 container_id,
                                         uint64 start_time,
                                         uint64 duration) {
  base::AutoLock auto_lock(lock_);

  RepresentationMap::iterator it = representation_map_.find(container_id);
  if (it == representation_map_.end()) {
    LOG(ERROR) << "Unexpected container_id: " << container_id;
    return false;
  }
  if (!it->second->AddNewSegment(start_time, duration))
    return false;
  return WriteMpdToFile();
}

bool SimpleMpdNotifier::AddContentProtectionElement(
    uint32 container_id,
    const ContentProtectionElement& content_protection_element) {
  NOTIMPLEMENTED();
  return false;
}

SimpleMpdNotifier::ContentType SimpleMpdNotifier::GetContentType(
    const MediaInfo& media_info) {
  const bool has_video = media_info.video_info().size() > 0;
  const bool has_audio = media_info.audio_info().size() > 0;
  const bool has_text = media_info.text_info().size() > 0;

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
    int64 length = file->Write(mpd_char_ptr, mpd_bytes_left);
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

}  // namespace dash_packager
