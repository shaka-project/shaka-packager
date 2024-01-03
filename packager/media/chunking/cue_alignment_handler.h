// Copyright 2018 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_CHUNKING_CUE_ALIGNMENT_HANDLER_
#define PACKAGER_MEDIA_CHUNKING_CUE_ALIGNMENT_HANDLER_

#include <deque>
#include <list>

#include <packager/media/base/media_handler.h>
#include <packager/media/chunking/sync_point_queue.h>

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
    // Only set for text stream.
    double max_text_sample_end_time_seconds = 0;

    // A list of cues that the stream should inject between media samples. When
    // there are no cues, the stream should run up to the hint.
    std::list<std::unique_ptr<StreamData>> cues;
  };

  // MediaHandler overrides.
  Status InitializeInternal() override;
  Status Process(std::unique_ptr<StreamData> data) override;
  Status OnFlushRequest(size_t stream_index) override;

  // Internal handling functions for different stream data.
  Status OnStreamInfo(std::unique_ptr<StreamData> data);

  Status OnVideoSample(std::unique_ptr<StreamData> sample);
  Status OnNonVideoSample(std::unique_ptr<StreamData> sample);
  Status OnSample(std::unique_ptr<StreamData> sample);

  // Update stream states with new sync point.
  Status UseNewSyncPoint(std::shared_ptr<const CueEvent> new_sync);

  // Check if everyone is waiting for new hint points.
  bool EveryoneWaitingAtHint() const;

  // Dispatch or save incoming sample.
  Status AcceptSample(std::unique_ptr<StreamData> sample,
                      StreamState* stream_state);

  // Dispatch all samples and cues (in the correct order) for the given stream.
  Status RunThroughSamples(StreamState* stream);

  SyncPointQueue* const sync_points_ = nullptr;
  std::deque<StreamState> stream_states_;

  // A common hint used by all streams. When a new cue is given to all streams,
  // the hint will be updated. The hint will always be larger than any cue. The
  // hint represents the min time in seconds for the next cue appear. The hints
  // are based off the un-promoted cue event times in |sync_points_|.
  //
  // When a video stream passes the hint, it will promote the corresponding cue
  // event. If all streams get to the hint and there are no video streams, the
  // thread will block until |sync_points_| gives back a promoted cue event.
  double hint_;
};

}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_CHUNKING_CUE_ALIGNMENT_HANDLER_
