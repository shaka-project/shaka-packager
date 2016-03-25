// Copyright 2016 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/hls/base/media_playlist.h"

#include <algorithm>
#include <cmath>

#include "packager/base/logging.h"
#include "packager/base/strings/stringprintf.h"
#include "packager/media/file/file.h"

namespace edash_packager {
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
  return base::StringPrintf("#EXTINF:%.3f\n%s\n", duration_,
                            file_name_.c_str());
}

class EncryptionInfoEntry : public HlsEntry {
 public:
  EncryptionInfoEntry(MediaPlaylist::EncryptionMethod method,
                      const std::string& url,
                      const std::string& iv,
                      const std::string& key_format,
                      const std::string& key_format_versions);

  ~EncryptionInfoEntry() override;

  std::string ToString() override;

 private:
  const MediaPlaylist::EncryptionMethod method_;
  const std::string url_;
  const std::string iv_;
  const std::string key_format_;
  const std::string key_format_versions_;

  DISALLOW_COPY_AND_ASSIGN(EncryptionInfoEntry);
};

EncryptionInfoEntry::EncryptionInfoEntry(MediaPlaylist::EncryptionMethod method,
                                         const std::string& url,
                                         const std::string& iv,
                                         const std::string& key_format,
                                         const std::string& key_format_versions)
    : HlsEntry(HlsEntry::EntryType::kExtKey),
      method_(method),
      url_(url),
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
  } else {
    DCHECK(method_ == MediaPlaylist::EncryptionMethod::kNone);
    method_attribute = "METHOD=NONE";
  }
  std::string ext_key = "#EXT-X-KEY:" + method_attribute + ",URI=\"" + url_ +
                        "\"";
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

MediaPlaylist::MediaPlaylist(const std::string& file_name,
                             const std::string& name,
                             const std::string& group_id)
    : file_name_(file_name), name_(name), group_id_(group_id) {}
MediaPlaylist::~MediaPlaylist() {}

void MediaPlaylist::SetTypeForTesting(MediaPlaylistType type) {
  type_ = type;
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
    type_ = MediaPlaylistType::kPlayListVideo;
    codec_ = media_info.video_info().codec();
  } else if (media_info.has_audio_info()) {
    type_ = MediaPlaylistType::kPlayListAudio;
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

    scoped_ptr<SegmentInfoEntry> info(new SegmentInfoEntry(file_name, 0.0));
    entries_.push_back(info.Pass());
    return;
  }

  const double segment_duration = static_cast<double>(duration) / time_scale_;
  if (segment_duration > longest_segment_duration_)
    longest_segment_duration_ = segment_duration;

  total_duration_in_seconds_ += segment_duration;
  total_segments_size_ += size;
  ++total_num_segments_;

  scoped_ptr<SegmentInfoEntry> info(
      new SegmentInfoEntry(file_name, segment_duration));
  entries_.push_back(info.Pass());
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
  static_assert(
      std::is_same<decltype(entries_), std::list<scoped_ptr<HlsEntry>>>::value,
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
                                      const std::string& iv,
                                      const std::string& key_format,
                                      const std::string& key_format_versions) {
  if (!entries_.empty()) {
    // No reason to have two consecutive EXT-X-KEY entries. Remove the previous
    // one.
    if (entries_.back()->type() == HlsEntry::EntryType::kExtKey)
      entries_.pop_back();
  }
  scoped_ptr<EncryptionInfoEntry> info(new EncryptionInfoEntry(
      method, url, iv, key_format, key_format_versions));
  entries_.push_back(info.Pass());
}

bool MediaPlaylist::WriteToFile(media::File* file) {
  if (!target_duration_set_) {
    SetTargetDuration(ceil(GetLongestSegmentDuration()));
  }

  std::string header = base::StringPrintf("#EXTM3U\n"
                                          "#EXT-X-TARGETDURATION:%d\n",
                                          target_duration_);
  std::string body;
  for (const auto& entry : entries_) {
    body.append(entry->ToString());
  }

  std::string content = header + body;
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
  if (total_duration_in_seconds_ == 0.0)
    return 0;
  if (total_segments_size_ == 0)
    return 0;
  return total_segments_size_ / total_duration_in_seconds_;
}

double MediaPlaylist::GetLongestSegmentDuration() {
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

}  // namespace hls
}  // namespace edash_packager
