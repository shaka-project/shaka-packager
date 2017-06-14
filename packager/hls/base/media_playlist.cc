// Copyright 2016 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/hls/base/media_playlist.h"

#include <algorithm>
#include <cmath>
#include <memory>

#include "packager/base/logging.h"
#include "packager/base/strings/stringprintf.h"
#include "packager/media/base/language_utils.h"
#include "packager/media/file/file.h"
#include "packager/version/version.h"

namespace shaka {
namespace hls {

namespace {
uint32_t GetTimeScale(const MediaInfo& media_info) {
  if (media_info.has_reference_time_scale())
    return media_info.reference_time_scale();

  if (media_info.has_video_info())
    return media_info.video_info().time_scale();

  if (media_info.has_audio_info())
    return media_info.audio_info().time_scale();
  return 0u;
}

std::string CreatePlaylistHeader(
    const std::string& init_segment_name,
    uint32_t target_duration,
    MediaPlaylist::MediaPlaylistType type) {
  const std::string version = GetPackagerVersion();
  std::string version_line;
  if (!version.empty()) {
    version_line =
        base::StringPrintf("## Generated with %s version %s\n",
                           GetPackagerProjectUrl().c_str(), version.c_str());
  }

  // 6 is required for EXT-X-MAP without EXT-X-I-FRAMES-ONLY.
  std::string header = base::StringPrintf(
      "#EXTM3U\n"
      "#EXT-X-VERSION:6\n"
      "%s"
      "#EXT-X-TARGETDURATION:%d\n",
      version_line.c_str(), target_duration);

  if (type == MediaPlaylist::MediaPlaylistType::kVod) {
    header += "#EXT-X-PLAYLIST-TYPE:VOD\n";
  }

  // Put EXT-X-MAP at the end since the rest of the playlist is about the
  // segment and key info.
  if (!init_segment_name.empty()) {
    header += "#EXT-X-MAP:URI=\"" + init_segment_name + "\"\n";
  }

  return header;
}

class SegmentInfoEntry : public HlsEntry {
 public:
  SegmentInfoEntry(const std::string& file_name, double duration);
  ~SegmentInfoEntry() override;

  std::string ToString() override;

 private:
  const std::string file_name_;
  const double duration_;

  DISALLOW_COPY_AND_ASSIGN(SegmentInfoEntry);
};

SegmentInfoEntry::SegmentInfoEntry(const std::string& file_name,
                                   double duration)
    : HlsEntry(HlsEntry::EntryType::kExtInf),
      file_name_(file_name),
      duration_(duration) {}
SegmentInfoEntry::~SegmentInfoEntry() {}

std::string SegmentInfoEntry::ToString() {
  return base::StringPrintf("#EXTINF:%.3f,\n%s\n", duration_,
                            file_name_.c_str());
}

class EncryptionInfoEntry : public HlsEntry {
 public:
  EncryptionInfoEntry(MediaPlaylist::EncryptionMethod method,
                      const std::string& url,
                      const std::string& key_id,
                      const std::string& iv,
                      const std::string& key_format,
                      const std::string& key_format_versions);

  ~EncryptionInfoEntry() override;

  std::string ToString() override;

 private:
  const MediaPlaylist::EncryptionMethod method_;
  const std::string url_;
  const std::string key_id_;
  const std::string iv_;
  const std::string key_format_;
  const std::string key_format_versions_;

