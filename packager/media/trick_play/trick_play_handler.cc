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

namespace {
const size_t kMainStreamIndex = 0;
}

TrickPlayHandler::TrickPlayHandler() {}

TrickPlayHandler::~TrickPlayHandler() {}

void TrickPlayHandler::SetHandlerForMainStream(
    std::shared_ptr<MediaHandler> handler) {
  SetHandler(kMainStreamIndex, std::move(handler));
}

void TrickPlayHandler::SetHandlerForTrickPlay(
    uint32_t trick_play_factor,
    std::shared_ptr<MediaHandler> handler) {
  trick_play_factors_.push_back(trick_play_factor);
  // Trick play streams start from index 1.
  SetHandler(trick_play_factors_.size(), std::move(handler));
}

Status TrickPlayHandler::InitializeInternal() {
  if (!HasMainStream()) {
    return Status(error::TRICK_PLAY_ERROR,
                  "Trick play does not have main stream");
  }
  if (trick_play_factors_.empty()) {
    return Status(error::TRICK_PLAY_ERROR,
                  "Trick play factors are not specified.");
  }
  size_t num_trick_play_factors = trick_play_factors_.size();
  cached_stream_data_.resize(num_trick_play_factors);
  playback_rates_.resize(num_trick_play_factors, 0);

  return Status::OK;
}

Status TrickPlayHandler::Process(std::unique_ptr<StreamData> stream_data) {
  // The non-trick play stream is dispatched at index 0.
  // The trick-play streams are dispatched to index 1, index 2 and so on.
  DCHECK(stream_data);
  DCHECK_EQ(stream_data->stream_index, 0u);

  std::unique_ptr<StreamData> copy(new StreamData);
  *copy = *stream_data;
  Status status = Dispatch(std::move(copy));
  if (!status.ok()) {
    return status;
  }

  std::shared_ptr<StreamData> shared_stream_data(std::move(stream_data));

  if (shared_stream_data->stream_data_type == StreamDataType::kStreamInfo) {
    if (shared_stream_data->stream_info->stream_type() != kStreamVideo) {
      status.SetError(error::TRICK_PLAY_ERROR,
                      "Trick play does not support non-video stream");
      return status;
    }
    const VideoStreamInfo& video_stream_info =
        static_cast<const VideoStreamInfo&>(*shared_stream_data->stream_info);
    if (video_stream_info.trick_play_factor() > 0) {
      status.SetError(error::TRICK_PLAY_ERROR,
                      "This stream is alreay a trick play stream.");
      return status;
    }
  }

  if (shared_stream_data->stream_data_type == StreamDataType::kSegmentInfo) {
    for (auto& cached_data : cached_stream_data_) {
      // It is possible that trick play stream has large frame duration that
      // some segments in the main stream are skipped. To avoid empty segments,
      // only cache SegementInfo with MediaSample before it.
      if (!cached_data.empty() &&
          cached_data.back()->stream_data_type == StreamDataType::kMediaSample)
        cached_data.push_back(shared_stream_data);
    }
    return Status::OK;
  }

  if (shared_stream_data->stream_data_type != StreamDataType::kMediaSample) {
    // Non media sample stream data needs to be dispatched to every output
    // stream. It is just cached in every queue until a new key frame comes or
    // the stream is flushed.
    for (size_t i = 0; i < cached_stream_data_.size(); ++i)
      cached_stream_data_[i].push_back(shared_stream_data);
    return Status::OK;
  }

  if (shared_stream_data->media_sample->is_key_frame()) {
    // For a new key frame, some of the trick play streams may include it.
    // The cached data in those trick play streams will be processed.
    DCHECK_EQ(trick_play_factors_.size(), cached_stream_data_.size());
    for (size_t i = 0; i < cached_stream_data_.size(); ++i) {
      uint32_t factor = trick_play_factors_[i];
      if (total_key_frames_ % factor == 0) {
        // Delay processing cached stream data until receiving the second key
        // frame so that the GOP size could be derived.
        if (!cached_stream_data_[i].empty() && total_key_frames_ > 0) {
          // Num of frames between first two key frames in the trick play
          // streams. Use this as the playback_rate.
          if (playback_rates_[i] == 0)
            playback_rates_[i] = total_frames_;

          Status status =
              ProcessCachedStreamData(i + 1, &cached_stream_data_[i]);
          if (!status.ok())
            return status;
        }
        cached_stream_data_[i].push_back(shared_stream_data);
      }
    }

    total_key_frames_++;
  }

  total_frames_++;
  prev_sample_end_timestamp_ = shared_stream_data->media_sample->dts() +
                               shared_stream_data->media_sample->duration();

  return Status::OK;
}

