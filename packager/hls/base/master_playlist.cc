// Copyright 2016 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/hls/base/master_playlist.h"

#include <inttypes.h>

#include <cmath>
#include <set>

#include "packager/base/files/file_path.h"
#include "packager/base/strings/string_number_conversions.h"
#include "packager/base/strings/stringprintf.h"
#include "packager/hls/base/media_playlist.h"
#include "packager/media/file/file.h"
#include "packager/media/file/file_closer.h"
#include "packager/version/version.h"

namespace shaka {
namespace hls {

namespace {

void AppendStreamInfoTag(uint64_t bitrate,
                         const std::string& codecs,
                         uint32_t width,
                         uint32_t height,
                         const std::string* audio_group_id,
                         const std::string& base_url,
                         const std::string& file_name,
                         std::string* out) {
  DCHECK(out);
  base::StringAppendF(out, "#EXT-X-STREAM-INF:");
  base::StringAppendF(out, "BANDWIDTH=%" PRIu64, bitrate);
  base::StringAppendF(out, ",CODECS=\"%s\"", codecs.c_str());
  base::StringAppendF(out, ",RESOLUTION=%" PRIu32 "x%" PRIu32, width, height);

  if (audio_group_id) {
    base::StringAppendF(out, ",AUDIO=\"%s\"", audio_group_id->c_str());
  }

  base::StringAppendF(out, "\n%s%s\n", base_url.c_str(), file_name.c_str());
}
}  // namespace

MasterPlaylist::MasterPlaylist(const std::string& file_name)
    : file_name_(file_name) {}
MasterPlaylist::~MasterPlaylist() {}

void MasterPlaylist::AddMediaPlaylist(MediaPlaylist* media_playlist) {
  DCHECK(media_playlist);
  switch (media_playlist->stream_type()) {
    case MediaPlaylist::MediaPlaylistStreamType::kPlayListAudio: {
      const std::string& group_id = media_playlist->group_id();
      audio_playlist_groups_[group_id].push_back(media_playlist);
      break;
    }
    case MediaPlaylist::MediaPlaylistStreamType::kPlayListVideo: {
      video_playlists_.push_back(media_playlist);
      break;
    }
    default: {
      NOTIMPLEMENTED() << static_cast<int>(media_playlist->stream_type())
                       << " not handled.";
      break;
    }
  }
  // Sometimes we need to iterate over all playlists, so keep a collection
  // of all playlists to make iterating easier.
  all_playlists_.push_back(media_playlist);
}

bool MasterPlaylist::WriteAllPlaylists(const std::string& base_url,
                                       const std::string& output_dir) {
  if (!WriteMasterPlaylist(base_url, output_dir)) {
    LOG(ERROR) << "Failed to write master playlist.";
    return false;
  }

  double longest_segment_duration = 0.0;
  if (!has_set_playlist_target_duration_) {
    for (const MediaPlaylist* playlist : all_playlists_) {
      const double playlist_longest_segment =
          playlist->GetLongestSegmentDuration();
      if (longest_segment_duration < playlist_longest_segment)
        longest_segment_duration = playlist_longest_segment;
    }
  }

  base::FilePath output_path = base::FilePath::FromUTF8Unsafe(output_dir);
  for (MediaPlaylist* playlist : all_playlists_) {
    std::string file_path =
        output_path
            .Append(base::FilePath::FromUTF8Unsafe(playlist->file_name()))
            .AsUTF8Unsafe();
    if (!has_set_playlist_target_duration_) {
      const bool set_target_duration = playlist->SetTargetDuration(
          static_cast<uint32_t>(ceil(longest_segment_duration)));
      LOG_IF(WARNING, !set_target_duration)
          << "Target duration was already set for " << file_path;
    }

    std::unique_ptr<media::File, media::FileCloser> file(
        media::File::Open(file_path.c_str(), "w"));
    if (!file) {
      LOG(ERROR) << "Failed to open file " << file_path;
      return false;
    }
    if (!playlist->WriteToFile(file.get())) {
      LOG(ERROR) << "Failed to write playlist " << file_path;
      return false;
    }
  }

  has_set_playlist_target_duration_ = true;
  return true;
}

bool MasterPlaylist::WriteMasterPlaylist(const std::string& base_url,
                                         const std::string& output_dir) {
  std::string file_path =
      base::FilePath::FromUTF8Unsafe(output_dir)
          .Append(base::FilePath::FromUTF8Unsafe(file_name_))
          .AsUTF8Unsafe();
  std::unique_ptr<media::File, media::FileCloser> file(
      media::File::Open(file_path.c_str(), "w"));
  if (!file) {
    LOG(ERROR) << "Failed to open file " << file_path;
    return false;
  }

  // TODO(rkuroiwa): Handle audio only.
  std::string audio_output;
  std::string video_output;
  for (const auto& group_id_audio_playlists : audio_playlist_groups_) {
    const std::string& group_id = group_id_audio_playlists.first;
    const std::list<const MediaPlaylist*>& audio_playlists =
        group_id_audio_playlists.second;

    uint64_t max_audio_bitrate = 0;
    for (const MediaPlaylist* audio_playlist : audio_playlists) {
      base::StringAppendF(
          &audio_output,
          "#EXT-X-MEDIA:TYPE=AUDIO,GROUP-ID=\"%s\",NAME=\"%s\",",
          group_id.c_str(), audio_playlist->name().c_str());
      std::string language = audio_playlist->GetLanguage();
      if (!language.empty()) {
        base::StringAppendF(
            &audio_output,
            "LANGUAGE=\"%s\",",
            language.c_str());
      }
      base::StringAppendF(
          &audio_output,
          "URI=\"%s\"\n",
          (base_url + audio_playlist->file_name()).c_str());
      const uint64_t audio_bitrate = audio_playlist->Bitrate();
      if (audio_bitrate > max_audio_bitrate)
        max_audio_bitrate = audio_bitrate;
    }
    for (const MediaPlaylist* video_playlist : video_playlists_) {
      const std::string& video_codec = video_playlist->codec();
      const uint64_t video_bitrate = video_playlist->Bitrate();

      // Assume all codecs are the same for same group ID.
      const std::string& audio_codec = audio_playlists.front()->codec();

      uint32_t video_width;
      uint32_t video_height;
      CHECK(video_playlist->GetResolution(&video_width, &video_height));

      AppendStreamInfoTag(video_bitrate + max_audio_bitrate,
                          video_codec + "," + audio_codec,
                          video_width,
                          video_height,
                          &group_id,
                          base_url,
                          video_playlist->file_name(),
                          &video_output);
    }
  }

  if (audio_playlist_groups_.empty()) {
    for (const MediaPlaylist* video_playlist : video_playlists_) {
      const std::string& video_codec = video_playlist->codec();
      const uint64_t video_bitrate = video_playlist->Bitrate();

      uint32_t video_width;
      uint32_t video_height;
      CHECK(video_playlist->GetResolution(&video_width, &video_height));

      AppendStreamInfoTag(video_bitrate,
                          video_codec,
                          video_width,
                          video_height,
                          nullptr,
                          base_url,
                          video_playlist->file_name(),
                          &video_output);
    }
  }

  const std::string version = GetPackagerVersion();
  std::string version_line;
  if (!version.empty()) {
    version_line =
        base::StringPrintf("## Generated with %s version %s\n",
                           GetPackagerProjectUrl().c_str(), version.c_str());
  }

  std::string content =
      "#EXTM3U\n" + version_line + audio_output + video_output;
  int64_t bytes_written = file->Write(content.data(), content.size());
  if (bytes_written < 0) {
    LOG(ERROR) << "Error while writing master playlist " << file_path;
    return false;
  }
  if (static_cast<size_t>(bytes_written) != content.size()) {
    LOG(ERROR) << "Written " << bytes_written << " but content size is "
               << content.size() << " " << file_path;
    return false;
  }

  return true;
}

}  // namespace hls
}  // namespace shaka
