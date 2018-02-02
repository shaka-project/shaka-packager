// Copyright 2016 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/hls/base/master_playlist.h"

#include <algorithm>  // std::max

#include <inttypes.h>

#include "packager/base/files/file_path.h"
#include "packager/base/strings/string_number_conversions.h"
#include "packager/base/strings/stringprintf.h"
#include "packager/file/file.h"
#include "packager/hls/base/media_playlist.h"
#include "packager/hls/base/tag.h"
#include "packager/version/version.h"

namespace shaka {
namespace hls {
namespace {
const char* kDefaultAudioGroupId = "default-audio-group";
const char* kDefaultSubtitleGroupId = "default-text-group";
const char* kUnexpectedGroupId = "unexpected-group";

struct Variant {
  std::string audio_codec;
  const std::string* audio_group_id = nullptr;
  const std::string* text_group_id = nullptr;
  uint64_t audio_bitrate = 0;
};

uint64_t MaxBitrate(const std::list<const MediaPlaylist*> playlists) {
  uint64_t max = 0;
  for (const auto& playlist : playlists) {
    max = std::max(max, playlist->Bitrate());
  }
  return max;
}

std::string GetAudioGroupCodecString(
    const std::list<const MediaPlaylist*>& group) {
  // TODO(vaage): Should be a concatenation of all the codecs in the group.
  return group.front()->codec();
}

std::list<Variant> AudioGroupsToVariants(
    const std::map<std::string, std::list<const MediaPlaylist*>>& groups) {
  std::list<Variant> variants;

  for (const auto& group : groups) {
    Variant variant;
    variant.audio_codec = GetAudioGroupCodecString(group.second);
    variant.audio_group_id = &group.first;
    variant.audio_bitrate = MaxBitrate(group.second);

    variants.push_back(variant);
  }

  // Make sure we return at least one variant so create a null variant if there
  // are no variants.
  if (variants.empty()) {
    variants.emplace_back();
  }

  return variants;
}

const char* GetGroupId(const MediaPlaylist& playlist) {
  const std::string& group_id = playlist.group_id();

  if (!group_id.empty()) {
    return group_id.c_str();
  }

  switch (playlist.stream_type()) {
    case MediaPlaylist::MediaPlaylistStreamType::kAudio:
      return kDefaultAudioGroupId;

    case MediaPlaylist::MediaPlaylistStreamType::kSubtitle:
      return kDefaultSubtitleGroupId;

    default:
      return kUnexpectedGroupId;
  }
}

std::list<Variant> SubtitleGroupsToVariants(
    const std::map<std::string, std::list<const MediaPlaylist*>>& groups) {
  std::list<Variant> variants;

  for (const auto& group : groups) {
    Variant variant;
    variant.text_group_id = &group.first;

    variants.push_back(variant);
  }

  // Make sure we return at least one variant so create a null variant if there
  // are no variants.
  if (variants.empty()) {
    variants.emplace_back();
  }

  return variants;
}

std::list<Variant> BuildVariants(
    const std::map<std::string, std::list<const MediaPlaylist*>>& audio_groups,
    const std::map<std::string, std::list<const MediaPlaylist*>>&
        subtitle_groups) {
  std::list<Variant> audio_variants = AudioGroupsToVariants(audio_groups);
  std::list<Variant> subtitle_variants =
      SubtitleGroupsToVariants(subtitle_groups);

  DCHECK_GE(audio_variants.size(), 1u);
  DCHECK_GE(subtitle_variants.size(), 1u);

  std::list<Variant> merged;

  for (const auto& audio_variant : audio_variants) {
    for (const auto& subtitle_variant : subtitle_variants) {
      Variant variant;
      variant.audio_codec = audio_variant.audio_codec;
      variant.audio_group_id = audio_variant.audio_group_id;
      variant.text_group_id = subtitle_variant.text_group_id;
      variant.audio_bitrate = audio_variant.audio_bitrate;

      merged.push_back(variant);
    }
  }

  DCHECK_GE(merged.size(), 1u);

  return merged;
}

void BuildVideoTag(const MediaPlaylist& playlist,
                   uint64_t max_audio_bitrate,
                   const std::string& audio_codec,
                   const std::string* audio_group_id,
                   const std::string* text_group_id,
                   const std::string& base_url,
                   std::string* out) {
  DCHECK(out);

  const uint64_t bitrate = playlist.Bitrate() + max_audio_bitrate;

  uint32_t width;
  uint32_t height;
  CHECK(playlist.GetDisplayResolution(&width, &height));

  std::string codecs = playlist.codec();
  if (!audio_codec.empty()) {
    base::StringAppendF(&codecs, ",%s", audio_codec.c_str());
  }

  Tag tag("#EXT-X-STREAM-INF", out);

  tag.AddNumber("BANDWIDTH", bitrate);
  tag.AddQuotedString("CODECS", codecs);
  tag.AddNumberPair("RESOLUTION", width, 'x', height);

  if (audio_group_id) {
    tag.AddQuotedString("AUDIO", *audio_group_id);
  }

  if (text_group_id) {
    tag.AddQuotedString("SUBTITLES", *text_group_id);
  }

  base::StringAppendF(out, "\n%s%s\n", base_url.c_str(),
                      playlist.file_name().c_str());
}

// Need to pass in |group_id| as it may have changed to a new default when
// grouped with other playlists.
void BuildMediaTag(const MediaPlaylist& playlist,
                   const std::string& group_id,
                   bool is_default,
                   bool is_autoselect,
                   const std::string& base_url,
                   std::string* out) {
  // Tag attribures should follow the order as defined in
  // https://tools.ietf.org/html/draft-pantos-http-live-streaming-23#section-3.5

  Tag tag("#EXT-X-MEDIA", out);

  // We should only be making media tags for audio and text.
  switch (playlist.stream_type()) {
    case MediaPlaylist::MediaPlaylistStreamType::kAudio:
      tag.AddString("TYPE", "AUDIO");
      break;

    case MediaPlaylist::MediaPlaylistStreamType::kSubtitle:
      tag.AddString("TYPE", "SUBTITLES");
      break;

    default:
      NOTREACHED() << "Cannot build media tag for type "
                   << static_cast<int>(playlist.stream_type());
      break;
  }

  tag.AddQuotedString("URI", base_url + playlist.file_name());
  tag.AddQuotedString("GROUP-ID", group_id);

  const std::string& language = playlist.GetLanguage();
  if (!language.empty()) {
    tag.AddQuotedString("LANGUAGE", language);
  }

  tag.AddQuotedString("NAME", playlist.name());

  if (is_default) {
    tag.AddString("DEFAULT", "YES");
  }

  if (is_autoselect) {
    tag.AddString("AUTOSELECT", "YES");
  }

  const MediaPlaylist::MediaPlaylistStreamType kAudio =
      MediaPlaylist::MediaPlaylistStreamType::kAudio;
  if (playlist.stream_type() == kAudio) {
    std::string channel_string = std::to_string(playlist.GetNumChannels());
    tag.AddQuotedString("CHANNELS", channel_string);
  }

  out->append("\n");
}

void BuildMediaTags(
    const std::map<std::string, std::list<const MediaPlaylist*>>& groups,
    const std::string& default_language,
    const std::string& base_url,
    std::string* out) {
  for (const auto& group : groups) {
    const std::string& group_id = group.first;
    const auto& playlists = group.second;

    // Tracks the language of the playlist in this group.
    // According to HLS spec: https://goo.gl/MiqjNd 4.3.4.1.1. Rendition Groups
    // - A Group MUST NOT have more than one member with a DEFAULT attribute of
    //   YES.
    // - Each EXT-X-MEDIA tag with an AUTOSELECT=YES attribute SHOULD have a
    //   combination of LANGUAGE[RFC5646], ASSOC-LANGUAGE, FORCED, and
    //   CHARACTERISTICS attributes that is distinct from those of other
    //   AUTOSELECT=YES members of its Group.
    // We tag the first rendition encountered with a particular language with
    // 'AUTOSELECT'; it is tagged with 'DEFAULT' too if the language matches
    // |default_language_|.
    std::set<std::string> languages;

    for (const auto& playlist : playlists) {
      bool is_default = false;
      bool is_autoselect = false;

      const std::string language = playlist->GetLanguage();
      if (languages.find(language) == languages.end()) {
        is_default = !language.empty() && language == default_language;
        is_autoselect = true;

        languages.insert(language);
      }

      BuildMediaTag(*playlist, group_id, is_default, is_autoselect, base_url,
                    out);
    }
  }
}
}  // namespace

MasterPlaylist::MasterPlaylist(const std::string& file_name,
                               const std::string& default_language)
    : file_name_(file_name), default_language_(default_language) {}
MasterPlaylist::~MasterPlaylist() {}

void MasterPlaylist::AddMediaPlaylist(MediaPlaylist* media_playlist) {
  DCHECK(media_playlist);
  switch (media_playlist->stream_type()) {
    case MediaPlaylist::MediaPlaylistStreamType::kAudio: {
      std::string group_id = GetGroupId(*media_playlist);
      audio_playlist_groups_[group_id].push_back(media_playlist);
      break;
    }
    case MediaPlaylist::MediaPlaylistStreamType::kVideo: {
      video_playlists_.push_back(media_playlist);
      break;
    }
    case MediaPlaylist::MediaPlaylistStreamType::kSubtitle: {
      std::string group_id = GetGroupId(*media_playlist);
      subtitle_playlist_groups_[group_id].push_back(media_playlist);
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
  std::string subtitle_output;

  // Write out all the audio tags.
  BuildMediaTags(audio_playlist_groups_, default_language_, base_url,
                 &audio_output);

  // Write out all the text tags.
  BuildMediaTags(subtitle_playlist_groups_, default_language_, base_url,
                 &subtitle_output);

  std::list<Variant> variants =
      BuildVariants(audio_playlist_groups_, subtitle_playlist_groups_);

  // Write all the video tags out.
  for (const auto& playlist : video_playlists_) {
    for (const auto& variant : variants) {
      BuildVideoTag(*playlist, variant.audio_bitrate, variant.audio_codec,
                    variant.audio_group_id, variant.text_group_id, base_url,
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

  std::string content = "";
  base::StringAppendF(&content, "#EXTM3U\n%s%s%s%s", version_line.c_str(),
                      audio_output.c_str(), subtitle_output.c_str(),
                      video_output.c_str());

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
