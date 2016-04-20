// Copyright 2016 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/hls/base/simple_hls_notifier.h"

#include "packager/base/base64.h"
#include "packager/base/logging.h"
#include "packager/base/strings/string_number_conversions.h"
#include "packager/base/strings/stringprintf.h"
#include "packager/hls/base/media_playlist.h"
#include "packager/media/base/widevine_pssh_data.pb.h"

namespace edash_packager {
namespace hls {

namespace {
const uint8_t kSystemIdWidevine[] = {0xed, 0xef, 0x8b, 0xa9, 0x79, 0xd6,
                                     0x4a, 0xce, 0xa3, 0xc8, 0x27, 0xdc,
                                     0xd5, 0x1d, 0x21, 0xed};
bool IsWidevineSystemId(const std::vector<uint8_t>& system_id) {
  return system_id.size() == arraysize(kSystemIdWidevine) &&
         std::equal(system_id.begin(), system_id.end(), kSystemIdWidevine);
}
}  // namespace

MediaPlaylistFactory::~MediaPlaylistFactory() {}

scoped_ptr<MediaPlaylist> MediaPlaylistFactory::Create(
    MediaPlaylist::MediaPlaylistType type,
    const std::string& file_name,
    const std::string& name,
    const std::string& group_id) {
  return scoped_ptr<MediaPlaylist>(
      new MediaPlaylist(type, file_name, name, group_id));
}

SimpleHlsNotifier::SimpleHlsNotifier(HlsProfile profile,
                                     const std::string& prefix,
                                     const std::string& output_dir,
                                     const std::string& master_playlist_name)
    : HlsNotifier(profile),
      prefix_(prefix),
      output_dir_(output_dir),
      media_playlist_factory_(new MediaPlaylistFactory()),
      master_playlist_(new MasterPlaylist(master_playlist_name)),
      media_playlist_map_deleter_(&media_playlist_map_) {}

SimpleHlsNotifier::~SimpleHlsNotifier() {}

bool SimpleHlsNotifier::Init() {
  return true;
}

bool SimpleHlsNotifier::NotifyNewStream(const MediaInfo& media_info,
                                        const std::string& playlist_name,
                                        const std::string& name,
                                        const std::string& group_id,
                                        uint32_t* stream_id) {
  DCHECK(stream_id);
  *stream_id = sequence_number_.GetNext();

  MediaPlaylist::MediaPlaylistType type;
  switch (profile()) {
    case HlsProfile::kLiveProfile:
      type = MediaPlaylist::MediaPlaylistType::kLive;
      break;
    case HlsProfile::kOnDemandProfile:
      type = MediaPlaylist::MediaPlaylistType::kVod;
      break;
    default:
      NOTREACHED();
      return false;
  }
  scoped_ptr<MediaPlaylist> media_playlist =
      media_playlist_factory_->Create(type, playlist_name, name, group_id);
  if (!media_playlist->SetMediaInfo(media_info)) {
    LOG(ERROR) << "Failed to set media info for playlist " << playlist_name;
    return false;
  }

  base::AutoLock auto_lock(lock_);
  master_playlist_->AddMediaPlaylist(media_playlist.get());
  media_playlist_map_.insert(
      std::make_pair(*stream_id, media_playlist.release()));
  return true;
}

bool SimpleHlsNotifier::NotifyNewSegment(uint32_t stream_id,
                                         const std::string& segment_name,
                                         uint64_t start_time,
                                         uint64_t duration,
                                         uint64_t size) {
  base::AutoLock auto_lock(lock_);
  auto result = media_playlist_map_.find(stream_id);
  if (result == media_playlist_map_.end()) {
    LOG(ERROR) << "Cannot find stream with ID: " << stream_id;
    return false;
  }
  auto& media_playlist = result->second;
  media_playlist->AddSegment(prefix_ + segment_name, duration, size);
  return true;
}

// TODO(rkuroiwa): Add static key support. for common system id.
bool SimpleHlsNotifier::NotifyEncryptionUpdate(
    uint32_t stream_id,
    const std::vector<uint8_t>& key_id,
    const std::vector<uint8_t>& system_id,
    const std::vector<uint8_t>& iv,
    const std::vector<uint8_t>& protection_system_specific_data) {
  base::AutoLock auto_lock(lock_);
  auto result = media_playlist_map_.find(stream_id);
  if (result == media_playlist_map_.end()) {
    LOG(ERROR) << "Cannot find stream with ID: " << stream_id;
    return false;
  }
  if (!IsWidevineSystemId(system_id)) {
    LOG(ERROR) << "Unknown system ID: "
               << base::HexEncode(system_id.data(), system_id.size());
    return false;
  }

  media::WidevinePsshData pssh_data;
  if (!pssh_data.ParseFromArray(protection_system_specific_data.data(),
                                protection_system_specific_data.size())) {
    LOG(ERROR) << "Failed ot parse protection_system_specific_data.";
    return false;
  }
  if (!pssh_data.has_provider() || !pssh_data.has_content_id() ||
      pssh_data.key_id_size() == 0) {
    LOG(ERROR) << "Missing fields to generate URI.";
    return false;
  }

  std::string content_id_base64;
  base::Base64Encode(pssh_data.content_id(), &content_id_base64);
  std::string json_format = base::StringPrintf(
      "{"
      "\"provider\":\"%s\","
      "\"content_id\":\"%s\","
      "\"key_ids\":[",
      pssh_data.provider().c_str(), content_id_base64.c_str());
  json_format += "\"" + base::HexEncode(key_id.data(), key_id.size()) + "\",";
  for (const std::string& id: pssh_data.key_id()) {
    if (key_id.size() == id.size() &&
        memcmp(key_id.data(), id.data(), id.size()) == 0) {
      continue;
    }
    json_format += "\"" + base::HexEncode(id.data(), id.size()) + "\",";
  }
  json_format += "]}";
  std::string json_format_base64;
  base::Base64Encode(json_format, &json_format_base64);

  auto& media_playlist = result->second;
  std::string iv_string;
  if (!iv.empty()) {
    iv_string = "0x" + base::HexEncode(iv.data(), iv.size());
  }
  media_playlist->AddEncryptionInfo(
      MediaPlaylist::EncryptionMethod::kSampleAes,
      "data:text/plain;base64," + json_format_base64, iv_string,
      "com.widevine", "");
  return true;
}

bool SimpleHlsNotifier::Flush() {
  base::AutoLock auto_lock(lock_);
  return master_playlist_->WriteAllPlaylists(prefix_, output_dir_);
}

}  // namespace hls
}  // namespace edash_packager