bool TrickPlayHandler::ValidateOutputStreamIndex(size_t stream_index) const {
  // Output stream index should be less than the number of trick play
  // streams + one original stream.
  return stream_index <= trick_play_factors_.size();
};

Status TrickPlayHandler::OnFlushRequest(size_t input_stream_index) {
  DCHECK_EQ(input_stream_index, 0u)
      << "Trick Play Handler should only have single input.";
  for (size_t i = 0; i < cached_stream_data_.size(); ++i) {
    LOG_IF(WARNING, playback_rates_[i] == 0)
        << "Max playout rate for trick play factor " << trick_play_factors_[i]
        << " is not determined. "
        << "Specify it as total number of frames: " << total_frames_ << ".";
    playback_rates_[i] = total_frames_;
    ProcessCachedStreamData(i + 1, &cached_stream_data_[i]);
  }
  return MediaHandler::FlushAllDownstreams();
}

bool TrickPlayHandler::HasMainStream() {
  const auto& handlers = output_handlers();
  const auto& main_stream_handler = handlers.find(kMainStreamIndex);
  if (main_stream_handler == handlers.end()) {
    return false;
  }
  return main_stream_handler->second.first != nullptr;
}

Status TrickPlayHandler::ProcessCachedStreamData(
    size_t output_stream_index,
    std::deque<std::shared_ptr<StreamData>>* cached_stream_data) {
  while (!cached_stream_data->empty()) {
    Status status =
        ProcessOneStreamData(output_stream_index, *cached_stream_data->front());
    if (!status.ok()) {
      return status;
    }
    cached_stream_data->pop_front();
  }
  return Status::OK;
}

Status TrickPlayHandler::ProcessOneStreamData(size_t output_stream_index,
                                              const StreamData& stream_data) {
  size_t trick_play_index = output_stream_index - 1;
  uint32_t trick_play_factor = trick_play_factors_[trick_play_index];
  Status status;
  switch (stream_data.stream_data_type) {
    // trick_play_factor in StreamInfo should be modified.
    case StreamDataType::kStreamInfo: {
      const VideoStreamInfo& video_stream_info =
          static_cast<const VideoStreamInfo&>(*stream_data.stream_info);
      std::shared_ptr<VideoStreamInfo> trick_play_video_stream_info(
          new VideoStreamInfo(video_stream_info));
      trick_play_video_stream_info->set_trick_play_factor(trick_play_factor);
      DCHECK_GT(playback_rates_[trick_play_index], 0u);
      trick_play_video_stream_info->set_playback_rate(
          playback_rates_[trick_play_index]);
      status =
          DispatchStreamInfo(output_stream_index, trick_play_video_stream_info);
      break;
    }
    case StreamDataType::kMediaSample: {
      if (stream_data.media_sample->is_key_frame()) {
        std::shared_ptr<MediaSample> trick_play_media_sample =
            MediaSample::CopyFrom(*(stream_data.media_sample));
        trick_play_media_sample->set_duration(prev_sample_end_timestamp_ -
                                              stream_data.media_sample->dts());
        status =
            DispatchMediaSample(output_stream_index, trick_play_media_sample);
      }
      break;
    }
    default:
      std::unique_ptr<StreamData> copy(new StreamData(stream_data));
      copy->stream_index = output_stream_index;
      status = Dispatch(std::move(copy));
      break;
  }
  return status;
}

}  // namespace media
}  // namespace shaka
