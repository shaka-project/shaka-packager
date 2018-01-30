// Copyright 2016 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_HLS_BASE_MEDIA_PLAYLIST_H_
#define PACKAGER_HLS_BASE_MEDIA_PLAYLIST_H_

#include <list>
#include <memory>
#include <string>

#include "packager/base/macros.h"
#include "packager/hls/public/hls_params.h"
#include "packager/mpd/base/media_info.pb.h"

namespace shaka {

class File;

namespace hls {

class HlsEntry {
 public:
  enum class EntryType {
    kExtInf,
    kExtKey,
    kExtDiscontinuity,
    kExtPlacementOpportunity,
  };
  virtual ~HlsEntry();

  EntryType type() const { return type_; }
  virtual std::string ToString() = 0;

 protected:
  explicit HlsEntry(EntryType type);

 private:
  EntryType type_;
};

/// Methods are virtual for mocking.
class MediaPlaylist {
 public:
  enum class MediaPlaylistStreamType {
    kUnknown,
    kAudio,
    kVideo,
    kVideoIFramesOnly,
    kSubtitle,
  };
  enum class EncryptionMethod {
    kNone,           // No encryption, i.e. clear.
    kAes128,         // Completely encrypted using AES-CBC.
    kSampleAes,      // Encrypted using SAMPLE-AES method.
    kSampleAesCenc,  // 'cenc' encrypted content.
  };

  /// @param playlist_type is the type of this media playlist.
  /// @param time_shift_buffer_depth determines the duration of the time
  ///        shifting buffer, only for live HLS.
  /// @param file_name is the file name of this media playlist.
  /// @param name is the name of this playlist. In other words this is the
  ///        value of the NAME attribute for EXT-X-MEDIA. This is not
  ///        necessarily the same as @a file_name.
  /// @param group_id is the group ID for this playlist. This is the value of
  ///        GROUP-ID attribute for EXT-X-MEDIA.
  MediaPlaylist(HlsPlaylistType playlist_type,
                double time_shift_buffer_depth,
                const std::string& file_name,
                const std::string& name,
                const std::string& group_id);
  virtual ~MediaPlaylist();

  const std::string& file_name() const { return file_name_; }
  const std::string& name() const { return name_; }
  const std::string& group_id() const { return group_id_; }
  MediaPlaylistStreamType stream_type() const { return stream_type_; }
  const std::string& codec() const { return codec_; }

  /// For testing only.
  void SetStreamTypeForTesting(MediaPlaylistStreamType stream_type);

  /// For testing only.
  void SetCodecForTesting(const std::string& codec);

  /// This must succeed before calling any other public methods.
  /// @param media_info is the info of the segments that are going to be added
  ///        to this playlist.
  /// @return true on success, false otherwise.
  virtual bool SetMediaInfo(const MediaInfo& media_info);

  /// Segments must be added in order.
  /// @param file_name is the file name of the segment.
  /// @param start_time is in terms of the timescale of the media.
  /// @param duration is in terms of the timescale of the media.
  /// @param start_byte_offset is the offset of where the subsegment starts.
  ///        This must be 0 if the whole segment is a subsegment.
  /// @param size is size in bytes.
  virtual void AddSegment(const std::string& file_name,
                          uint64_t start_time,
                          uint64_t duration,
                          uint64_t start_byte_offset,
                          uint64_t size);

  /// Keyframes must be added in order. It is also called before the containing
  /// segment being called.
  /// @param timestamp is the timestamp of the key frame in timescale of the
  ///        media.
  /// @param start_byte_offset is the offset of where the key frame starts.
  /// @param size is size in bytes.
  virtual void AddKeyFrame(uint64_t timestamp,
                           uint64_t start_byte_offset,
                           uint64_t size);

  /// All segments added after calling this method must be decryptable with
  /// the key that can be fetched from |url|, until calling this again.
  /// @param method is the encryption method.
  /// @param url specifies where the key is i.e. the value of the URI attribute.
  /// #param key_id is the default key ID for the encrypted segements.
  /// @param iv is the initialization vector in human readable format, i.e. the
  ///        value for IV attribute. This may be empty.
  /// @param key_format is the key format, i.e. the KEYFORMAT value. This may be
  ///        empty.
  /// @param key_format_versions is the KEYFORMATVERIONS value. This may be
  ///        empty.
  virtual void AddEncryptionInfo(EncryptionMethod method,
                                 const std::string& url,
                                 const std::string& key_id,
                                 const std::string& iv,
                                 const std::string& key_format,
                                 const std::string& key_format_versions);

