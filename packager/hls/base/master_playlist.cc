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
#include "packager/base/strings/string_util.h"
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

void AppendVersionString(std::string* content) {
  const std::string version = GetPackagerVersion();
  if (version.empty())
    return;
  base::StringAppendF(content, "## Generated with %s version %s\n",
                      GetPackagerProjectUrl().c_str(), version.c_str());
}

// This structure roughly maps to the Variant stream in HLS specification.
// Each variant specifies zero or one audio group and zero or one text group.
struct Variant {
  std::set<std::string> audio_codecs;
  std::set<std::string> text_codecs;
  const std::string* audio_group_id = nullptr;
  const std::string* text_group_id = nullptr;
  // The bitrates should be the sum of audio bitrate and text bitrate.
  // However, given the constraints and assumptions, it makes sense to exclude
  // text bitrate out of the calculation:
  // - Text streams usually have a very small negligible bitrate.
  // - Text does not have constant bitrates. To avoid fluctuation, an arbitrary
  //   value is assigned to the text bitrates in the parser. It does not make
  //   sense to take that text bitrate into account here.
  uint64_t max_audio_bitrate = 0;
  uint64_t avg_audio_bitrate = 0;
};

uint64_t GetMaximumMaxBitrate(const std::list<const MediaPlaylist*> playlists) {
  uint64_t max = 0;
  for (const auto& playlist : playlists) {
    max = std::max(max, playlist->MaxBitrate());
  }
  return max;
}

uint64_t GetMaximumAvgBitrate(const std::list<const MediaPlaylist*> playlists) {
  uint64_t max = 0;
  for (const auto& playlist : playlists) {
    max = std::max(max, playlist->AvgBitrate());
  }
  return max;
}

std::set<std::string> GetGroupCodecString(
    const std::list<const MediaPlaylist*>& group) {
  std::set<std::string> codecs;

  for (const MediaPlaylist* playlist : group) {
    codecs.insert(playlist->codec());
  }

  // To support some older players, we cannot include "wvtt" in the codec
  // string. As per HLS guidelines, "wvtt" is optional. When it is included, it
  // can cause playback errors on some Apple produces. Excluding it allows
  // playback on all Apple products. See
  // https://github.com/shaka-project/shaka-packager/issues/402 for all details.
  auto wvtt = codecs.find("wvtt");
  if (wvtt != codecs.end()) {
    codecs.erase(wvtt);
  }
  // TTML is specified using 'stpp.ttml.im1t'; see section 5.10 of
  // https://developer.apple.com/documentation/http_live_streaming/hls_authoring_specification_for_apple_devices
  auto ttml = codecs.find("ttml");
  if (ttml != codecs.end()) {
    codecs.erase(ttml);
    codecs.insert("stpp.ttml.im1t");
  }

  return codecs;
}

std::list<Variant> AudioGroupsToVariants(
    const std::map<std::string, std::list<const MediaPlaylist*>>& groups) {
  std::list<Variant> variants;

  for (const auto& group : groups) {
    Variant variant;
    variant.audio_group_id = &group.first;
    variant.max_audio_bitrate = GetMaximumMaxBitrate(group.second);
    variant.avg_audio_bitrate = GetMaximumAvgBitrate(group.second);
    variant.audio_codecs = GetGroupCodecString(group.second);

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
    variant.text_codecs = GetGroupCodecString(group.second);

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
      variant.audio_codecs = audio_variant.audio_codecs;
      variant.text_codecs = subtitle_variant.text_codecs;
      variant.audio_group_id = audio_variant.audio_group_id;
      variant.text_group_id = subtitle_variant.text_group_id;
      variant.max_audio_bitrate = audio_variant.max_audio_bitrate;
      variant.avg_audio_bitrate = audio_variant.avg_audio_bitrate;

      merged.push_back(variant);
    }
  }

  DCHECK_GE(merged.size(), 1u);

  return merged;
}

