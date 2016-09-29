// Copyright 2015 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef MEDIA_FORMATS_WEBM_SEEK_HEAD_H_
#define MEDIA_FORMATS_WEBM_SEEK_HEAD_H_

#include <stdint.h>
#include <vector>

#include "packager/third_party/libwebm/src/mkvmuxer.hpp"

namespace shaka {
namespace media {

/// Used to write the SeekHead to the output stream.  This supports non-seekable
/// files and setting the values before write; this also supports updating.
class SeekHead {
 public:
  SeekHead();
  ~SeekHead();

  /// Writes the seek head to the given writer.  This should only be called
  /// once.  For seekable files, use WriteVoid first, then call this method.
  bool Write(mkvmuxer::IMkvWriter* writer);
  /// Writes a void element large enough to fit the SeekHead.
  bool WriteVoid(mkvmuxer::IMkvWriter* writer);

  void set_cluster_pos(uint64_t pos) { cluster_pos_ = pos; }
  void set_cues_pos(uint64_t pos) { cues_pos_ = pos; }
  void set_info_pos(uint64_t pos) { info_pos_ = pos; }
  void set_tracks_pos(uint64_t pos) { tracks_pos_ = pos; }

 private:
  SeekHead(const SeekHead&) = delete;
  SeekHead& operator=(const SeekHead&) = delete;

  struct SeekElement {
    mkvmuxer::uint64 id;
    mkvmuxer::uint64 position;
    mkvmuxer::uint64 size;

    SeekElement(uint64_t seek_id, uint64_t seek_position)
        : id(seek_id), position(seek_position), size(0) {}
  };

  // Create seek element vector from positions.
  std::vector<SeekElement> CreateSeekElements();

  // In practice, these positions, if set, will never be 0, so we use a zero
  // value to denote that they are not set.
  uint64_t cluster_pos_ = 0;
  uint64_t cues_pos_ = 0;
  uint64_t info_pos_ = 0;
  uint64_t tracks_pos_ = 0;
  bool wrote_void_ = false;
  const uint64_t total_void_size_ = 0;
};

}  // namespace media
}  // namespace shaka

#endif  // MEDIA_FORMATS_WEBM_SEEK_HEAD_H_
