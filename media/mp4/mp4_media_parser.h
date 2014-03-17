// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef MEDIA_MP4_MP4_MEDIA_PARSER_H_
#define MEDIA_MP4_MP4_MEDIA_PARSER_H_

#include <vector>

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "media/base/media_parser.h"
#include "media/mp4/offset_byte_queue.h"

namespace media {

namespace mp4 {

struct Movie;
class BoxReader;
class ProtectionSystemSpecificHeader;
class TrackRunIterator;

class MP4MediaParser : public MediaParser {
 public:
  MP4MediaParser();
  virtual ~MP4MediaParser();

  /// @name MediaParser implementation overrides.
  /// @{
  virtual void Init(const InitCB& init_cb,
                    const NewSampleCB& new_sample_cb,
                    const NeedKeyCB& need_key_cb) OVERRIDE;
  virtual bool Parse(const uint8* buf, int size) OVERRIDE;
  /// @}

 private:
  enum State {
    kWaitingForInit,
    kParsingBoxes,
    kEmittingSamples,
    kError
  };

  bool ParseBox(bool* err);
  bool ParseMoov(mp4::BoxReader* reader);
  bool ParseMoof(mp4::BoxReader* reader);

  void EmitNeedKeyIfNecessary(
      const std::vector<ProtectionSystemSpecificHeader>& headers);

  // To retain proper framing, each 'mdat' atom must be read; to limit memory
  // usage, the atom's data needs to be discarded incrementally as frames are
  // extracted from the stream. This function discards data from the stream up
  // to |offset|, updating the |mdat_tail_| value so that framing can be
  // retained after all 'mdat' information has been read.
  // Returns 'true' on success, 'false' if there was an error.
  bool ReadAndDiscardMDATsUntil(const int64 offset);

  void ChangeState(State new_state);

  bool EmitConfigs();

  bool EnqueueSample(bool* err);

  void Reset();

  State state_;
  InitCB init_cb_;
  NewSampleCB new_sample_cb_;
  NeedKeyCB need_key_cb_;

  OffsetByteQueue queue_;

  // These two parameters are only valid in the |kEmittingSegments| state.
  //
  // |moof_head_| is the offset of the start of the most recently parsed moof
  // block. All byte offsets in sample information are relative to this offset,
  // as mandated by the Media Source spec.
  int64 moof_head_;
  // |mdat_tail_| is the stream offset of the end of the current 'mdat' box.
  // Valid iff it is greater than the head of the queue.
  int64 mdat_tail_;

  scoped_ptr<Movie> moov_;
  scoped_ptr<TrackRunIterator> runs_;

  DISALLOW_COPY_AND_ASSIGN(MP4MediaParser);
};

}  // namespace mp4
}  // namespace media

#endif  // MEDIA_MP4_MP4_MEDIA_PARSER_H_
