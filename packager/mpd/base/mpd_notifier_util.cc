// Copyright 2015 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/mpd/base/mpd_notifier_util.h"

#include "packager/base/strings/string_number_conversions.h"
#include "packager/base/strings/string_util.h"
#include "packager/media/file/file_closer.h"
#include "packager/media/file/file.h"
#include "packager/mpd/base/mpd_utils.h"

namespace edash_packager {

using media::File;
using media::FileCloser;

namespace {

// Helper function for adding ContentProtection for AdaptatoinSet or
// Representation.
// Works because both classes have AddContentProtectionElement().
template <typename ContentProtectionParent>
void AddContentProtectionElementsHelper(const MediaInfo& media_info,
                                        ContentProtectionParent* parent) {
  DCHECK(parent);
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

    parent->AddContentProtectionElement(mp4_content_protection);
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
      cenc_pssh.content = base64_encoded_pssh;
      drm_content_protection.subelements.push_back(cenc_pssh);
    }

    parent->AddContentProtectionElement(drm_content_protection);
  }

  LOG_IF(WARNING, protected_content.content_protection_entry().size() == 0)
      << "The media is encrypted but no content protection specified.";
}

}  // namespace

// Coverts binary data into human readable UUID format.
bool HexToUUID(const std::string& data, std::string* uuid_format) {
  DCHECK(uuid_format);
  const size_t kExpectedUUIDSize = 16;
  if (data.size() != kExpectedUUIDSize) {
    LOG(ERROR) << "UUID size is expected to be " << kExpectedUUIDSize
               << " but is " << data.size() << " and the data in hex is "
               << base::HexEncode(data.data(), data.size());
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
  base::StringPiece fifth = all.substr(20, 12);

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

bool WriteMpdToFile(const std::string& output_path, MpdBuilder* mpd_builder) {
  CHECK(!output_path.empty());

  std::string mpd;
  if (!mpd_builder->ToString(&mpd)) {
    LOG(ERROR) << "Failed to write MPD to string.";
    return false;
  }

  scoped_ptr<File, FileCloser> file(File::Open(output_path.c_str(), "w"));
  if (!file) {
    LOG(ERROR) << "Failed to open file for writing: " << output_path;
    return false;
  }

  const char* mpd_char_ptr = mpd.data();
  size_t mpd_bytes_left = mpd.size();
  while (mpd_bytes_left > 0) {
    int64_t length = file->Write(mpd_char_ptr, mpd_bytes_left);
    if (length <= 0) {
      LOG(ERROR) << "Failed to write to file '" << output_path << "' ("
                 << length << ").";
      return false;
    }
    mpd_char_ptr += length;
    mpd_bytes_left -= length;
  }
  // Release the pointer because Close() destructs itself.
  return file.release()->Close();
}

ContentType GetContentType(const MediaInfo& media_info) {
  const bool has_video = media_info.has_video_info();
  const bool has_audio = media_info.has_audio_info();
  const bool has_text = media_info.has_text_info();

  if (MoreThanOneTrue(has_video, has_audio, has_text)) {
    NOTIMPLEMENTED() << "MediaInfo with more than one stream is not supported.";
    return kContentTypeUnknown;
  }
  if (!AtLeastOneTrue(has_video, has_audio, has_text)) {
    LOG(ERROR) << "MediaInfo should contain one audio, video, or text stream.";
    return kContentTypeUnknown;
  }
  return has_video ? kContentTypeVideo
                   : (has_audio ? kContentTypeAudio : kContentTypeText);
}

void AddContentProtectionElements(const MediaInfo& media_info,
                                  AdaptationSet* adaptation_set) {
  AddContentProtectionElementsHelper(media_info, adaptation_set);
}

void AddContentProtectionElements(const MediaInfo& media_info,
                                  Representation* representation) {
  AddContentProtectionElementsHelper(media_info, representation);
}

}  // namespace edash_packager
