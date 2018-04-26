// Copyright 2018 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_CHUNKING_CUE_ALIGNMENT_HANDLER_
#define PACKAGER_MEDIA_CHUNKING_CUE_ALIGNMENT_HANDLER_

#include <list>

#include "packager/media/base/media_handler.h"
#include "packager/media/chunking/sync_point_queue.h"

namespace shaka {
namespace media {

/// The cue alignment handler is a N-to-N handler that will inject CueEvents
/// into all streams. It will align the cues across streams (and handlers)
/// using a shared SyncPointQueue.
///
/// There should be a cue alignment handler per demuxer/thread and not per
/// stream. A cue alignment handler must be one per thread in order to properly
/// manage blocking.
class CueAlignmentHandler : public MediaHandler {
 public:
  explicit CueAlignmentHandler(SyncPointQueue* sync_points);
  ~CueAlignmentHandler() = default;

 private:
  CueAlignmentHandler(const CueAlignmentHandler&) = delete;
  CueAlignmentHandler& operator=(const CueAlignmentHandler&) = delete;

  struct StreamState {
    // Information for the stream.
    std::shared_ptr<const StreamInfo> info;
    // Cached samples that cannot be dispatched. All the samples should be at or
    // after |hint|.
    std::list<std::unique_ptr<StreamData>> samples;
    // If set, the stream is pending to be flushed.
    bool to_be_flushed = false;

    // If set, it points to the next cue it has to send downstream. Note that if
    // it is not set, the next cue is not determined.
    // This is set but not really used by video stream.
    std::shared_ptr<const CueEvent> cue;
    // If |cue| is set, this is the hint for the cue after |cue|, i.e. next next
    // cue; otherwise this is the hint for the next cue. This holds the time in
    // seconds of the scheduled (unpromoted) next or next next cue's time. This
    // is essentially the barrier for the sample stream. Non video samples after
    // this barrier must wait until the |cue| is determined and thus the next
    // hint to be determined; video samples will promote the hint to cue when
    // seeing the first key frame after |next_cue_hint|.
    double next_cue_hint = 0;
  };

  // MediaHandler overrides.
  Status InitializeInternal() override;
  Status Process(std::unique_ptr<StreamData> data) override;
  Status OnFlushRequest(size_t stream_index) override;

  // Internal handling functions for different stream data.
  Status OnStreamInfo(std::unique_ptr<StreamData> data);
  Status OnSample(std::unique_ptr<StreamData> sample);

  // Update stream states with new sync point.
  Status UseNewSyncPoint(std::shared_ptr<const CueEvent> new_sync);

  // Check if everyone is waiting for new hint points.
  bool EveryoneWaitingAtHint() const;

  // Dispatch or save incoming sample.
  Status AcceptSample(std::unique_ptr<StreamData> sample,
                      StreamState* stream_state);

  // Dispatch the cue if the new sample comes at or after it.
  Status DispatchCueIfNeeded(size_t stream_index,
                             double next_sample_time_in_seconds,
                             StreamState* stream_state);

  SyncPointQueue* const sync_points_ = nullptr;
  std::vector<StreamState> stream_states_;
};

}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_CHUNKING_CUE_ALIGNMENT_HANDLER_
