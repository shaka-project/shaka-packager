// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MP4_MP4_STREAM_PARSER_H_
#define MEDIA_MP4_MP4_STREAM_PARSER_H_

#include <set>
#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "media/base/media_export.h"
#include "media/base/stream_parser.h"
#include "media/mp4/offset_byte_queue.h"
#include "media/mp4/track_run_iterator.h"

namespace media {
namespace mp4 {

struct Movie;
class BoxReader;

class MEDIA_EXPORT MP4StreamParser : public StreamParser {
 public:
  MP4StreamParser(const std::set<int>& audio_object_types, bool has_sbr);
  virtual ~MP4StreamParser();

  virtual void Init(const InitCB& init_cb, const NewConfigCB& config_cb,
                    const NewBuffersCB& new_buffers_cb,
                    const NewTextBuffersCB& text_cb,
                    const NeedKeyCB& need_key_cb,
                    const AddTextTrackCB& add_text_track_cb,
                    const NewMediaSegmentCB& new_segment_cb,
                    const base::Closure& end_of_segment_cb,
                    const LogCB& log_cb) OVERRIDE;
  virtual void Flush() OVERRIDE;
  virtual bool Parse(const uint8* buf, int size) OVERRIDE;

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
  bool PrepareAVCBuffer(const AVCDecoderConfigurationRecord& avc_config,
                        std::vector<uint8>* frame_buf,
                        std::vector<SubsampleEntry>* subsamples) const;
  bool PrepareAACBuffer(const AAC& aac_config,
                        std::vector<uint8>* frame_buf,
                        std::vector<SubsampleEntry>* subsamples) const;
  bool EnqueueSample(BufferQueue* audio_buffers,
                     BufferQueue* video_buffers,
                     bool* err);
  bool SendAndFlushSamples(BufferQueue* audio_buffers,
                           BufferQueue* video_buffers);

  void Reset();

  State state_;
  InitCB init_cb_;
  NewConfigCB config_cb_;
  NewBuffersCB new_buffers_cb_;
  NeedKeyCB need_key_cb_;
  NewMediaSegmentCB new_segment_cb_;
  base::Closure end_of_segment_cb_;
  LogCB log_cb_;

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

  scoped_ptr<mp4::Movie> moov_;
  scoped_ptr<mp4::TrackRunIterator> runs_;

  bool has_audio_;
  bool has_video_;
  uint32 audio_track_id_;
  uint32 video_track_id_;
  // The object types allowed for audio tracks.
  std::set<int> audio_object_types_;
  bool has_sbr_;
  bool is_audio_track_encrypted_;
  bool is_video_track_encrypted_;

  DISALLOW_COPY_AND_ASSIGN(MP4StreamParser);
};

}  // namespace mp4
}  // namespace media

#endif  // MEDIA_MP4_MP4_STREAM_PARSER_H_
