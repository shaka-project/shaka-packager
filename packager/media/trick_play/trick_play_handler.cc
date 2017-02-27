// Copyright 2017 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/trick_play/trick_play_handler.h"

#include "packager/base/logging.h"
#include "packager/media/base/video_stream_info.h"

namespace shaka {
namespace media {

TrickPlayHandler::TrickPlayHandler(const TrickPlayOptions& trick_play_option)
    : trick_play_options_(trick_play_option),
      cached_stream_data_(trick_play_option.trick_play_rates.size()) {
  for (auto rate : trick_play_option.trick_play_rates) {
    CHECK_GT(rate, 0);
  }
}

TrickPlayHandler::~TrickPlayHandler() {}

Status TrickPlayHandler::InitializeInternal() {
  return Status::OK;
}

Status TrickPlayHandler::Process(
    std::unique_ptr<StreamData> input_stream_data) {
  // The non-trick play stream is dispatched at index 0.
  // The trick-play streams are dispatched to index 1, index 2 and so on.
  DCHECK_EQ(input_stream_data->stream_index, 0u);
  std::unique_ptr<StreamData> output_stream_data(new StreamData());
  *output_stream_data = *input_stream_data;
  Status status = Dispatch(std::move(output_stream_data));
  if (!status.ok()) {
    return status;
  }

  std::shared_ptr<StreamData> stream_data(std::move(input_stream_data));
  if (stream_data->stream_data_type == StreamDataType::kStreamInfo) {
    if (stream_data->stream_info->stream_type() != kStreamVideo) {
      status.SetError(error::TRICK_PLAY_ERROR,
                      "Trick play does not support non-video stream");
      return status;
    }
    const VideoStreamInfo& video_stream_info =
        static_cast<const VideoStreamInfo&>(*stream_data->stream_info);
    if (video_stream_info.trick_play_rate() > 0) {
      status.SetError(error::TRICK_PLAY_ERROR,
                      "This stream is alreay a trick play stream.");
      return status;
    }
  }

  if (stream_data->stream_data_type != StreamDataType::kMediaSample) {
    // Non media sample stream data needs to be dispatched to every output
    // stream. It is just cached in every queue until a new key frame comes or
    // the stream is flushed.
    for (size_t i = 0; i < cached_stream_data_.size(); ++i)
      cached_stream_data_[i].push_back(stream_data);
    return Status::OK;
  }

  if (stream_data->media_sample->is_key_frame()) {
    // For a new key frame, some of the trick play streams may include it.
    // The cached data in those trick play streams will be processed.
    DCHECK_EQ(trick_play_options_.trick_play_rates.size(),
              cached_stream_data_.size());
    for (size_t i = 0; i < cached_stream_data_.size(); ++i) {
      int16_t rate = trick_play_options_.trick_play_rates[i];
      if (total_key_frames_ % rate == 0) {
        if (!cached_stream_data_[i].empty()) {
          Status status =
              ProcessCachedStreamData(i + 1, &cached_stream_data_[i]);
          if (!status.ok())
            return status;
        }
        cached_stream_data_[i].push_back(stream_data);
      }
    }

    total_key_frames_++;
  }

  prev_sample_end_timestamp_ =
      stream_data->media_sample->dts() + stream_data->media_sample->duration();
  return Status::OK;
}

bool TrickPlayHandler::ValidateOutputStreamIndex(size_t stream_index) const {
  // Output stream index should be less than the number of trick play
  // streams + one original stream.
  return stream_index <= trick_play_options_.trick_play_rates.size();
};

Status TrickPlayHandler::OnFlushRequest(size_t input_stream_index) {
  DCHECK_EQ(input_stream_index, 0u)
      << "Trick Play Handler should only have single input.";
  for (size_t i = 0; i < cached_stream_data_.size(); ++i) {
    ProcessCachedStreamData(i + 1, &cached_stream_data_[i]);
  }
  return MediaHandler::FlushDownstream(input_stream_index);
}

Status TrickPlayHandler::ProcessCachedStreamData(
    int output_stream_index,
    std::deque<std::shared_ptr<StreamData>>* cached_stream_data) {
  while (!cached_stream_data->empty()) {
    Status status =
        ProcessOneStreamData(output_stream_index, cached_stream_data->front());
    if (!status.ok()) {
      return status;
    }
    cached_stream_data->pop_front();
  }
  return Status::OK;
}

Status TrickPlayHandler::ProcessOneStreamData(
    int output_stream_index,
    const std::shared_ptr<StreamData>& stream_data) {
  uint32_t trick_play_rate =
      trick_play_options_.trick_play_rates[output_stream_index - 1];
  Status status;
  switch (stream_data->stream_data_type) {
    // trick_play_rate in StreamInfo should be modified.
    case StreamDataType::kStreamInfo: {
      const VideoStreamInfo& video_stream_info =
          static_cast<const VideoStreamInfo&>(*stream_data->stream_info);
      std::shared_ptr<VideoStreamInfo> trick_play_video_stream_info(
          new VideoStreamInfo(video_stream_info));
      trick_play_video_stream_info->set_trick_play_rate(trick_play_rate);
      status =
          DispatchStreamInfo(output_stream_index, trick_play_video_stream_info);
      break;
    }
    case StreamDataType::kMediaSample: {
      if (stream_data->media_sample->is_key_frame()) {
        std::shared_ptr<MediaSample> trick_play_media_sample =
            MediaSample::CopyFrom(*(stream_data->media_sample));
        trick_play_media_sample->set_duration(prev_sample_end_timestamp_ -
                                              stream_data->media_sample->dts());

        status =
            DispatchMediaSample(output_stream_index, trick_play_media_sample);
      }
      break;
    }
    default:
      std::unique_ptr<StreamData> new_stream_data(new StreamData(*stream_data));
      new_stream_data->stream_index = output_stream_index;
      status = Dispatch(std::move(new_stream_data));
      break;
  }
  return status;
}

}  // namespace media
}  // namespace shaka
