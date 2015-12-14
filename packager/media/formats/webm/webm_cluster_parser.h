// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_WEBM_WEBM_CLUSTER_PARSER_H_
#define MEDIA_FORMATS_WEBM_WEBM_CLUSTER_PARSER_H_

#include <deque>
#include <map>
#include <set>
#include <string>

#include "packager/base/memory/scoped_ptr.h"
#include "packager/media/base/media_parser.h"
#include "packager/media/base/media_sample.h"
#include "packager/media/formats/webm/webm_parser.h"
#include "packager/media/formats/webm/webm_tracks_parser.h"

namespace edash_packager {
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

  /// Opus packets encode the duration and other parameters in the 5 most
  /// significant bits of the first byte. The index in this array corresponds
  /// to the duration of each frame of the packet in microseconds. See
  /// https://tools.ietf.org/html/rfc6716#page-14
  static const uint16_t kOpusFrameDurationsMu[];

 private:
  // Helper class that manages per-track state.
  class Track {
   public:
    Track(int track_num,
          bool is_video,
          int64_t default_duration,
          const MediaParser::NewSampleCB& new_sample_cb);
    ~Track();

    int track_num() const { return track_num_; }

    // If |last_added_buffer_missing_duration_| is set, updates its duration
    // relative to |buffer|'s timestamp, and emits it and unsets
    // |last_added_buffer_missing_duration_|. Otherwise, if |buffer| is missing
    // duration, saves |buffer| into |last_added_buffer_missing_duration_|.
    bool EmitBuffer(const scoped_refptr<MediaSample>& buffer);

    // If |last_added_buffer_missing_duration_| is set, updates its duration to
    // be non-kNoTimestamp value of |estimated_next_frame_duration_| or a
    // hard-coded default, then emits it and unsets
    // |last_added_buffer_missing_duration_|. (This method helps stream parser
    // emit all buffers in a media segment before signaling end of segment.)
    void ApplyDurationEstimateIfNeeded();

    // Clears all buffer state, including any possibly held-aside buffer that
    // was missing duration.
    void Reset();

    // Helper function used to inspect block data to determine if the
    // block is a keyframe.
    // |data| contains the bytes in the block.
    // |size| indicates the number of bytes in |data|.
    bool IsKeyframe(const uint8_t* data, int size) const;

    int64_t default_duration() const { return default_duration_; }

   private:
    // Helper that sanity-checks |buffer| duration, updates
    // |estimated_next_frame_duration_|, and emits |buffer|.
    // Returns false if |buffer| failed sanity check and therefore was not
    // emitted. Returns true otherwise.
    bool EmitBufferHelp(const scoped_refptr<MediaSample>& buffer);

    // Helper that calculates the buffer duration to use in
    // ApplyDurationEstimateIfNeeded().
    int64_t GetDurationEstimate();

    // Counts the number of estimated durations used in this track. Used to
    // prevent log spam for LOG()s about estimated duration.
    int num_duration_estimates_ = 0;

    int track_num_;
    bool is_video_;

    // Parsed track buffers, each with duration and in (decode) timestamp order,
    // that have not yet been emitted. Note that up to one additional buffer
    // missing duration may be tracked by |last_added_buffer_missing_duration_|.
    scoped_refptr<MediaSample> last_added_buffer_missing_duration_;

    // If kNoTimestamp, then |estimated_next_frame_duration_| will be used.
    int64_t default_duration_;

    // If kNoTimestamp, then a default value will be used. This estimate is the
    // maximum duration seen so far for this track, and is used only if
    // |default_duration_| is kNoTimestamp.
    int64_t estimated_next_frame_duration_;

    MediaParser::NewSampleCB new_sample_cb_;
  };

  typedef std::map<int, Track> TextTrackMap;

 public:
  WebMClusterParser(int64_t timecode_scale,
                    int audio_track_num,
                    int64_t audio_default_duration,
                    int video_track_num,
                    int64_t video_default_duration,
                    const WebMTracksParser::TextTracks& text_tracks,
                    const std::set<int64_t>& ignored_tracks,
                    const std::string& audio_encryption_key_id,
                    const std::string& video_encryption_key_id,
                    const AudioCodec audio_codec,
                    const MediaParser::NewSampleCB& new_sample_cb);
  ~WebMClusterParser() override;

  /// Resets the parser state so it can accept a new cluster.
  void Reset();

  /// Flush data currently in the parser and reset the parser so it can accept a
  /// new cluster.
  void Flush();

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
                  int64_t discard_padding);
  bool OnBlock(bool is_simple_block,
               int track_num,
               int timecode,
               int duration,
               int flags,
               const uint8_t* data,
               int size,
               const uint8_t* additional,
               int additional_size,
               int64_t discard_padding);

  // Resets the Track objects associated with each text track.
  void ResetTextTracks();

  // Search for the indicated track_num among the text tracks.  Returns NULL
  // if that track num is not a text track.
  Track* FindTextTrack(int track_num);

  // Attempts to read the duration from the encoded audio data, returning as
  // kNoTimestamp if duration cannot be retrieved.
  // Avoid calling if encrypted; may produce unexpected output. See
  // implementation for supported codecs.
  int64_t TryGetEncodedAudioDuration(const uint8_t* data, int size);

  // Reads Opus packet header to determine packet duration. Duration returned
  // as kNoTimestamp upon failure to read duration from packet.
  int64_t ReadOpusDuration(const uint8_t* data, int size);

  // Tracks the number of LOGs made in process of reading encoded duration.
  // Useful to prevent log spam.
  int num_duration_errors_ = 0;

  double timecode_multiplier_;  // Multiplier used to convert timecodes into
                                // microseconds.
  std::set<int64_t> ignored_tracks_;
  std::string audio_encryption_key_id_;
  std::string video_encryption_key_id_;
  const AudioCodec audio_codec_;

  WebMListParser parser_;

  int64_t last_block_timecode_ = -1;
  scoped_ptr<uint8_t[]> block_data_;
  int block_data_size_ = -1;
  int64_t block_duration_ = -1;
  int64_t block_add_id_ = -1;

  scoped_ptr<uint8_t[]> block_additional_data_;
  // Must be 0 if |block_additional_data_| is null. Must be > 0 if
  // |block_additional_data_| is NOT null.
  int block_additional_data_size_ = 0;

  int64_t discard_padding_ = -1;
  bool discard_padding_set_ = false;

  int64_t cluster_timecode_ = -1;
  int64_t cluster_start_time_;
  bool cluster_ended_ = false;

  Track audio_;
  Track video_;
  TextTrackMap text_track_map_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(WebMClusterParser);
};

}  // namespace media
}  // namespace edash_packager

#endif  // MEDIA_FORMATS_WEBM_WEBM_CLUSTER_PARSER_H_
