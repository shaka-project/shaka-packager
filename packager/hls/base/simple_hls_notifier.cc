// Copyright 2016 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/hls/base/simple_hls_notifier.h"

#include <cmath>

#include "packager/base/base64.h"
#include "packager/base/files/file_path.h"
#include "packager/base/json/json_writer.h"
#include "packager/base/logging.h"
#include "packager/base/optional.h"
#include "packager/base/strings/string_number_conversions.h"
#include "packager/base/strings/stringprintf.h"
#include "packager/hls/base/media_playlist.h"
#include "packager/media/base/fixed_key_source.h"
#include "packager/media/base/protection_system_specific_info.h"
#include "packager/media/base/widevine_key_source.h"
#include "packager/media/base/widevine_pssh_data.pb.h"

namespace shaka {
namespace hls {

namespace {

const char kUriBase64Prefix[] = "data:text/plain;base64,";
const char kUriFairplayPrefix[] = "skd://";
const char kWidevineDashIfIopUUID[] =
    "urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed";

bool IsWidevineSystemId(const std::vector<uint8_t>& system_id) {
  return system_id.size() == arraysize(media::kWidevineSystemId) &&
         std::equal(system_id.begin(), system_id.end(),
                    media::kWidevineSystemId);
}

bool IsCommonSystemId(const std::vector<uint8_t>& system_id) {
  return system_id.size() == arraysize(media::kCommonSystemId) &&
         std::equal(system_id.begin(), system_id.end(), media::kCommonSystemId);
}

std::string Base64EncodeUri(const std::string& prefix,
                            const std::string& data) {
    std::string data_base64;
    base::Base64Encode(data, &data_base64);
    return prefix + data_base64;
}

std::string VectorToString(const std::vector<uint8_t>& v) {
    return std::string(v.begin(), v.end());
}

bool IsFairplaySystemId(const std::vector<uint8_t>& system_id) {
  return system_id.size() == arraysize(media::kFairplaySystemId) &&
      std::equal(system_id.begin(), system_id.end(), media::kFairplaySystemId);
}

// TODO(rkuroiwa): Dedup these with the functions in MpdBuilder.
std::string MakePathRelative(const std::string& original_path,
                             const std::string& output_dir) {
  return (original_path.find(output_dir) == 0)
             ? original_path.substr(output_dir.size())
             : original_path;
}

void MakePathsRelativeToOutputDirectory(const std::string& output_dir,
                                        MediaInfo* media_info) {
  DCHECK(media_info);
  const std::string kFileProtocol("file://");
  std::string prefix_stripped_output_dir =
      (output_dir.find(kFileProtocol) == 0)
          ? output_dir.substr(kFileProtocol.size())
          : output_dir;

  if (prefix_stripped_output_dir.empty())
    return;

  std::string directory_with_separator(
      base::FilePath::FromUTF8Unsafe(prefix_stripped_output_dir)
      .AsEndingWithSeparator()
      .AsUTF8Unsafe());
  if (directory_with_separator.empty())
    return;

  if (media_info->has_media_file_name()) {
    media_info->set_media_file_name(MakePathRelative(
        media_info->media_file_name(), directory_with_separator));
  }
  if (media_info->has_segment_template()) {
    media_info->set_segment_template(MakePathRelative(
        media_info->segment_template(), directory_with_separator));
  }
}

bool WidevinePsshToJson(const std::vector<uint8_t>& pssh_box,
                        const std::vector<uint8_t>& key_id,
                        std::string* pssh_json) {
  media::ProtectionSystemSpecificInfo pssh_info;
  if (!pssh_info.Parse(pssh_box.data(), pssh_box.size())) {
    LOG(ERROR) << "Failed to parse PSSH box.";
    return false;
  }

  media::WidevinePsshData pssh_proto;
  if (!pssh_proto.ParseFromArray(pssh_info.pssh_data().data(),
                                 pssh_info.pssh_data().size())) {
    LOG(ERROR) << "Failed to parse protection_system_specific_data.";
    return false;
  }
  if (!pssh_proto.has_provider() ||
      (!pssh_proto.has_content_id() && pssh_proto.key_id_size() == 0)) {
    LOG(ERROR) << "Missing fields to generate URI.";
    return false;
  }

  base::DictionaryValue pssh_dict;
  pssh_dict.SetString("provider", pssh_proto.provider());
  if (pssh_proto.has_content_id()) {
    std::string content_id_base64;
    base::Base64Encode(base::StringPiece(pssh_proto.content_id().data(),
                                         pssh_proto.content_id().size()),
                       &content_id_base64);
    pssh_dict.SetString("content_id", content_id_base64);
  }
  base::ListValue* key_ids = new base::ListValue();

  key_ids->AppendString(base::HexEncode(key_id.data(), key_id.size()));
  for (const std::string& id : pssh_proto.key_id()) {
    if (key_id.size() == id.size() &&
        memcmp(key_id.data(), id.data(), id.size()) == 0) {
      continue;
    }
    key_ids->AppendString(base::HexEncode(id.data(), id.size()));
  }
  pssh_dict.Set("key_ids", key_ids);

  if (!base::JSONWriter::Write(pssh_dict, pssh_json)) {
    LOG(ERROR) << "Failed to write to JSON.";
    return false;
  }
  return true;
}

base::Optional<MediaPlaylist::EncryptionMethod> StringToEncrypionMethod(
    const std::string& method) {
  if (method == "cenc") {
    return MediaPlaylist::EncryptionMethod::kSampleAesCenc;
  } else if (method == "cbcs") {
    return MediaPlaylist::EncryptionMethod::kSampleAes;
  } else if (method == "cbca") {
    // cbca is a place holder for sample aes.
    return MediaPlaylist::EncryptionMethod::kSampleAes;
  } else {
    return base::nullopt;
  }
}

void NotifyEncryptionToMediaPlaylist(
    MediaPlaylist::EncryptionMethod encryption_method,
    const std::string& uri,
    const std::vector<uint8_t>& key_id,
    const std::vector<uint8_t>& iv,
    const std::string& key_format,
    const std::string& key_format_version,
    MediaPlaylist* media_playlist) {
  std::string iv_string;
  if (!iv.empty()) {
    iv_string = "0x" + base::HexEncode(iv.data(), iv.size());
  }
  std::string key_id_string;
  if (!key_id.empty()) {
    key_id_string = "0x" + base::HexEncode(key_id.data(), key_id.size());
  }

  media_playlist->AddEncryptionInfo(
      encryption_method,
      uri, key_id_string, iv_string,
      key_format, key_format_version);
}

// Creates JSON format and the format similar to MPD.
bool HandleWidevineKeyFormats(
    MediaPlaylist::EncryptionMethod encryption_method,
    const std::vector<uint8_t>& key_id,
    const std::vector<uint8_t>& iv,
    const std::vector<uint8_t>& protection_system_specific_data,
    MediaPlaylist* media_playlist) {
  if (encryption_method == MediaPlaylist::EncryptionMethod::kSampleAes) {
    // This format allows SAMPLE-AES only.
    std::string key_uri_data;
    if (!WidevinePsshToJson(protection_system_specific_data, key_id,
                            &key_uri_data)) {
      return false;
    }



    std::string key_uri_data_base64 =
        Base64EncodeUri(kUriBase64Prefix, key_uri_data);

    NotifyEncryptionToMediaPlaylist(encryption_method, key_uri_data_base64,
                                    std::vector<uint8_t>(), iv, "com.widevine",
                                    "1", media_playlist);
  }

  std::string pssh_as_string(
      reinterpret_cast<const char*>(protection_system_specific_data.data()),
      protection_system_specific_data.size());

  std::string key_uri_data_base64 =
      Base64EncodeUri(kUriBase64Prefix, pssh_as_string);

  NotifyEncryptionToMediaPlaylist(encryption_method, key_uri_data_base64,
                                  key_id, iv, kWidevineDashIfIopUUID, "1",
                                  media_playlist);
  return true;
}

bool WriteMediaPlaylist(const std::string& output_dir,
                        MediaPlaylist* playlist) {
  std::string file_path =
      base::FilePath::FromUTF8Unsafe(output_dir)
          .Append(base::FilePath::FromUTF8Unsafe(playlist->file_name()))
          .AsUTF8Unsafe();
  if (!playlist->WriteToFile(file_path)) {
    LOG(ERROR) << "Failed to write playlist " << file_path;
    return false;
  }
  return true;
}

}  // namespace

MediaPlaylistFactory::~MediaPlaylistFactory() {}

std::unique_ptr<MediaPlaylist> MediaPlaylistFactory::Create(
    HlsPlaylistType type,
    double time_shift_buffer_depth,
    const std::string& file_name,
    const std::string& name,
    const std::string& group_id) {
  return std::unique_ptr<MediaPlaylist>(new MediaPlaylist(
      type, time_shift_buffer_depth, file_name, name, group_id));
}

SimpleHlsNotifier::SimpleHlsNotifier(HlsPlaylistType playlist_type,
                                     double time_shift_buffer_depth,
                                     const std::string& prefix,
                                     const std::string& output_dir,
                                     const std::string& master_playlist_name)
    : HlsNotifier(playlist_type),
      time_shift_buffer_depth_(time_shift_buffer_depth),
      prefix_(prefix),
      output_dir_(output_dir),
      media_playlist_factory_(new MediaPlaylistFactory()),
      master_playlist_(new MasterPlaylist(master_playlist_name)) {}

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

