// Copyright 2015 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef MEDIA_FORMATS_WEBM_SEEK_HEAD_H_
#define MEDIA_FORMATS_WEBM_SEEK_HEAD_H_

#include <stdint.h>
#include <vector>

#include "base/macros.h"
#include "packager/third_party/libwebm/src/mkvmuxer.hpp"

namespace edash_packager {
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
  uint64_t GetPayloadSize(std::vector<uint64_t>* data);

  int64_t cluster_pos_;
  int64_t cues_pos_;
  int64_t info_pos_;
  int64_t tracks_pos_;
  bool wrote_void_;

  DISALLOW_COPY_AND_ASSIGN(SeekHead);
};

}  // namespace media
}  // namespace edash_packager

#endif  // MEDIA_FORMATS_WEBM_SEEK_HEAD_H_
