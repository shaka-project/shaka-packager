// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_WEBM_TRACKS_BUILDER_H_
#define MEDIA_WEBM_TRACKS_BUILDER_H_

#include <list>
#include <string>
#include <vector>

#include "base/basictypes.h"

namespace media {

class TracksBuilder {
 public:
  TracksBuilder();
  ~TracksBuilder();

  void AddTrack(int track_num, int track_type, const std::string& codec_id,
                const std::string& name, const std::string& language);

  std::vector<uint8> Finish();

 private:
  int GetTracksSize() const;
  int GetTracksPayloadSize() const;
  void WriteTracks(uint8* buffer, int buffer_size) const;

  class Track {
   public:
    Track(int track_num, int track_type, const std::string& codec_id,
          const std::string& name, const std::string& language);

    int GetSize() const;
    void Write(uint8** buf, int* buf_size) const;
   private:
    int GetPayloadSize() const;

    int track_num_;
    int track_type_;
    std::string codec_id_;
    std::string name_;
    std::string language_;
  };

  typedef std::list<Track> TrackList;
  TrackList tracks_;

  DISALLOW_COPY_AND_ASSIGN(TracksBuilder);
};

}  // namespace media

#endif  // MEDIA_WEBM_TRACKS_BUILDER_H_
