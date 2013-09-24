// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_WEBM_WEBM_CLUSTER_PARSER_H_
#define MEDIA_WEBM_WEBM_CLUSTER_PARSER_H_

#include <deque>
#include <map>
#include <set>
#include <string>

#include "base/memory/scoped_ptr.h"
#include "media/base/media_export.h"
#include "media/base/media_log.h"
#include "media/base/stream_parser_buffer.h"
#include "media/webm/webm_parser.h"
#include "media/webm/webm_tracks_parser.h"

namespace media {

class MEDIA_EXPORT WebMClusterParser : public WebMParserClient {
 private:
  // Helper class that manages per-track state.
  class Track {
   public:
    Track(int track_num, bool is_video);
    ~Track();

    int track_num() const { return track_num_; }
    const std::deque<scoped_refptr<StreamParserBuffer> >& buffers() const {
      return buffers_;
    }

    bool AddBuffer(const scoped_refptr<StreamParserBuffer>& buffer);

    // Clears all buffer state.
    void Reset();

    // Helper function used to inspect block data to determine if the
    // block is a keyframe.
    // |data| contains the bytes in the block.
    // |size| indicates the number of bytes in |data|.
    bool IsKeyframe(const uint8* data, int size) const;

   private:
    int track_num_;
    std::deque<scoped_refptr<StreamParserBuffer> > buffers_;
    bool is_video_;
  };

  typedef std::map<int, Track> TextTrackMap;

 public:
  typedef std::deque<scoped_refptr<StreamParserBuffer> > BufferQueue;

  class MEDIA_EXPORT TextTrackIterator {
   public:
    explicit TextTrackIterator(const TextTrackMap& text_track_map);
    TextTrackIterator(const TextTrackIterator& rhs);
    ~TextTrackIterator();

    // To visit each text track.  If the iterator is exhausted, it returns
    // as parameters the values 0 and NULL, and the function returns false.
    // Otherwise, it returns the buffers for the associated track, and the
    // function returns true.
    bool operator()(int* track_num, const BufferQueue** buffers);
   private:
    TextTrackIterator& operator=(const TextTrackIterator&);

    TextTrackMap::const_iterator iterator_;
    const TextTrackMap::const_iterator iterator_end_;
  };

  WebMClusterParser(int64 timecode_scale,
                    int audio_track_num,
                    int video_track_num,
                    const WebMTracksParser::TextTracks& text_tracks,
                    const std::set<int64>& ignored_tracks,
                    const std::string& audio_encryption_key_id,
                    const std::string& video_encryption_key_id,
                    const LogCB& log_cb);
  virtual ~WebMClusterParser();

  // Resets the parser state so it can accept a new cluster.
  void Reset();

  // Parses a WebM cluster element in |buf|.
  //
  // Returns -1 if the parse fails.
  // Returns 0 if more data is needed.
  // Returns the number of bytes parsed on success.
  int Parse(const uint8* buf, int size);

  base::TimeDelta cluster_start_time() const { return cluster_start_time_; }
  const BufferQueue& audio_buffers() const { return audio_.buffers(); }
  const BufferQueue& video_buffers() const { return video_.buffers(); }

  // Returns an iterator object, allowing each text track to be visited.
  TextTrackIterator CreateTextTrackIterator() const;

  // Returns true if the last Parse() call stopped at the end of a cluster.
  bool cluster_ended() const { return cluster_ended_; }

 private:
  // WebMParserClient methods.
  virtual WebMParserClient* OnListStart(int id) OVERRIDE;
  virtual bool OnListEnd(int id) OVERRIDE;
  virtual bool OnUInt(int id, int64 val) OVERRIDE;
  virtual bool OnBinary(int id, const uint8* data, int size) OVERRIDE;

  bool ParseBlock(bool is_simple_block, const uint8* buf, int size,
                  const uint8* additional, int additional_size, int duration);
  bool OnBlock(bool is_simple_block, int track_num, int timecode, int duration,
               int flags, const uint8* data, int size,
               const uint8* additional, int additional_size);

  // Resets the Track objects associated with each text track.
  void ResetTextTracks();

  // Search for the indicated track_num among the text tracks.  Returns NULL
  // if that track num is not a text track.
  Track* FindTextTrack(int track_num);

  double timecode_multiplier_;  // Multiplier used to convert timecodes into
                                // microseconds.
  std::set<int64> ignored_tracks_;
  std::string audio_encryption_key_id_;
  std::string video_encryption_key_id_;

  WebMListParser parser_;

  int64 last_block_timecode_;
  scoped_ptr<uint8[]> block_data_;
  int block_data_size_;
  int64 block_duration_;
  int64 block_add_id_;
  scoped_ptr<uint8[]> block_additional_data_;
  int block_additional_data_size_;

  int64 cluster_timecode_;
  base::TimeDelta cluster_start_time_;
  bool cluster_ended_;

  Track audio_;
  Track video_;
  TextTrackMap text_track_map_;
  LogCB log_cb_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(WebMClusterParser);
};

}  // namespace media

#endif  // MEDIA_WEBM_WEBM_CLUSTER_PARSER_H_
