// Copyright 2017 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/chunking/chunking_handler.h"

#include "packager/base/logging.h"
#include "packager/base/threading/platform_thread.h"
#include "packager/media/base/media_sample.h"

namespace {
int64_t kThreadIdUnset = -1;
int64_t kTimeStampToDispatchAllSamples = -1;
}  // namespace

namespace shaka {
namespace media {

ChunkingHandler::ChunkingHandler(const ChunkingOptions& chunking_options)
    : chunking_options_(chunking_options), thread_id_(kThreadIdUnset) {
  CHECK_NE(chunking_options.segment_duration_in_seconds, 0u);
}

ChunkingHandler::~ChunkingHandler() {}

Status ChunkingHandler::InitializeInternal() {
  segment_info_.resize(num_input_streams());
  subsegment_info_.resize(num_input_streams());
  time_scales_.resize(num_input_streams());
  last_sample_end_timestamps_.resize(num_input_streams());
  return Status::OK;
}

Status ChunkingHandler::Process(std::unique_ptr<StreamData> stream_data) {
  switch (stream_data->stream_data_type) {
    case StreamDataType::kStreamInfo: {
      // Make sure the inputs come from the same thread.
      const int64_t thread_id =
          static_cast<int64_t>(base::PlatformThread::CurrentId());
      int64_t expected = kThreadIdUnset;
      if (!thread_id_.compare_exchange_strong(expected, thread_id) &&
          expected != thread_id) {
        return Status(error::CHUNKING_ERROR,
                      "Inputs should come from the same thread.");
      }

      const auto time_scale = stream_data->stream_info->time_scale();
      // The video stream is treated as the main stream. If there is only one
      // stream, it is the main stream.
      const bool is_main_stream =
          main_stream_index_ == -1 &&
          (stream_data->stream_info->stream_type() == kStreamVideo ||
           num_input_streams() == 1);
      if (is_main_stream) {
        main_stream_index_ = stream_data->stream_index;
        segment_duration_ =
            chunking_options_.segment_duration_in_seconds * time_scale;
        subsegment_duration_ =
            chunking_options_.subsegment_duration_in_seconds * time_scale;
      } else if (stream_data->stream_info->stream_type() == kStreamVideo) {
        return Status(error::CHUNKING_ERROR,
                      "Only one video stream is allowed per chunking handler.");
      }
      time_scales_[stream_data->stream_index] = time_scale;
      break;
    }
    case StreamDataType::kSegmentInfo:
      VLOG(3) << "Drop existing segment info.";
      return Status::OK;
    case StreamDataType::kMediaSample: {
      const int stream_index = stream_data->stream_index;
      DCHECK_NE(time_scales_[stream_index], 0u)
          << "kStreamInfo should arrive before kMediaSample";
      if (stream_index != main_stream_index_) {
        if (!stream_data->media_sample->is_key_frame()) {
          return Status(error::CHUNKING_ERROR,
                        "All non video samples should be key frames.");
        }
        // Cache non main stream samples, since we don't know yet whether these
        // samples belong to the current or next segment.
        non_main_samples_.push_back(std::move(stream_data));
        // The streams are expected to be synchronized, so we don't expect to
        // see a lot of samples before seeing video samples.
        const size_t kMaxSamplesPerStreamBeforeVideoSample = 5u;
        if (non_main_samples_.size() >
            num_input_streams() * kMaxSamplesPerStreamBeforeVideoSample) {
          return Status(error::CHUNKING_ERROR,
                        "Too many non video samples before video sample.");
        }
        return Status::OK;
      }

      const MediaSample* sample = stream_data->media_sample.get();
      Status status = ProcessMediaSample(sample);
      if (!status.ok())
        return status;
      // Discard samples before segment start.
      if (!segment_info_[stream_index])
        return Status::OK;
      last_sample_end_timestamps_[stream_index] =
          sample->dts() + sample->duration();
      break;
    }
    default:
      VLOG(3) << "Stream data type "
              << static_cast<int>(stream_data->stream_data_type) << " ignored.";
      break;
  }
  return Dispatch(std::move(stream_data));
}

Status ChunkingHandler::FlushStream(int input_stream_index) {
  if (segment_info_[input_stream_index]) {
    Status status;
    if (input_stream_index != main_stream_index_) {
      status = DispatchNonMainSamples(kTimeStampToDispatchAllSamples);
      if (!status.ok())
        return status;
    }
    auto& segment_info = segment_info_[input_stream_index];
    if (segment_info->start_timestamp != -1) {
      segment_info->duration = last_sample_end_timestamps_[input_stream_index] -
                               segment_info->start_timestamp;
      status = DispatchSegmentInfo(input_stream_index, std::move(segment_info));
      if (!status.ok())
        return status;
    }
  }
  return MediaHandler::FlushStream(input_stream_index);
}

Status ChunkingHandler::ProcessMediaSample(const MediaSample* sample) {
  const bool is_key_frame = sample->is_key_frame();
  const int64_t timestamp = sample->dts();
  // Check if we need to terminate the current (sub)segment.
  bool new_segment = false;
  bool new_subsegment = false;
  if (is_key_frame || !chunking_options_.segment_sap_aligned) {
    const int64_t segment_index = timestamp / segment_duration_;
    if (segment_index != current_segment_index_) {
      current_segment_index_ = segment_index;
      new_segment = true;
    }
  }
  if (!new_segment && subsegment_duration_ > 0 &&
      (is_key_frame || !chunking_options_.subsegment_sap_aligned)) {
    const int64_t subsegment_index =
        (timestamp - segment_info_[main_stream_index_]->start_timestamp) /
        subsegment_duration_;
    if (subsegment_index != current_subsegment_index_) {
      current_subsegment_index_ = subsegment_index;
      new_subsegment = true;
    }
  }

  Status status;
  if (new_segment || new_subsegment) {
    // Dispatch the samples before |timestamp| - See the implemention on how we
    // determine if a sample is before |timestamp|..
    status.Update(DispatchNonMainSamples(timestamp));
  }

  if (new_segment) {
    status.Update(DispatchSegmentInfoForAllStreams());
    segment_info_[main_stream_index_]->start_timestamp = timestamp;
  }
  if (subsegment_duration_ > 0 && (new_segment || new_subsegment)) {
    status.Update(DispatchSubsegmentInfoForAllStreams());
    subsegment_info_[main_stream_index_]->start_timestamp = timestamp;
  }
  if (!status.ok())
    return status;

  // Dispatch non-main samples for the next segment.
  return DispatchNonMainSamples(kTimeStampToDispatchAllSamples);
}

Status ChunkingHandler::DispatchNonMainSamples(int64_t timestamp_threshold) {
  Status status;
  while (status.ok() && !non_main_samples_.empty()) {
    DCHECK_EQ(non_main_samples_.front()->stream_data_type,
              StreamDataType::kMediaSample);
    const int stream_index = non_main_samples_.front()->stream_index;
    const MediaSample* sample = non_main_samples_.front()->media_sample.get();
    // If the portion of the sample before |timestamp_threshold| is bigger than
    // the other portion, we consider it part of the current segment.
    const int64_t timestamp = sample->dts() + sample->duration() / 2;
    const bool stop =
        (timestamp_threshold != kTimeStampToDispatchAllSamples &&
         (static_cast<double>(timestamp) / time_scales_[stream_index]) >
             (static_cast<double>(timestamp_threshold) /
              time_scales_[main_stream_index_]));
    VLOG(3) << "Sample ts: " << sample->dts() << " "
            << " duration: " << sample->duration()
            << " scale: " << time_scales_[stream_index] << "\n"
            << " threshold: " << timestamp_threshold
            << " scale: " << time_scales_[main_stream_index_]
            << (stop ? " stop "
                     : (segment_info_[stream_index] ? " dispatch "
                                                    : " discard "));
    if (stop)
      break;
    // Only dispatch samples if the segment has started, otherwise discard
    // them.
    if (segment_info_[stream_index]) {
      if (segment_info_[stream_index]->start_timestamp == -1)
        segment_info_[stream_index]->start_timestamp = sample->dts();
      if (subsegment_info_[stream_index] &&
          subsegment_info_[stream_index]->start_timestamp == -1) {
        subsegment_info_[stream_index]->start_timestamp = sample->dts();
      }
      last_sample_end_timestamps_[stream_index] =
          sample->dts() + sample->duration();
      status.Update(Dispatch(std::move(non_main_samples_.front())));
    }
    non_main_samples_.pop_front();
  }
  return status;
}

Status ChunkingHandler::DispatchSegmentInfoForAllStreams() {
  Status status;
  for (int i = 0; i < static_cast<int>(segment_info_.size()) && status.ok();
       ++i) {
    if (segment_info_[i] && segment_info_[i]->start_timestamp != -1) {
      segment_info_[i]->duration =
          last_sample_end_timestamps_[i] - segment_info_[i]->start_timestamp;
      status.Update(DispatchSegmentInfo(i, std::move(segment_info_[i])));
    }
    segment_info_[i].reset(new SegmentInfo);
    subsegment_info_[i].reset();
  }
  return status;
}

Status ChunkingHandler::DispatchSubsegmentInfoForAllStreams() {
  Status status;
  for (int i = 0; i < static_cast<int>(subsegment_info_.size()) && status.ok();
       ++i) {
    if (subsegment_info_[i] && subsegment_info_[i]->start_timestamp != -1) {
      subsegment_info_[i]->duration =
          last_sample_end_timestamps_[i] - subsegment_info_[i]->start_timestamp;
      status.Update(DispatchSegmentInfo(i, std::move(subsegment_info_[i])));
    }
    subsegment_info_[i].reset(new SegmentInfo);
    subsegment_info_[i]->is_subsegment = true;
  }
  return status;
}

}  // namespace media
}  // namespace shaka