void BuildStreamInfTag(const MediaPlaylist& playlist,
                       const Variant& variant,
                       const std::string& base_url,
                       std::string* out) {
  DCHECK(out);

  std::string tag_name;
  switch (playlist.stream_type()) {
    case MediaPlaylist::MediaPlaylistStreamType::kAudio:
    case MediaPlaylist::MediaPlaylistStreamType::kVideo:
      tag_name = "#EXT-X-STREAM-INF";
      break;
    case MediaPlaylist::MediaPlaylistStreamType::kVideoIFramesOnly:
      tag_name = "#EXT-X-I-FRAME-STREAM-INF";
      break;
    default:
      NOTREACHED() << "Cannot build STREAM-INFO tag for type "
                   << static_cast<int>(playlist.stream_type());
      break;
  }
  Tag tag(tag_name, out);

  tag.AddNumber("BANDWIDTH", playlist.MaxBitrate() + variant.max_audio_bitrate);
  tag.AddNumber("AVERAGE-BANDWIDTH",
                playlist.AvgBitrate() + variant.avg_audio_bitrate);

  std::vector<std::string> all_codecs;
  all_codecs.push_back(playlist.codec());
  all_codecs.insert(all_codecs.end(), variant.audio_codecs.begin(),
                    variant.audio_codecs.end());
  all_codecs.insert(all_codecs.end(), variant.text_codecs.begin(),
                    variant.text_codecs.end());
  tag.AddQuotedString("CODECS", base::JoinString(all_codecs, ","));

  uint32_t width;
  uint32_t height;
  if (playlist.GetDisplayResolution(&width, &height)) {
    tag.AddNumberPair("RESOLUTION", width, 'x', height);

    // Right now the frame-rate returned may not be accurate in some scenarios.
    // TODO(kqyang): Fix frame-rate computation.
    const bool is_iframe_playlist =
        playlist.stream_type() ==
        MediaPlaylist::MediaPlaylistStreamType::kVideoIFramesOnly;
    if (!is_iframe_playlist) {
      const double frame_rate = playlist.GetFrameRate();
      if (frame_rate > 0)
        tag.AddFloat("FRAME-RATE", frame_rate);
    }

    const std::string video_range = playlist.GetVideoRange();
    if (!video_range.empty())
      tag.AddString("VIDEO-RANGE", video_range);
  }

  if (variant.audio_group_id) {
    tag.AddQuotedString("AUDIO", *variant.audio_group_id);
  }

  if (variant.text_group_id) {
    tag.AddQuotedString("SUBTITLES", *variant.text_group_id);
  }

  // Since CEA captions in Shaka Packager are only an input format, but not
  // supported as output, the HLS output should always indicate that there are
  // no captions.  Explicitly signaling a lack of captions in HLS keeps Safari
  // from assuming captions and showing a text track that doesn't exist.
  // https://github.com/shaka-project/shaka-packager/issues/922#issuecomment-804304019
  tag.AddString("CLOSED-CAPTIONS", "NONE");

  if (playlist.stream_type() ==
      MediaPlaylist::MediaPlaylistStreamType::kVideoIFramesOnly) {
    tag.AddQuotedString("URI", base_url + playlist.file_name());
    out->append("\n");
  } else {
    base::StringAppendF(out, "\n%s%s\n", base_url.c_str(),
                        playlist.file_name().c_str());
  }
}

// Need to pass in |group_id| as it may have changed to a new default when
// grouped with other playlists.
void BuildMediaTag(const MediaPlaylist& playlist,
                   const std::string& group_id,
                   bool is_default,
                   bool is_autoselect,
                   const std::string& base_url,
                   std::string* out) {
  // Tag attributes should follow the order as defined in
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

  const std::string& language = playlist.language();
  if (!language.empty()) {
    tag.AddQuotedString("LANGUAGE", language);
  }

  tag.AddQuotedString("NAME", playlist.name());

  if (is_default) {
    tag.AddString("DEFAULT", "YES");
  } else {
     tag.AddString("DEFAULT", "NO");
  }

  if (is_autoselect) {
    tag.AddString("AUTOSELECT", "YES");
  }

  const std::vector<std::string>& characteristics = playlist.characteristics();
  if (!characteristics.empty()) {
    tag.AddQuotedString("CHARACTERISTICS",
                        base::JoinString(characteristics, ","));
  }

  const MediaPlaylist::MediaPlaylistStreamType kAudio =
      MediaPlaylist::MediaPlaylistStreamType::kAudio;
  if (playlist.stream_type() == kAudio) {
    if (playlist.GetEC3JocComplexity() != 0) {
      // HLS Authoring Specification for Apple Devices Appendices documents how
      // to handle Dolby Digital Plus JOC content.
      // https://developer.apple.com/documentation/http_live_streaming/hls_authoring_specification_for_apple_devices/hls_authoring_specification_for_apple_devices_appendices
      std::string channel_string =
        std::to_string(playlist.GetEC3JocComplexity()) + "/JOC";
      tag.AddQuotedString("CHANNELS", channel_string);
    } else if (playlist.GetAC4ImsFlag() || playlist.GetAC4CbiFlag()) {
      // Dolby has qualified using IMSA to present AC4 immersive audio (IMS and
      // CBI without object-based audio) for Dolby internal use only. IMSA is
      // not included in any publicly-available specifications as of June, 2020.
      std::string channel_string =
        std::to_string(playlist.GetNumChannels()) + "/IMSA";
      tag.AddQuotedString("CHANNELS", channel_string);
    } else {
      // According to HLS spec:
      // https://tools.ietf.org/html/draft-pantos-hls-rfc8216bis 4.4.6.1.
      // CHANNELS is a quoted-string that specifies an ordered,
      // slash-separated ("/") list of parameters. The first parameter is a
      // count of audio channels, and the second parameter identifies the
      // encoding of object-based audio used by the Rendition.
      std::string channel_string = std::to_string(playlist.GetNumChannels());
      tag.AddQuotedString("CHANNELS", channel_string);
    }
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

      if (playlist->is_dvs()) {
        // According to HLS Authoring Specification for Apple Devices
        // https://developer.apple.com/documentation/http_live_streaming/hls_authoring_specification_for_apple_devices#overview
        // section 2.13 If you provide DVS, the AUTOSELECT attribute MUST have
        //              a value of "YES".
        is_autoselect = true;
      } else {
        const std::string language = playlist->language();
        if (languages.find(language) == languages.end()) {
          is_default = !language.empty() && language == default_language;
          is_autoselect = true;

          languages.insert(language);
        }
      }

      BuildMediaTag(*playlist, group_id, is_default, is_autoselect, base_url,
                    out);
    }
  }
}