  DISALLOW_COPY_AND_ASSIGN(EncryptionInfoEntry);
};

EncryptionInfoEntry::EncryptionInfoEntry(MediaPlaylist::EncryptionMethod method,
                                         const std::string& url,
                                         const std::string& key_id,
                                         const std::string& iv,
                                         const std::string& key_format,
                                         const std::string& key_format_versions)
    : HlsEntry(HlsEntry::EntryType::kExtKey),
      method_(method),
      url_(url),
      key_id_(key_id),
      iv_(iv),
      key_format_(key_format),
      key_format_versions_(key_format_versions) {}

EncryptionInfoEntry::~EncryptionInfoEntry() {}

std::string EncryptionInfoEntry::ToString() {
  std::string method_attribute;
  if (method_ == MediaPlaylist::EncryptionMethod::kSampleAes) {
    method_attribute = "METHOD=SAMPLE-AES";
  } else if (method_ == MediaPlaylist::EncryptionMethod::kAes128) {
    method_attribute = "METHOD=AES-128";
  } else if (method_ == MediaPlaylist::EncryptionMethod::kSampleAesCenc) {
    method_attribute = "METHOD=SAMPLE-AES-CENC";
  } else {
    DCHECK(method_ == MediaPlaylist::EncryptionMethod::kNone);
    method_attribute = "METHOD=NONE";
  }
  std::string ext_key = "#EXT-X-KEY:" + method_attribute + ",URI=\"" + url_ +
                        "\"";
  if (!key_id_.empty()) {
    ext_key += ",KEYID=" + key_id_;
  }
  if (!iv_.empty()) {
    ext_key += ",IV=" + iv_;
  }
  if (!key_format_versions_.empty()) {
    ext_key += ",KEYFORMATVERSIONS=\"" + key_format_versions_ + "\"";
  }
  if (key_format_.empty())
    return ext_key + "\n";

  return ext_key + ",KEYFORMAT=\"" + key_format_ + "\"\n";
}

}  // namespace

HlsEntry::HlsEntry(HlsEntry::EntryType type) : type_(type) {}
HlsEntry::~HlsEntry() {}

MediaPlaylist::MediaPlaylist(MediaPlaylistType type,
                             const std::string& file_name,
                             const std::string& name,
                             const std::string& group_id)
    : file_name_(file_name), name_(name), group_id_(group_id), type_(type) {
  LOG_IF(WARNING, type != MediaPlaylistType::kVod)
      << "Non VOD Media Playlist is not supported.";
}

MediaPlaylist::~MediaPlaylist() {}

void MediaPlaylist::SetStreamTypeForTesting(
    MediaPlaylistStreamType stream_type) {
  stream_type_ = stream_type;
}

void MediaPlaylist::SetCodecForTesting(const std::string& codec) {
  codec_ = codec;
}

bool MediaPlaylist::SetMediaInfo(const MediaInfo& media_info) {
  const uint32_t time_scale = GetTimeScale(media_info);
  if (time_scale == 0) {
    LOG(ERROR) << "MediaInfo does not contain a valid timescale.";
    return false;
  }

  if (media_info.has_video_info()) {
    stream_type_ = MediaPlaylistStreamType::kPlayListVideo;
    codec_ = media_info.video_info().codec();
  } else if (media_info.has_audio_info()) {
    stream_type_ = MediaPlaylistStreamType::kPlayListAudio;
    codec_ = media_info.audio_info().codec();
  } else {
    NOTIMPLEMENTED();
    return false;
  }

  time_scale_ = time_scale;
  media_info_ = media_info;
  return true;
}

void MediaPlaylist::AddSegment(const std::string& file_name,
                               uint64_t duration,
                               uint64_t size) {
  if (time_scale_ == 0) {
    LOG(WARNING) << "Timescale is not set and the duration for " << duration
                 << " cannot be calculated. The output will be wrong.";

    entries_.emplace_back(new SegmentInfoEntry(file_name, 0.0));
    return;
  }

  const double segment_duration_seconds =
      static_cast<double>(duration) / time_scale_;
  if (segment_duration_seconds > longest_segment_duration_)
    longest_segment_duration_ = segment_duration_seconds;

  const int kBitsInByte = 8;
  const uint64_t bitrate = kBitsInByte * size / segment_duration_seconds;
  max_bitrate_ = std::max(max_bitrate_, bitrate);
  entries_.emplace_back(
      new SegmentInfoEntry(file_name, segment_duration_seconds));
}

// TODO(rkuroiwa): This works for single key format but won't work for multiple
// key formats (e.g. different DRM systems).
// Candidate algorithm:
// Assume entries_ is std::list (static_assert below).
// Create a map from key_format to EncryptionInfoEntry (iterator actually).
// Iterate over entries_ until it hits SegmentInfoEntry. While iterating over
// entries_ if there are multiple EncryptionInfoEntry with the same key_format,
// erase the older ones using the iterator.
// Note that when erasing std::list iterators, only the deleted iterators are
// invalidated.
void MediaPlaylist::RemoveOldestSegment() {
  static_assert(std::is_same<decltype(entries_),
                             std::list<std::unique_ptr<HlsEntry>>>::value,
                "This algorithm assumes std::list.");
  if (entries_.empty())
    return;
  if (entries_.front()->type() == HlsEntry::EntryType::kExtInf) {
    entries_.pop_front();
    return;
  }

  // Make sure that the first EXT-X-KEY entry doesn't get popped out until the
  // next EXT-X-KEY entry because the first EXT-X-KEY applies to all the
  // segments following until the next one.

  if (entries_.size() == 1) {
    // More segments might get added, leave the entry in.
    return;
  }

  if (entries_.size() == 2) {
    auto entries_itr = entries_.begin();
    ++entries_itr;
    if ((*entries_itr)->type() == HlsEntry::EntryType::kExtKey) {
      entries_.pop_front();
    } else {
      entries_.erase(entries_itr);
    }
    return;
  }

  auto entries_itr = entries_.begin();
  ++entries_itr;
  if ((*entries_itr)->type() == HlsEntry::EntryType::kExtInf) {
    DCHECK((*entries_itr)->type() == HlsEntry::EntryType::kExtInf);
    entries_.erase(entries_itr);
    return;
  }

  ++entries_itr;
  // This assumes that there is a segment between 2 EXT-X-KEY entries.
  // Which should be the case due to logic in AddEncryptionInfo().
  DCHECK((*entries_itr)->type() == HlsEntry::EntryType::kExtInf);
  entries_.erase(entries_itr);
  entries_.pop_front();
}

void MediaPlaylist::AddEncryptionInfo(MediaPlaylist::EncryptionMethod method,
                                      const std::string& url,
                                      const std::string& key_id,
                                      const std::string& iv,
                                      const std::string& key_format,
                                      const std::string& key_format_versions) {
  entries_.emplace_back(new EncryptionInfoEntry(
      method, url, key_id, iv, key_format, key_format_versions));
}

bool MediaPlaylist::WriteToFile(media::File* file) {
  if (!target_duration_set_) {
    SetTargetDuration(ceil(GetLongestSegmentDuration()));
  }

  std::string header = CreatePlaylistHeader(media_info_.init_segment_name(),
                                            target_duration_, type_);

  std::string body;
  if (!entries_.empty()) {
    const bool first_is_ext_key =
        entries_.front()->type() == HlsEntry::EntryType::kExtKey;
    bool inserted_discontinuity_tag = false;
    for (const auto& entry : entries_) {
      if (!first_is_ext_key && !inserted_discontinuity_tag &&
          entry->type() == HlsEntry::EntryType::kExtKey) {
        body.append("#EXT-X-DISCONTINUITY\n");
        inserted_discontinuity_tag = true;
      }
      body.append(entry->ToString());
    }
  }

  std::string content = header + body;

  if (type_ == MediaPlaylistType::kVod) {
    content += "#EXT-X-ENDLIST\n";
  }

  int64_t bytes_written = file->Write(content.data(), content.size());
  if (bytes_written < 0) {
    LOG(ERROR) << "Error while writing playlist to file.";
    return false;
  }

  // TODO(rkuroiwa): There are at least 2 while (remaining_bytes > 0) logic in
  // this library to handle partial writes by File. Dedup them and use it here
  // has well.
  if (static_cast<size_t>(bytes_written) < content.size()) {
    LOG(ERROR) << "Failed to write the whole playlist. Wrote " << bytes_written
               << " but the playlist is " << content.size() << " bytes.";
    return false;
  }

  return true;
}

uint64_t MediaPlaylist::Bitrate() const {
  if (media_info_.has_bandwidth())
    return media_info_.bandwidth();
  return max_bitrate_;
}

double MediaPlaylist::GetLongestSegmentDuration() const {
  return longest_segment_duration_;
}

bool MediaPlaylist::SetTargetDuration(uint32_t target_duration) {
  if (target_duration_set_) {
    LOG(WARNING) << "Cannot set target duration to " << target_duration
                 << ". Target duration already set to " << target_duration_;
    return false;
  }
  target_duration_ = target_duration;
  target_duration_set_ = true;
  return true;
}

// Duplicated from MpdUtils because:
// 1. MpdUtils header depends on libxml header, which is not in the deps here
// 2. GetLanguage depends on MediaInfo from packager/mpd/
// 3. Moving GetLanguage to LanguageUtils would create a a media => mpd dep.
// TODO: fix this dependency situation and factor this out to a common location
std::string MediaPlaylist::GetLanguage() const {
  std::string lang;
  if (media_info_.has_audio_info()) {
    lang = media_info_.audio_info().language();
  } else if (media_info_.has_text_info()) {
    lang = media_info_.text_info().language();
  }
  return LanguageToShortestForm(lang);
}

bool MediaPlaylist::GetResolution(uint32_t* width, uint32_t* height) const {
  DCHECK(width);
  DCHECK(height);
  if (media_info_.has_video_info()) {
    *width = media_info_.video_info().width();
    *height = media_info_.video_info().height();
    return true;
  }
  return false;
}

}  // namespace hls
}  // namespace shaka