  MediaInfo adjusted_media_info(media_info);
  MakePathsRelativeToOutputDirectory(output_dir_, &adjusted_media_info);

  std::unique_ptr<MediaPlaylist> media_playlist =
      media_playlist_factory_->Create(playlist_type(), time_shift_buffer_depth_,
                                      playlist_name, name, group_id);
  if (!media_playlist->SetMediaInfo(adjusted_media_info)) {
    LOG(ERROR) << "Failed to set media info for playlist " << playlist_name;
    return false;
  }

  MediaPlaylist::EncryptionMethod encryption_method =
      MediaPlaylist::EncryptionMethod::kNone;
  if (media_info.protected_content().has_protection_scheme()) {
    const std::string& protection_scheme =
        media_info.protected_content().protection_scheme();
    base::Optional<MediaPlaylist::EncryptionMethod> enc_method =
        StringToEncrypionMethod(protection_scheme);
    if (!enc_method) {
      LOG(ERROR) << "Failed to recognize protection scheme "
                 << protection_scheme;
      return false;
    }
    encryption_method = enc_method.value();
  }

  *stream_id = sequence_number_.GetNext();
  base::AutoLock auto_lock(lock_);
  master_playlist_->AddMediaPlaylist(media_playlist.get());
  stream_map_[*stream_id].reset(
      new StreamEntry{std::move(media_playlist), encryption_method});
  return true;
}

bool SimpleHlsNotifier::NotifyNewSegment(uint32_t stream_id,
                                         const std::string& segment_name,
                                         uint64_t start_time,
                                         uint64_t duration,
                                         uint64_t start_byte_offset,
                                         uint64_t size) {
  base::AutoLock auto_lock(lock_);
  auto stream_iterator = stream_map_.find(stream_id);
  if (stream_iterator == stream_map_.end()) {
    LOG(ERROR) << "Cannot find stream with ID: " << stream_id;
    return false;
  }
  const std::string relative_segment_name =
      MakePathRelative(segment_name, output_dir_);

  auto& media_playlist = stream_iterator->second->media_playlist;
  media_playlist->AddSegment(prefix_ + relative_segment_name, start_time,
                             duration, start_byte_offset, size);

  // Update target duration.
  uint32_t longest_segment_duration =
      static_cast<uint32_t>(ceil(media_playlist->GetLongestSegmentDuration()));
  bool target_duration_updated = false;
  if (longest_segment_duration > target_duration_) {
    target_duration_ = longest_segment_duration;
    target_duration_updated = true;
  }

  // Update the playlists when there is new segments in live mode.
  if (playlist_type() == HlsPlaylistType::kLive ||
      playlist_type() == HlsPlaylistType::kEvent) {
    if (!master_playlist_->WriteMasterPlaylist(prefix_, output_dir_)) {
      LOG(ERROR) << "Failed to write master playlist.";
      return false;
    }
    // Update all playlists if target duration is updated.
    if (target_duration_updated) {
      for (auto& streams : stream_map_) {
        MediaPlaylist* playlist = streams.second->media_playlist.get();
        playlist->SetTargetDuration(target_duration_);
        if (!WriteMediaPlaylist(output_dir_, playlist))
          return false;
      }
    } else {
      return WriteMediaPlaylist(output_dir_, media_playlist.get());
    }
  }
  return true;
}

bool SimpleHlsNotifier::NotifyEncryptionUpdate(
    uint32_t stream_id,
    const std::vector<uint8_t>& key_id,
    const std::vector<uint8_t>& system_id,
    const std::vector<uint8_t>& iv,
    const std::vector<uint8_t>& protection_system_specific_data) {
  base::AutoLock auto_lock(lock_);
  auto stream_iterator = stream_map_.find(stream_id);
  if (stream_iterator == stream_map_.end()) {
    LOG(ERROR) << "Cannot find stream with ID: " << stream_id;
    return false;
  }

  std::unique_ptr<MediaPlaylist>& media_playlist =
      stream_iterator->second->media_playlist;
  const MediaPlaylist::EncryptionMethod encryption_method =
      stream_iterator->second->encryption_method;
  LOG_IF(WARNING, encryption_method == MediaPlaylist::EncryptionMethod::kNone)
      << "Got encryption notification but the encryption method is NONE";
  if (IsWidevineSystemId(system_id)) {
    return HandleWidevineKeyFormats(encryption_method,
                                    key_id, iv, protection_system_specific_data,
                                    media_playlist.get());
  }
  if (IsCommonSystemId(system_id)) {
    // Use key_id as the key_uri. The player needs to have custom logic to
    // convert it to the actual key url.
    std::string key_uri_data = VectorToString(key_id);
    std::string key_uri_data_base64 =
        Base64EncodeUri(kUriBase64Prefix, key_uri_data);
    NotifyEncryptionToMediaPlaylist(encryption_method,
                                    key_uri_data_base64, std::vector<uint8_t>(),
                                    iv, "identity", "", media_playlist.get());
    return true;
  }

  if (IsFairplaySystemId(system_id)) {
    // Use key_id as the key_uri. The player needs to have custom logic to
    // convert it to the actual key url.
    std::string key_uri_data = VectorToString(key_id);
    std::string key_uri_data_base64 =
        Base64EncodeUri(kUriFairplayPrefix, key_uri_data);

    // Fairplay defines IV to be carried with the key, not the playlist.
    NotifyEncryptionToMediaPlaylist(encryption_method,
                                    key_uri_data_base64, std::vector<uint8_t>(),
                                    std::vector<uint8_t>(),
                                    "com.apple.streamingkeydelivery", "1",
                                    media_playlist.get());
    return true;
  }

  LOG(ERROR) << "Unknown system ID: "
             << base::HexEncode(system_id.data(), system_id.size());
  return false;
}



bool SimpleHlsNotifier::Flush() {
  base::AutoLock auto_lock(lock_);
  if (!master_playlist_->WriteMasterPlaylist(prefix_, output_dir_)) {
    LOG(ERROR) << "Failed to write master playlist.";
    return false;
  }
  for (auto& streams : stream_map_) {
    MediaPlaylist* playlist = streams.second->media_playlist.get();
    playlist->SetTargetDuration(target_duration_);
    if (!WriteMediaPlaylist(output_dir_, playlist))
      return false;
  }
  return true;
}

}  // namespace hls
}  // namespace shaka
