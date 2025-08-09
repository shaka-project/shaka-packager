// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PACKAGER_MEDIA_FORMATS_WEBM_WEBM_CLUSTER_PARSER_H_
#define PACKAGER_MEDIA_FORMATS_WEBM_WEBM_CLUSTER_PARSER_H_

#include <cstdint>
#include <deque>
#include <map>
#include <memory>
#include <set>
#include <string>

#include <packager/macros/classes.h>
#include <packager/media/base/decryptor_source.h>
#include <packager/media/base/media_parser.h>
#include <packager/media/base/media_sample.h>
#include <packager/media/formats/webm/webm_parser.h>
#include <packager/media/formats/webm/webm_tracks_parser.h>

namespace shaka {
namespace media {

class WebMClusterParser : public WebMParserClient {
 public:
  /// Numbers chosen to estimate the duration of a buffer if none is set and
  /// there is not enough information to get a better estimate.
  enum {
    /// Common 1k samples @44.1kHz
    kDefaultAudioBufferDurationInMs = 23,

    /// Chosen to represent 16fps duration, which will prevent MSE stalls in
    /// videos with frame-rates as low as 8fps.
    kDefaultVideoBufferDurationInMs = 63
  };

 private:
  // Helper class that manages per-track state.
  class Track {
   public:
    Track(int track_num,
          bool is_video,
          int64_t default_duration,
          const MediaParser::NewMediaSampleCB& new_sample_cb);
    ~Track();

    int track_num() const { return track_num_; }

    // If |last_added_buffer_missing_duration_| is set, updates its duration
    // relative to |buffer|'s timestamp, and emits it and unsets
    // |last_added_buffer_missing_duration_|. Otherwise, if |buffer| is missing
    // duration, saves |buffer| into |last_added_buffer_missing_duration_|.
    bool EmitBuffer(const std::shared_ptr<MediaSample>& buffer);

    // If |last_added_buffer_missing_duration_| is set, estimate the duration
    // for this buffer using helper function GetDurationEstimate() then emits it
    // and unsets |last_added_buffer_missing_duration_| (This method helps
    // stream parser emit all buffers in a media segment).
    bool ApplyDurationEstimateIfNeeded();

    // Clears all buffer state, including any possibly held-aside buffer that
    // was missing duration.
    void Reset();

   private:
    // Helper that sanity-checks |buffer| duration, updates
    // |estimated_next_frame_duration_|, and emits |buffer|.
    // Returns false if |buffer| failed sanity check and therefore was not
    // emitted. Returns true otherwise.
    bool EmitBufferHelp(const std::shared_ptr<MediaSample>& buffer);

    // Helper function that calculates the buffer duration to use in
    // ApplyDurationEstimateIfNeeded().
    int64_t GetDurationEstimate();

    int track_num_;
    bool is_video_;

    // Holding the sample that is missing duration. The duration will be
    // computed from the difference in timestamp when next sample arrives; or
    // estimated if it is the last sample in this track.
    std::shared_ptr<MediaSample> last_added_buffer_missing_duration_;

    // If kNoTimestamp, then |estimated_next_frame_duration_| will be used.
    int64_t default_duration_;

    // If kNoTimestamp, then a hardcoded default value will be used. This
    // estimate is the maximum duration seen so far for this track, and is used
    // only if |default_duration_| is kNoTimestamp.
    int64_t estimated_next_frame_duration_;

    MediaParser::NewMediaSampleCB new_sample_cb_;
  };

  typedef std::map<int, Track> TextTrackMap;