void AppendPlaylists(const std::string& default_audio_language,
                     const std::string& default_text_language,
                     const std::string& base_url,
                     const std::list<MediaPlaylist*>& playlists,
                     std::string* content) {
  std::map<std::string, std::list<const MediaPlaylist*>> audio_playlist_groups;
  std::map<std::string, std::list<const MediaPlaylist*>>
      subtitle_playlist_groups;
  std::list<const MediaPlaylist*> video_playlists;
  std::list<const MediaPlaylist*> iframe_playlists;
  for (const MediaPlaylist* playlist : playlists) {
    switch (playlist->stream_type()) {
      case MediaPlaylist::MediaPlaylistStreamType::kAudio:
        audio_playlist_groups[GetGroupId(*playlist)].push_back(playlist);
        break;
      case MediaPlaylist::MediaPlaylistStreamType::kVideo:
        video_playlists.push_back(playlist);
        break;
      case MediaPlaylist::MediaPlaylistStreamType::kVideoIFramesOnly:
        iframe_playlists.push_back(playlist);
        break;
      case MediaPlaylist::MediaPlaylistStreamType::kSubtitle:
        subtitle_playlist_groups[GetGroupId(*playlist)].push_back(playlist);
        break;
      default:
        NOTIMPLEMENTED() << static_cast<int>(playlist->stream_type())
                         << " not handled.";
    }
  }

  if (!audio_playlist_groups.empty()) {
    content->append("\n");
    BuildMediaTags(audio_playlist_groups, default_audio_language, base_url,
                   content);
  }

  if (!subtitle_playlist_groups.empty()) {
    content->append("\n");
    BuildMediaTags(subtitle_playlist_groups, default_text_language, base_url,
                   content);
  }

  std::list<Variant> variants =
      BuildVariants(audio_playlist_groups, subtitle_playlist_groups);
  for (const auto& variant : variants) {
    if (video_playlists.empty())
      break;
    content->append("\n");
    for (const auto& playlist : video_playlists) {
      BuildStreamInfTag(*playlist, variant, base_url, content);
    }
  }

  if (!iframe_playlists.empty()) {
    content->append("\n");
    for (const auto& playlist : iframe_playlists) {
      // I-Frame playlists do not have variant. Just use the default.
      BuildStreamInfTag(*playlist, Variant(), base_url, content);
    }
  }

  // Generate audio-only master playlist when there are no videos and subtitles.
  if (!audio_playlist_groups.empty() && video_playlists.empty() &&
      subtitle_playlist_groups.empty()) {
    content->append("\n");
    for (const auto& playlist_group : audio_playlist_groups) {
      Variant variant;
      // Populate |audio_group_id|, which will be propagated to "AUDIO" field.
      // Leaving other fields, e.g. xxx_audio_bitrate in |Variant|, as
      // null/empty/zero intentionally as the information is already available
      // in audio |playlist|.
      variant.audio_group_id = &playlist_group.first;
      for (const auto& playlist : playlist_group.second) {
        BuildStreamInfTag(*playlist, variant, base_url, content);
      }
    }
  }
}

}  // namespace

MasterPlaylist::MasterPlaylist(const std::string& file_name,
                               const std::string& default_audio_language,
                               const std::string& default_text_language,
                               bool is_independent_segments)
    : file_name_(file_name),
      default_audio_language_(default_audio_language),
      default_text_language_(default_text_language),
      is_independent_segments_(is_independent_segments) {}

MasterPlaylist::~MasterPlaylist() {}

bool MasterPlaylist::WriteMasterPlaylist(
    const std::string& base_url,
    const std::string& output_dir,
    const std::list<MediaPlaylist*>& playlists) {
  std::string content = "#EXTM3U\n";
  AppendVersionString(&content);

  if (is_independent_segments_) {
    content.append("\n#EXT-X-INDEPENDENT-SEGMENTS\n");
  }
  AppendPlaylists(default_audio_language_, default_text_language_, base_url,
                  playlists, &content);

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
