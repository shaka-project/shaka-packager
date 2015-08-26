// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/mpd/util/mpd_writer.h"

#include <google/protobuf/text_format.h>
#include <stdint.h>

#include "packager/media/file/file.h"
#include "packager/mpd/base/mpd_builder.h"
#include "packager/mpd/base/mpd_utils.h"

using edash_packager::media::File;

namespace edash_packager {

namespace {

// On entry set |has_video|, |has_audio|, and |has_text| to false.
// On success, return true and set appropriate |has_*| variables. Otherwise
// return false.
bool HasVideoAudioText(const std::list<MediaInfo>& media_infos,
                       bool* has_video,
                       bool* has_audio,
                       bool* has_text) {
  DCHECK(has_video);
  DCHECK(has_audio);
  DCHECK(has_text);

  *has_video = false;
  *has_audio = false;
  *has_text = false;

  for (std::list<MediaInfo>::const_iterator it = media_infos.begin();
       it != media_infos.end();
       ++it) {
    const MediaInfo& media_info = *it;
    const bool media_info_has_video = media_info.has_video_info();
    const bool media_info_has_audio = media_info.has_audio_info();
    const bool media_info_has_text = media_info.has_text_info();

    if (MoreThanOneTrue(
            media_info_has_video, media_info_has_audio, media_info_has_text)) {
      LOG(ERROR) << "MpdWriter cannot handle MediaInfo with more than "
                    "one stream.";
      return false;
    }

    if (!AtLeastOneTrue(
            media_info_has_video, media_info_has_audio, media_info_has_text)) {
      LOG(ERROR) << "MpdWriter requires that MediaInfo contain one "
                    "audio, video, or text stream.";
      return false;
    }

    *has_video = *has_video || media_info_has_video;
    *has_audio = *has_audio || media_info_has_audio;
    *has_text = *has_text || media_info_has_text;
  }

  return true;
}

bool SetMediaInfosToMpdBuilder(const std::list<MediaInfo>& media_infos,
                               MpdBuilder* mpd_builder) {
  if (media_infos.empty()) {
    LOG(ERROR) << "No MediaInfo to generate an MPD.";
    return false;
  }

  bool has_video = false;
  bool has_audio = false;
  bool has_text = false;
  if (!HasVideoAudioText(media_infos, &has_video, &has_audio, &has_text))
    return false;

  DCHECK(mpd_builder);

  // [type][lang] = AdaptationSet
  std::map<std::string, std::map<std::string, AdaptationSet*> > map;
  // This puts video sets into the map first, which keeps some pre-existing
  // test expectations from changing.
  if (has_video) {
    map["video"][""] = mpd_builder->AddAdaptationSet("");
  }

  for (std::list<MediaInfo>::const_iterator it = media_infos.begin();
       it != media_infos.end();
       ++it) {
    const MediaInfo& media_info = *it;
    DCHECK(OnlyOneTrue(media_info.has_video_info(),
                       media_info.has_audio_info(),
                       media_info.has_text_info()));

    std::string lang;
    AdaptationSet** adaptation_set = NULL;
    if (media_info.has_video_info()) {
      adaptation_set = &map["video"][lang];
    } else if (media_info.has_audio_info()) {
      lang = media_info.audio_info().language();
      adaptation_set = &map["audio"][lang];
    } else if (media_info.has_text_info()) {
      adaptation_set = &map["text"][lang];
    }
    if (!*adaptation_set) {
      *adaptation_set = mpd_builder->AddAdaptationSet(lang);
    }

    Representation* representation =
        (*adaptation_set)->AddRepresentation(media_info);
    if (!representation) {
      LOG(ERROR) << "Failed to add representation.";
      return false;
    }
  }

  return true;
}
}  // namespace

MpdWriter::MpdWriter() {}
MpdWriter::~MpdWriter() {}

bool MpdWriter::AddFile(const std::string& media_info_path,
                        const std::string& mpd_path) {
  std::string file_content;
  if (!media::File::ReadFileToString(media_info_path.c_str(),
                                     &file_content)) {
    LOG(ERROR) << "Failed to read " << media_info_path << " to string.";
    return false;
  }

  MediaInfo media_info;
  if (!::google::protobuf::TextFormat::ParseFromString(file_content,
                                                       &media_info)) {
    LOG(ERROR) << "Failed to parse " << file_content << " to MediaInfo.";
    return false;
  }

  MpdBuilder::MakePathsRelativeToMpd(mpd_path, &media_info);
  media_infos_.push_back(media_info);
  return true;
}

void MpdWriter::AddBaseUrl(const std::string& base_url) {
  base_urls_.push_back(base_url);
}

// NOTE: The only use case we have for this is static profile, i.e. VOD.
bool MpdWriter::WriteMpdToString(std::string* output) {
  CHECK(output);

  MpdBuilder mpd_builder(MpdBuilder::kStatic, MpdOptions());
  for (std::list<std::string>::const_iterator it = base_urls_.begin();
       it != base_urls_.end();
       ++it) {
    const std::string& base_url = *it;
    mpd_builder.AddBaseUrl(base_url);
  }

  if (!SetMediaInfosToMpdBuilder(media_infos_, &mpd_builder)) {
    LOG(ERROR) << "Failed to set MediaInfos to MpdBuilder.";
    return false;
  }

  return mpd_builder.ToString(output);
}

bool MpdWriter::WriteMpdToFile(const char* file_name) {
  CHECK(file_name);

  std::string mpd;
  if (!WriteMpdToString(&mpd)) {
    LOG(ERROR) << "Failed to write MPD to string.";
    return false;
  }

  File* file = File::Open(file_name, "w");
  if (!file) {
    LOG(ERROR) << "Failed to write MPD to string.";
    return false;
  }

  const char* mpd_char_ptr = mpd.data();
  size_t mpd_bytes_left = mpd.size();
  while (mpd_bytes_left > 0) {
    int64_t length = file->Write(mpd_char_ptr, mpd_bytes_left);
    if (length < 0) {
      LOG(ERROR) << "Write error " << length;
      return false;
    }

    if (static_cast<size_t>(length) > mpd_bytes_left) {
      LOG(ERROR) << "Wrote " << length << " bytes but there was only "
                 << mpd_bytes_left << " bytes to write.";
      return false;
    }

    mpd_char_ptr += length;
    mpd_bytes_left -= length;
  }

  if (!file->Flush()) {
    LOG(ERROR) << "Failed to flush file.";
    return false;
  }

  return file->Close();
}

}  // namespace edash_packager