 public:
  /// Create a WebMClusterParser from given parameters.
  /// @param timecode_scale indicates timecode scale for the clusters.
  /// @param audio_stream_info references audio stream information. It will
  ///        be NULL if there are no audio tracks available.
  /// @param video_stream_info references video stream information. It will
  ///        be NULL if there are no video tracks available.
  /// @param vp_config references vp configuration record. Only useful for
  ///        video.
  /// @param audio_default_duration indicates default duration for audio
  ///        samples.
  /// @param video_default_duration indicates default duration for video
  ///        samples.
  /// @param text_tracks contains text track information.
  /// @param ignored_tracks contains a list of ignored track ids.
  /// @param audio_encryption_key_id indicates the encryption key id for audio
  ///        samples if there is an audio stream and the audio stream is
  ///        encrypted. It is empty otherwise.
  /// @param video_encryption_key_id indicates the encryption key id for video
  ///        samples if there is a video stream and the video stream is
  ///        encrypted. It is empty otherwise.
  /// @param new_sample_cb is the callback to emit new samples.
  /// @param init_cb is the callback to initialize streams.
  /// @param decryption_key_source points to a decryption key source to fetch
  ///        decryption keys. Should not be NULL if the tracks are encrypted.
  WebMClusterParser(int64_t timecode_scale,
                    std::shared_ptr<AudioStreamInfo> audio_stream_info,
                    std::shared_ptr<VideoStreamInfo> video_stream_info,
                    const VPCodecConfigurationRecord& vp_config,
                    int64_t audio_default_duration,
                    int64_t video_default_duration,
                    const WebMTracksParser::TextTracks& text_tracks,
                    const std::set<int64_t>& ignored_tracks,
                    const std::string& audio_encryption_key_id,
                    const std::string& video_encryption_key_id,
                    const MediaParser::NewMediaSampleCB& new_sample_cb,
                    const MediaParser::InitCB& init_cb,
                    KeySource* decryption_key_source);
  ~WebMClusterParser() override;

  /// Resets the parser state so it can accept a new cluster.
  void Reset();

  /// Flush data currently in the parser and reset the parser so it can accept a
  /// new cluster.
  /// @return true on success, false otherwise.
  [[nodiscard]] bool Flush();

  /// Parses a WebM cluster element in |buf|.
  /// @return -1 if the parse fails.
  /// @return 0 if more data is needed.
  /// @return The number of bytes parsed on success.
  int Parse(const uint8_t* buf, int size);

  int64_t cluster_start_time() const { return cluster_start_time_; }

  /// @return true if the last Parse() call stopped at the end of a cluster.
  bool cluster_ended() const { return cluster_ended_; }

 private:
  // WebMParserClient methods.
  WebMParserClient* OnListStart(int id) override;
  bool OnListEnd(int id) override;
  bool OnUInt(int id, int64_t val) override;
  bool OnBinary(int id, const uint8_t* data, int size) override;

  bool ParseBlock(bool is_simple_block,
                  const uint8_t* buf,
                  int size,
                  const uint8_t* additional,
                  int additional_size,
                  int duration,
                  int64_t discard_padding,
                  bool reference_block_set);
  bool OnBlock(bool is_simple_block,
               int track_num,
               int timecode,
               int duration,
               const uint8_t* data,
               int size,
               const uint8_t* additional,
               int additional_size,
               int64_t discard_padding,
               bool is_key_frame);

  // Resets the Track objects associated with each text track.
  void ResetTextTracks();

  // Search for the indicated track_num among the text tracks.  Returns NULL
  // if that track num is not a text track.
  Track* FindTextTrack(int track_num);

  // Multiplier used to convert timecodes into microseconds.
  double timecode_multiplier_;

  std::shared_ptr<AudioStreamInfo> audio_stream_info_;
  std::shared_ptr<VideoStreamInfo> video_stream_info_;
  VPCodecConfigurationRecord vp_config_;
  std::set<int64_t> ignored_tracks_;

  std::unique_ptr<DecryptorSource> decryptor_source_;
  std::string audio_encryption_key_id_;
  std::string video_encryption_key_id_;

  WebMListParser parser_;

  // Indicates whether init_cb has been executed. |init_cb| is executed when we
  // have codec configuration of video stream, which is extracted from the first
  // video sample.
  bool initialized_;
  MediaParser::InitCB init_cb_;

  int64_t last_block_timecode_ = -1;
  std::unique_ptr<uint8_t[]> block_data_;
  int block_data_size_ = -1;
  int64_t block_duration_ = -1;
  int64_t block_add_id_ = -1;

  std::unique_ptr<uint8_t[]> block_additional_data_;
  // Must be 0 if |block_additional_data_| is null. Must be > 0 if
  // |block_additional_data_| is NOT null.
  int block_additional_data_size_ = 0;

  int64_t discard_padding_ = -1;
  bool discard_padding_set_ = false;

  bool reference_block_set_ = false;

  int64_t cluster_timecode_ = -1;
  int64_t cluster_start_time_;
  bool cluster_ended_ = false;

  Track audio_;
  Track video_;
  TextTrackMap text_track_map_;

  DISALLOW_COPY_AND_ASSIGN(WebMClusterParser);
};

}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_FORMATS_WEBM_WEBM_CLUSTER_PARSER_H_
