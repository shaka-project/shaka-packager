// Copyright 2016 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/hls/base/master_playlist.h"

#include <inttypes.h>

#include <cmath>
#include <list>
#include <map>
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

MasterPlaylist::MasterPlaylist(const std::string& file_name)
    : file_name_(file_name) {}
MasterPlaylist::~MasterPlaylist() {}

void MasterPlaylist::AddMediaPlaylist(MediaPlaylist* media_playlist) {
  media_playlists_.push_back(media_playlist);
}

bool MasterPlaylist::WriteAllPlaylists(const std::string& base_url,
                                       const std::string& output_dir) {
  if (!WriteMasterPlaylist(base_url, output_dir)) {
    LOG(ERROR) << "Failed to write master playlist.";
    return false;
  }

  double longest_segment_duration = 0.0;
  if (!has_set_playlist_target_duration_) {
    for (const MediaPlaylist* playlist : media_playlists_) {
      const double playlist_longest_segment =
          playlist->GetLongestSegmentDuration();
      if (longest_segment_duration < playlist_longest_segment)
        longest_segment_duration = playlist_longest_segment;
    }
  }

  base::FilePath output_path = base::FilePath::FromUTF8Unsafe(output_dir);
  for (MediaPlaylist* playlist : media_playlists_) {
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

  // TODO(rkuroiwa): This can be done in AddMediaPlaylist(), no need to create
  // map and list on the fly.
  std::map<std::string, std::list<const MediaPlaylist*>> audio_group_map;
  std::list<const MediaPlaylist*> video_playlists;
  for (const MediaPlaylist* media_playlist : media_playlists_) {
    MediaPlaylist::MediaPlaylistStreamType stream_type =
        media_playlist->stream_type();
    if (stream_type == MediaPlaylist::MediaPlaylistStreamType::kPlayListAudio) {
      auto& audio_playlists = audio_group_map[media_playlist->group_id()];
      audio_playlists.push_back(media_playlist);
    } else if (stream_type ==
               MediaPlaylist::MediaPlaylistStreamType::kPlayListVideo) {
      video_playlists.push_back(media_playlist);
    } else {
      NOTIMPLEMENTED() << static_cast<int>(stream_type) << " not handled.";
    }
  }

  // TODO(rkuroiwa): Handle audio only.
  std::string audio_output;
  std::string video_output;
  for (auto& group_id_audio_playlists : audio_group_map) {
    const std::string& group_id = group_id_audio_playlists.first;
    const std::list<const MediaPlaylist*>& audio_playlists =
        group_id_audio_playlists.second;

    uint64_t max_audio_bitrate = 0;
    for (const MediaPlaylist* audio_playlist : audio_playlists) {
      base::StringAppendF(
          &audio_output,
          "#EXT-X-MEDIA:TYPE=AUDIO,GROUP-ID=\"%s\",NAME=\"%s\",URI=\"%s\"\n",
          group_id.c_str(), audio_playlist->name().c_str(),
          (base_url + audio_playlist->file_name()).c_str());
      const uint64_t audio_bitrate = audio_playlist->Bitrate();
      if (audio_bitrate > max_audio_bitrate)
        max_audio_bitrate = audio_bitrate;
    }
    for (const MediaPlaylist* video_playlist : video_playlists) {
      const std::string& video_codec = video_playlist->codec();
      const uint64_t video_bitrate = video_playlist->Bitrate();

      // Assume all codecs are the same for same group ID.
      const std::string& audio_codec = audio_playlists.front()->codec();
      base::StringAppendF(
          &video_output,
          "#EXT-X-STREAM-INF:AUDIO=\"%s\",CODECS=\"%s\",BANDWIDTH=%" PRIu64 "\n"
          "%s\n",
          group_id.c_str(), (video_codec + "," + audio_codec).c_str(),
          video_bitrate + max_audio_bitrate,
          (base_url + video_playlist->file_name()).c_str());
    }
  }

  if (audio_group_map.empty()) {
    for (const MediaPlaylist* video_playlist : video_playlists) {
      const std::string& video_codec = video_playlist->codec();
      const uint64_t video_bitrate = video_playlist->Bitrate();
      base::StringAppendF(&video_output,
                          "#EXT-X-STREAM-INF:CODECS=\"%s\",BANDWIDTH=%" PRIu64
                          "\n%s\n",
                          video_codec.c_str(), video_bitrate,
                          (base_url + video_playlist->file_name()).c_str());
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