  /// Add #EXT-X-PLACEMENT-OPPORTUNITY for mid-roll ads. See
  /// https://support.google.com/dfp_premium/answer/7295798?hl=en.
  virtual void AddPlacementOpportunity();

  /// Write the playlist to |file_path|.
  /// This does not close the file.
  /// If target duration is not set expliticly, this will try to find the target
  /// duration. Note that target duration cannot be changed. So calling this
  /// without explicitly setting the target duration and before adding any
  /// segments will end up setting the target duration to 0 and will always
  /// generate an invalid playlist.
  /// @param file_path is the output file path accepted by the File
  ///        implementation.
  /// @return true on success, false otherwise.
  virtual bool WriteToFile(const std::string& file_path);

  /// If bitrate is specified in MediaInfo then it will use that value.
  /// Otherwise, returns the max bitrate.
  /// @return the bitrate (in bits per second) of this MediaPlaylist.
  virtual uint64_t Bitrate() const;

  /// @return the longest segment’s duration. This will return 0 if no
  ///         segments have been added.
  virtual double GetLongestSegmentDuration() const;

  /// Set the target duration of this MediaPlaylist.
  /// In other words this is the value for EXT-X-TARGETDURATION.
  /// If this is not called before calling Write(), it will estimate the best
  /// target duration.
  /// The spec does not allow changing EXT-X-TARGETDURATION. However, this class
  /// has no control over the input source.
  /// @param target_duration is the target duration for this playlist.
  virtual void SetTargetDuration(uint32_t target_duration);

  /// @return the language of the media, as an ISO language tag in its shortest
  ///         form.  May be an empty string for video.
  virtual std::string GetLanguage() const;

  /// @return number of channels for audio. 0 is returned for video.
  virtual int GetNumChannels() const;

  /// @return true if |width| and |height| have been set with a valid
  ///         resolution values.
  virtual bool GetDisplayResolution(uint32_t* width, uint32_t* height) const;

 private:
  // Add a SegmentInfoEntry (#EXTINF).
  void AddSegmentInfoEntry(const std::string& segment_file_name,
                           uint64_t start_time,
                           uint64_t duration,
                           uint64_t start_byte_offset,
                           uint64_t size);
  // Remove elements from |entries_| for live profile. Increments
  // |sequence_number_| by the number of segments removed.
  void SlideWindow();

  const HlsPlaylistType playlist_type_;
  const double time_shift_buffer_depth_;
  // Mainly for MasterPlaylist to use these values.
  const std::string file_name_;
  const std::string name_;
  const std::string group_id_;
  MediaInfo media_info_;
  MediaPlaylistStreamType stream_type_ = MediaPlaylistStreamType::kUnknown;
  // Whether to use byte range for SegmentInfoEntry.
  bool use_byte_range_ = false;
  std::string codec_;
  int media_sequence_number_ = 0;
  bool inserted_discontinuity_tag_ = false;
  int discontinuity_sequence_number_ = 0;

  double longest_segment_duration_ = 0.0;
  uint32_t time_scale_ = 0;

  uint64_t max_bitrate_ = 0;

  // Cache the previous calls AddSegment() end offset. This is used to construct
  // SegmentInfoEntry.
  uint64_t previous_segment_end_offset_ = 0;

  // See SetTargetDuration() comments.
  bool target_duration_set_ = false;
  uint32_t target_duration_ = 0;

  std::list<std::unique_ptr<HlsEntry>> entries_;

  // Used by kVideoIFrameOnly playlists to track the i-frames (key frames).
  struct KeyFrameInfo {
    uint64_t timestamp;
    uint64_t start_byte_offset;
    uint64_t size;
    uint64_t duration;
    std::string segment_file_name;
  };
  std::list<KeyFrameInfo> key_frames_;

  DISALLOW_COPY_AND_ASSIGN(MediaPlaylist);
};

}  // namespace hls
}  // namespace shaka

#endif  // PACKAGER_HLS_BASE_MEDIA_PLAYLIST_H_
