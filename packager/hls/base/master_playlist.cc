// Copyright 2016 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/hls/base/master_playlist.h"

#include <inttypes.h>

#include "packager/base/files/file_path.h"
#include "packager/base/strings/string_number_conversions.h"
#include "packager/base/strings/stringprintf.h"
#include "packager/file/file.h"
#include "packager/hls/base/media_playlist.h"
#include "packager/version/version.h"

namespace shaka {
namespace hls {

namespace {

void AppendMediaTag(const std::string& base_url,
                    const std::string& group_id,
                    const MediaPlaylist* audio_playlist,
                    std::string* out) {
  DCHECK(audio_playlist);
  DCHECK(out);

  out->append("#EXT-X-MEDIA:TYPE=AUDIO");
  base::StringAppendF(out, ",URI=\"%s\"",
                      (base_url + audio_playlist->file_name()).c_str());
  base::StringAppendF(out, ",GROUP-ID=\"%s\"", group_id.c_str());
  std::string language = audio_playlist->GetLanguage();
  if (!language.empty())
    base::StringAppendF(out, ",LANGUAGE=\"%s\"", language.c_str());
  base::StringAppendF(out, ",NAME=\"%s\"", audio_playlist->name().c_str());
  base::StringAppendF(out, ",CHANNELS=\"%d\"",
                      audio_playlist->GetNumChannels());
  out->append("\n");
}

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

bool MasterPlaylist::WriteMasterPlaylist(const std::string& base_url,
                                         const std::string& output_dir) {
  // TODO(rkuroiwa): Handle audio only.
  std::string audio_output;
  std::string video_output;
  for (const auto& group_id_audio_playlists : audio_playlist_groups_) {
    const std::string& group_id = group_id_audio_playlists.first;
    const std::list<const MediaPlaylist*>& audio_playlists =
        group_id_audio_playlists.second;

    uint64_t max_audio_bitrate = 0;
    for (const MediaPlaylist* audio_playlist : audio_playlists) {
      AppendMediaTag(base_url, group_id, audio_playlist, &audio_output);
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
      CHECK(video_playlist->GetDisplayResolution(&video_width, &video_height));

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
      CHECK(video_playlist->GetDisplayResolution(&video_width, &video_height));

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

  // Skip if the playlist is already written.
  if (content == written_playlist_)
    return true;

  std::string file_path =
      base::FilePath::FromUTF8Unsafe(output_dir)
          .Append(base::FilePath::FromUTF8Unsafe(file_name_))
          .AsUTF8Unsafe();
  if (!File::WriteFileAtomically(file_path.c_str(), content)) {
    LOG(ERROR) << "Failed to write master playlist to: " << file_path;
    return false;
  }
  written_playlist_ = content;
  return true;
}

}  // namespace hls
}  // namespace shaka
