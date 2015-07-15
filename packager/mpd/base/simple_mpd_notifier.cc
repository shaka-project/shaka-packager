// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/mpd/base/simple_mpd_notifier.h"

#include "packager/base/base64.h"
#include "packager/base/logging.h"
#include "packager/base/strings/string_number_conversions.h"
#include "packager/base/strings/string_util.h"
#include "packager/media/file/file.h"
#include "packager/mpd/base/mpd_builder.h"
#include "packager/mpd/base/mpd_utils.h"

using edash_packager::media::File;

namespace edash_packager {

namespace {

// Coverts binary data into human readable UUID format.
bool HexToUUID(const std::string& data, std::string* uuid_format) {
  DCHECK(uuid_format);
  const size_t kExpectedUUIDSize = 16;
  if (data.size() != kExpectedUUIDSize) {
    LOG(ERROR) << "Default key ID size is expected to be " << kExpectedUUIDSize
               << " but is " << data.size();
    return false;
  }

  const std::string hex_encoded =
      StringToLowerASCII(base::HexEncode(data.data(), data.size()));
  DCHECK_EQ(hex_encoded.size(), kExpectedUUIDSize * 2);
  base::StringPiece all(hex_encoded);
  // Note UUID has 5 parts separated with dashes.
  // e.g. 123e4567-e89b-12d3-a456-426655440000
  // These StringPieces have each part.
  base::StringPiece first = all.substr(0, 8);
  base::StringPiece second = all.substr(8, 4);
  base::StringPiece third = all.substr(12, 4);
  base::StringPiece fourth = all.substr(16, 4);
  base::StringPiece fifth= all.substr(20, 12);

  // 32 hexadecimal characters with 4 hyphens.
  const size_t kHumanReadableUUIDSize = 36;
  uuid_format->reserve(kHumanReadableUUIDSize);
  first.CopyToString(uuid_format);
  uuid_format->append("-");
  second.AppendToString(uuid_format);
  uuid_format->append("-");
  third.AppendToString(uuid_format);
  uuid_format->append("-");
  fourth.AppendToString(uuid_format);
  uuid_format->append("-");
  fifth.AppendToString(uuid_format);
  return true;
}

// This might be useful for DashIopCompliantMpdNotifier. If so it might make
// sense to template this so that it accepts Representation and AdaptationSet.
// For SimpleMpdNotifier, just put it in Representation. It should still
// generate a valid MPD.
void AddContentProtectionElements(const MediaInfo& media_info,
                                  Representation* representation) {
  DCHECK(representation);
  if (!media_info.has_protected_content())
    return;

  const MediaInfo::ProtectedContent& protected_content =
      media_info.protected_content();

  const char kEncryptedMp4Uri[] = "urn:mpeg:dash:mp4protection:2011";
  const char kEncryptedMp4Value[] = "cenc";

  // DASH MPD spec specifies a default ContentProtection element for ISO BMFF
  // (MP4) files.
  const bool is_mp4_container =
      media_info.container_type() == MediaInfo::CONTAINER_MP4;
  if (is_mp4_container) {
    ContentProtectionElement mp4_content_protection;
    mp4_content_protection.scheme_id_uri = kEncryptedMp4Uri;
    mp4_content_protection.value = kEncryptedMp4Value;
    if (protected_content.has_default_key_id()) {
      std::string key_id_uuid_format;
      if (HexToUUID(protected_content.default_key_id(), &key_id_uuid_format)) {
        mp4_content_protection.additional_attributes["cenc:default_KID"] =
            key_id_uuid_format;
      } else {
        LOG(ERROR) << "Failed to convert default key ID into UUID format.";
      }
    }

    representation->AddContentProtectionElement(mp4_content_protection);
  }

  for (int i = 0; i < protected_content.content_protection_entry().size();
       ++i) {
    const MediaInfo::ProtectedContent::ContentProtectionEntry& entry =
        protected_content.content_protection_entry(i);
    if (!entry.has_uuid()) {
      LOG(WARNING)
          << "ContentProtectionEntry was specified but no UUID is set for "
          << entry.name_version() << ", skipping.";
      continue;
    }

    ContentProtectionElement drm_content_protection;
    drm_content_protection.scheme_id_uri = "urn:uuid:" + entry.uuid();
    if (entry.has_name_version())
      drm_content_protection.value = entry.name_version();

    if (entry.has_pssh()) {
      std::string base64_encoded_pssh;
      base::Base64Encode(entry.pssh(), &base64_encoded_pssh);
      Element cenc_pssh;
      cenc_pssh.name = "cenc:pssh";
      cenc_pssh.content =  base64_encoded_pssh;
      drm_content_protection.subelements.push_back(cenc_pssh);
    }

    representation->AddContentProtectionElement(drm_content_protection);
  }

  LOG_IF(WARNING, protected_content.content_protection_entry().size() == 0)
      << "The media is encrypted but no content protection specified.";
}
}  // namespace

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

  AddContentProtectionElements(media_info, representation);
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
