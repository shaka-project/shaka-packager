// Copyright 2017 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/media/trick_play/trick_play_handler.h>

#include <absl/log/check.h>
#include <absl/log/log.h>

#include <packager/media/base/video_stream_info.h>
#include <packager/status.h>

namespace shaka {
namespace media {
namespace {
const size_t kStreamIndexIn = 0;
const size_t kStreamIndexOut = 0;
}  // namespace

TrickPlayHandler::TrickPlayHandler(uint32_t factor) : factor_(factor) {
  DCHECK_GE(factor, 1u)
      << "Trick Play Handles must have a factor of 1 or higher.";
}

Status TrickPlayHandler::InitializeInternal() {
  return Status::OK;
}

Status TrickPlayHandler::Process(std::unique_ptr<StreamData> stream_data) {
  DCHECK(stream_data);
  DCHECK_EQ(stream_data->stream_index, kStreamIndexIn);

  switch (stream_data->stream_data_type) {
    case StreamDataType::kStreamInfo:
      return OnStreamInfo(*stream_data->stream_info);

    case StreamDataType::kSegmentInfo:
      return OnSegmentInfo(std::move(stream_data->segment_info));

    case StreamDataType::kMediaSample:
      return OnMediaSample(*stream_data->media_sample);

    case StreamDataType::kCueEvent:
      // Add the cue event to be dispatched later.
      delayed_messages_.push_back(std::move(stream_data));
      return Status::OK;

    default:
      return Status(error::TRICK_PLAY_ERROR,
                    "Trick play only supports stream info, segment info, and "
                    "media sample messages.");
  }
}

Status TrickPlayHandler::OnFlushRequest(size_t input_stream_index) {
  DCHECK_EQ(input_stream_index, 0u);

  // Send everything out in its "as-is" state as we no longer need to update
  // anything.
  Status s;
  while (s.ok() && delayed_messages_.size()) {
    s.Update(Dispatch(std::move(delayed_messages_.front())));
    delayed_messages_.pop_front();
  }

  return s.ok() ? MediaHandler::FlushAllDownstreams() : s;
}

Status TrickPlayHandler::OnStreamInfo(const StreamInfo& info) {
  if (info.stream_type() != kStreamVideo) {
    return Status(error::TRICK_PLAY_ERROR,
                  "Trick play does not support non-video stream");
  }

  // Copy the video so we can edit it. Set play back rate to be zero. It will be
  // updated later before being dispatched downstream.
  video_info_ = std::make_shared<VideoStreamInfo>(
      static_cast<const VideoStreamInfo&>(info));

  if (video_info_->trick_play_factor() > 0) {
    return Status(error::TRICK_PLAY_ERROR,
                  "This stream is already a trick play stream.");
  }

  video_info_->set_trick_play_factor(factor_);
  video_info_->set_playback_rate(0);

  // Add video info to the message queue so that it can be sent out with all
  // other messages. It won't be sent until the second trick play frame comes
  // through. Until then, it can be updated via the |video_info_| member.
  delayed_messages_.push_back(
      StreamData::FromStreamInfo(kStreamIndexOut, video_info_));

  return Status::OK;
}

Status TrickPlayHandler::OnSegmentInfo(
    std::shared_ptr<const SegmentInfo> info) {
  if (delayed_messages_.empty()) {
    return Status(error::TRICK_PLAY_ERROR,
                  "Cannot handle segments with no preceding samples.");
  }

  // Trick play does not care about sub segments, only full segments matter.
  if (info->is_subsegment) {
    return Status::OK;
  }

  const StreamDataType previous_type =
      delayed_messages_.back()->stream_data_type;

  switch (previous_type) {
    case StreamDataType::kSegmentInfo:
      // In the case that there was an empty segment (no trick frame between in
      // a segment) extend the previous segment to include the empty segment to
      // avoid holes.
      previous_segment_->duration += info->duration;
      return Status::OK;

    case StreamDataType::kMediaSample:
      // The segment has ended and there are media samples in the segment.
      // Add the segment info to the list of delayed messages. Segment info will
      // not get sent downstream until the next trick play frame comes through
      // or flush is called.
      previous_segment_ = std::make_shared<SegmentInfo>(*info);
      delayed_messages_.push_back(
          StreamData::FromSegmentInfo(kStreamIndexOut, previous_segment_));
      return Status::OK;

    default:
      return Status(error::TRICK_PLAY_ERROR,
                    "Unexpected sample in trick play deferred queue : type=" +
                        std::to_string(static_cast<int>(previous_type)));
  }
}

Status TrickPlayHandler::OnMediaSample(const MediaSample& sample) {
  total_frames_++;

  if (sample.is_key_frame()) {
    total_key_frames_++;

    if ((total_key_frames_ - 1) % factor_ == 0) {
      return OnTrickFrame(sample);
    }
  }
  // If the frame is not a trick play frame, then take the duration of this
  // frame and add it to the previous trick play frame so that it will span the
  // gap created by not passing this frame through.
  DCHECK(previous_trick_frame_);
  previous_trick_frame_->set_duration(previous_trick_frame_->duration() +
                                      sample.duration());

  return Status::OK;
}

Status TrickPlayHandler::OnTrickFrame(const MediaSample& sample) {
  total_trick_frames_++;

  // Make a message we can store until later.
  previous_trick_frame_ = sample.Clone();

  // Add the message to our queue so that it will be ready to go out.
  delayed_messages_.push_back(
      StreamData::FromMediaSample(kStreamIndexOut, previous_trick_frame_));

  // We need two trick play frames before we can send out our stream info, so we
  // cannot send this media sample until after we send our sample info
  // downstream.
  if (total_trick_frames_ < 2) {
    return Status::OK;
  }

  // Update this now as it may be sent out soon via the delay message queue.
  if (total_trick_frames_ == 2) {
    // At this point, video_info will be at the head of the delay message queue
    // and can still be updated safely.

    // The play back rate is determined by the number of frames between the
    // first two trick play frames. The first trick play frame will be the
    // first frame in the video.
    video_info_->set_playback_rate(total_frames_ - 1);
  }

  // Send out all delayed messages up until the new trick play frame we just
  // added.
  Status s;
  while (s.ok() && delayed_messages_.size() > 1) {
    s.Update(Dispatch(std::move(delayed_messages_.front())));
    delayed_messages_.pop_front();
  }
  return s;
}

}  // namespace media
}  // namespace shaka
