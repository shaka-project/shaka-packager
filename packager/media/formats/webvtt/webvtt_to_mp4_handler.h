// Copyright 2017 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_FORMATS_WEBVTT_WEBVTT_MP4_CUE_HANDLER_H_
#define PACKAGER_MEDIA_FORMATS_WEBVTT_WEBVTT_MP4_CUE_HANDLER_H_

#include <stdint.h>

#include <list>
#include <queue>

#include "packager/media/base/buffer_writer.h"
#include "packager/media/base/media_handler.h"

namespace shaka {
namespace media {

class DisplayAction;

class DisplayActionCompare {
 public:
  bool operator()(const std::shared_ptr<DisplayAction>& left,
                  const std::shared_ptr<DisplayAction>& right) const;
};

// Take text samples, convert them to Mp4 boxes, and send them down stream.
// Virtual methods should only be overridden for testing only.
class WebVttToMp4Handler : public MediaHandler {
 public:
  WebVttToMp4Handler() = default;

 protected:
  // |Process| and |OnFlushRequest| need to be protected so that it can be
  // called for testing.
  Status Process(std::unique_ptr<StreamData> stream_data) override;
  Status OnFlushRequest(size_t input_stream_index) override;

  // This is made protected-virtual so that we can override it for testing.
  virtual void WriteCue(const std::string& id,
                        const std::string& settings,
                        const std::string& payload,
                        BufferWriter* out);

 private:
  WebVttToMp4Handler(const WebVttToMp4Handler&) = delete;
  WebVttToMp4Handler& operator=(const WebVttToMp4Handler&) = delete;

  Status InitializeInternal() override;

  // Merge and send all samples in the queue downstream while the head of the
  // queue's time is less than |cutoff|. |cutoff| is needed as we can only
  // merge and send samples when we are sure no new samples will appear before
  // the next action.
  void ProcessUpToTime(uint64_t cutoff_time);

  // Merge together all TextSamples in |samples| into a single MP4 box and
  // pass the box downstream.
  Status MergeAndSendSamples(const std::list<const TextSample*>& samples,
                             uint64_t start_time,
                             uint64_t end_time);

  // Take a Mp4 box as a byte buffer and send it downstream.
  Status WriteSample(uint64_t start,
                     uint64_t end,
                     const uint8_t* sample,
                     size_t sample_length);

  // Get a new id for the next action.
  uint64_t NextActionId();

  uint64_t next_change_ = 0;

  // This is the current state of the box we are writing.
  BufferWriter box_writer_;

  // |actions_| is a time sorted list of actions that affect the timeline (e.g.
  //  adding or removing a cue). |active_| is the list of all cues that are
  // currently on screen.
  // When the cue is to be on screen, it is added to |active_|. When it is time
  // for the cue to come off screen, it is removed from |active_|.
  // As |actions_| has a shared pointer to the cue, |active_| can use normal
  // pointers as the pointer will be valid and it makes the |remove| call
  // easier.

  std::priority_queue<std::shared_ptr<DisplayAction>,
                      std::vector<std::shared_ptr<DisplayAction>>,
                      DisplayActionCompare>
      actions_;
  std::list<const TextSample*> active_;

  uint64_t next_id_ = 0;
};

}  // namespace media
}  // namespace shaka
#endif  // PACKAGER_MEDIA_FORMATS_WEBVTT_WEBVTT_MP4_CUE_HANDLER_H_
