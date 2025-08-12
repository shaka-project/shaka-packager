// Copyright 2017 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_BASE_TRICK_PLAY_HANDLER_H_
#define PACKAGER_MEDIA_BASE_TRICK_PLAY_HANDLER_H_

#include <cstdint>
#include <list>

#include <packager/media/base/media_handler.h>

namespace shaka {
namespace media {

class VideoStreamInfo;

/// TrickPlayHandler is a single-input single-output media handler. It takes
/// the input stream and converts it to a trick play stream by limiting which
/// samples get passed downstream.
// The stream data in trick play streams are not simple duplicates. Some
// information get changed (e.g. VideoStreamInfo.trick_play_factor).
class TrickPlayHandler : public MediaHandler {
 public:
  explicit TrickPlayHandler(uint32_t factor);

 private:
  TrickPlayHandler(const TrickPlayHandler&) = delete;
  TrickPlayHandler& operator=(const TrickPlayHandler&) = delete;

  Status InitializeInternal() override;
  Status Process(std::unique_ptr<StreamData> stream_data) override;
  Status OnFlushRequest(size_t input_stream_index) override;

  Status OnStreamInfo(const StreamInfo& info);
  Status OnSegmentInfo(std::shared_ptr<const SegmentInfo> info);
  Status OnMediaSample(const MediaSample& sample);
  Status OnTrickFrame(const MediaSample& sample);

  const uint32_t factor_;

  uint64_t total_frames_ = 0;
  uint64_t total_key_frames_ = 0;
  uint64_t total_trick_frames_ = 0;

  // We cannot just send video info through as we need to calculate the play
  // rate using the first two trick play frames. This reference should only be
  // used to update the play back rate before video info is sent downstream.
  // After getting sent downstream, this should never be used.
  std::shared_ptr<VideoStreamInfo> video_info_;

  // We need to track the segment that most recently finished so that we can
  // extend its duration if there are empty segments.
  std::shared_ptr<SegmentInfo> previous_segment_;

  // Since we are dropping frames, the time that those frames would have been
  // on screen need to be added to the frame before them. Keep a reference to
  // the most recent trick play frame so that we can grow its duration as we
  // drop other frames.
  std::shared_ptr<MediaSample> previous_trick_frame_;

  // Since we cannot send messages downstream right away, keep a queue of
  // messages that need to be sent down. At the start, we use this to queue
  // messages until we can send out |video_info_|. To ensure messages are
  // kept in order, messages are only dispatched through this queue and never
  // directly.
  std::list<std::unique_ptr<StreamData>> delayed_messages_;
};

}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_BASE_TRICK_PLAY_HANDLER_H_
