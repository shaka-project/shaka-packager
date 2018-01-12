// Copyright 2017 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/chunking/chunking_handler.h"

#include <algorithm>

#include "packager/base/logging.h"
#include "packager/base/threading/platform_thread.h"
#include "packager/media/base/media_sample.h"

namespace {
int64_t kThreadIdUnset = -1;
}  // namespace

namespace shaka {
namespace media {

ChunkingHandler::ChunkingHandler(const ChunkingParams& chunking_params)
    : chunking_params_(chunking_params),
      thread_id_(kThreadIdUnset),
      media_sample_comparator_(this),
      cached_media_sample_stream_data_(media_sample_comparator_) {
  CHECK_NE(chunking_params.segment_duration_in_seconds, 0u);
}

ChunkingHandler::~ChunkingHandler() {}

Status ChunkingHandler::InitializeInternal() {
  segment_info_.resize(num_input_streams());
  subsegment_info_.resize(num_input_streams());
  time_scales_.resize(num_input_streams());
  last_sample_end_timestamps_.resize(num_input_streams());
  num_cached_samples_.resize(num_input_streams());
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
          main_stream_index_ == kInvalidStreamIndex &&
          (stream_data->stream_info->stream_type() == kStreamVideo ||
           num_input_streams() == 1);
      if (is_main_stream) {
        main_stream_index_ = stream_data->stream_index;
        segment_duration_ =
            chunking_params_.segment_duration_in_seconds * time_scale;
        subsegment_duration_ =
            chunking_params_.subsegment_duration_in_seconds * time_scale;
      } else if (stream_data->stream_info->stream_type() == kStreamVideo) {
        return Status(error::CHUNKING_ERROR,
                      "Only one video stream is allowed per chunking handler.");
      }
      time_scales_[stream_data->stream_index] = time_scale;
      break;
    }
    case StreamDataType::kScte35Event: {
      if (stream_data->stream_index != main_stream_index_) {
        VLOG(3) << "Dropping scte35 event from non main stream.";
        return Status::OK;
      }
      scte35_events_.push(std::move(stream_data));
      return Status::OK;
    }
    case StreamDataType::kSegmentInfo:
      VLOG(3) << "Droppping existing segment info.";
      return Status::OK;
    case StreamDataType::kMediaSample: {
      const size_t stream_index = stream_data->stream_index;
      DCHECK_NE(time_scales_[stream_index], 0u)
          << "kStreamInfo should arrive before kMediaSample";

      if (stream_index != main_stream_index_ &&
          !stream_data->media_sample->is_key_frame()) {
        return Status(error::CHUNKING_ERROR,
                      "All non video samples should be key frames.");
      }
      // The streams are expected to be roughly synchronized, so we don't expect
      // to see a lot of samples from one stream but no samples from another
      // stream.
      // The value is kind of arbitrary here. For a 24fps video, it is ~40s.
      const size_t kMaxCachedSamplesPerStream = 1000u;
      if (num_cached_samples_[stream_index] >= kMaxCachedSamplesPerStream) {
        LOG(ERROR) << "Streams are not synchronized:";
        for (size_t i = 0; i < num_cached_samples_.size(); ++i)
          LOG(ERROR) << " [Stream " << i << "] " << num_cached_samples_[i];
        return Status(error::CHUNKING_ERROR, "Streams are not synchronized.");
      }

      cached_media_sample_stream_data_.push(std::move(stream_data));
      ++num_cached_samples_[stream_index];

      // If we have cached samples from every stream, the first sample in
      // |cached_media_samples_stream_data_| is guaranteed to be the earliest
      // sample. Extract and process that sample.
      if (std::all_of(num_cached_samples_.begin(), num_cached_samples_.end(),
                      [](size_t num_samples) { return num_samples > 0; })) {
        while (true) {
          const size_t top_stream_index =
              cached_media_sample_stream_data_.top()->stream_index;
          Status status = ProcessMediaSampleStreamData(
              *cached_media_sample_stream_data_.top());
          if (!status.ok())
            return status;
          cached_media_sample_stream_data_.pop();
          if (--num_cached_samples_[top_stream_index] == 0)
            break;
        }
      }
      return Status::OK;
    }
    default:
      VLOG(3) << "Stream data type "
              << static_cast<int>(stream_data->stream_data_type) << " ignored.";
      break;
  }
  return Dispatch(std::move(stream_data));
}

Status ChunkingHandler::OnFlushRequest(size_t input_stream_index) {
  // Process all cached samples.
  while (!cached_media_sample_stream_data_.empty()) {
    Status status =
        ProcessMediaSampleStreamData(*cached_media_sample_stream_data_.top());
    if (!status.ok())
      return status;
    --num_cached_samples_[cached_media_sample_stream_data_.top()->stream_index];
    cached_media_sample_stream_data_.pop();
  }
  if (segment_info_[input_stream_index]) {
    auto& segment_info = segment_info_[input_stream_index];
    if (segment_info->start_timestamp != -1) {
      segment_info->duration = last_sample_end_timestamps_[input_stream_index] -
                               segment_info->start_timestamp;
      Status status =
          DispatchSegmentInfo(input_stream_index, std::move(segment_info));
      if (!status.ok())
        return status;
    }
  }
  const size_t output_stream_index = input_stream_index;
  return FlushDownstream(output_stream_index);
}

Status ChunkingHandler::ProcessMainMediaSample(const MediaSample* sample) {
  const bool is_key_frame = sample->is_key_frame();
  const int64_t timestamp = sample->dts();
  // Check if we need to terminate the current (sub)segment.
  bool new_segment = false;
  bool new_subsegment = false;
  std::shared_ptr<CueEvent> cue_event;
  if (is_key_frame || !chunking_params_.segment_sap_aligned) {
    const int64_t segment_index = timestamp / segment_duration_;
    if (segment_index != current_segment_index_) {
      current_segment_index_ = segment_index;
      // Reset subsegment index.
      current_subsegment_index_ = 0;
      new_segment = true;
    }
    // We use 'while' instead of 'if' to make sure to pop off multiple SCTE35
    // events that may be very close to each other.
    while (!scte35_events_.empty() &&
           (scte35_events_.top()->scte35_event->start_time <= timestamp)) {
      // For simplicity, don't change |current_segment_index_|.
      current_subsegment_index_ = 0;
      new_segment = true;

      cue_event = std::make_shared<CueEvent>();
      // Use PTS instead of DTS for cue event timestamp.
      cue_event->timestamp = sample->pts();
      cue_event->cue_data = scte35_events_.top()->scte35_event->cue_data;
      LOG(INFO) << "Chunked at " << timestamp << " for Ad Cue.";

      scte35_events_.pop();
    }
  }
  if (!new_segment && subsegment_duration_ > 0 &&
      (is_key_frame || !chunking_params_.subsegment_sap_aligned)) {
    const int64_t subsegment_index =
        (timestamp - segment_info_[main_stream_index_]->start_timestamp) /
        subsegment_duration_;
    if (subsegment_index != current_subsegment_index_) {
      current_subsegment_index_ = subsegment_index;
      new_subsegment = true;
    }
  }

  Status status;
  if (new_segment) {
    status.Update(DispatchSegmentInfoForAllStreams());
    segment_info_[main_stream_index_]->start_timestamp = timestamp;

    if (cue_event)
      status.Update(DispatchCueEventForAllStreams(std::move(cue_event)));
  }
  if (subsegment_duration_ > 0 && (new_segment || new_subsegment)) {
    status.Update(DispatchSubsegmentInfoForAllStreams());
    subsegment_info_[main_stream_index_]->start_timestamp = timestamp;
  }
  return status;
}

Status ChunkingHandler::ProcessMediaSampleStreamData(
    const StreamData& media_sample_stream_data) {
  const size_t stream_index = media_sample_stream_data.stream_index;
  const auto sample = std::move(media_sample_stream_data.media_sample);

  if (stream_index == main_stream_index_) {
    Status status = ProcessMainMediaSample(sample.get());
    if (!status.ok())
      return status;
  }

  VLOG(3) << "Stream index: " << stream_index << " "
          << "Sample ts: " << sample->dts() << " "
          << " duration: " << sample->duration()
          << " scale: " << time_scales_[stream_index] << "\n"
          << " scale: " << time_scales_[main_stream_index_]
          << (segment_info_[stream_index] ? " dispatch " : " discard ");
  // Discard samples before segment start. If the segment has started,
  // |segment_info_[stream_index]| won't be null.
  if (!segment_info_[stream_index])
    return Status::OK;
  if (segment_info_[stream_index]->start_timestamp == -1)
    segment_info_[stream_index]->start_timestamp = sample->dts();
  if (subsegment_info_[stream_index] &&
      subsegment_info_[stream_index]->start_timestamp == -1) {
    subsegment_info_[stream_index]->start_timestamp = sample->dts();
  }
  last_sample_end_timestamps_[stream_index] =
      sample->dts() + sample->duration();
  return DispatchMediaSample(stream_index, std::move(sample));
}

Status ChunkingHandler::DispatchSegmentInfoForAllStreams() {
  Status status;
  for (size_t i = 0; i < segment_info_.size() && status.ok(); ++i) {
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
  for (size_t i = 0; i < subsegment_info_.size() && status.ok(); ++i) {
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

Status ChunkingHandler::DispatchCueEventForAllStreams(
    std::shared_ptr<CueEvent> cue_event) {
  Status status;
  for (size_t i = 0; i < segment_info_.size() && status.ok(); ++i) {
    std::shared_ptr<CueEvent> new_cue_event(new CueEvent(*cue_event));
    new_cue_event->timestamp = cue_event->timestamp * time_scales_[i] /
                               time_scales_[main_stream_index_];
    status.Update(DispatchCueEvent(i, std::move(new_cue_event)));
  }
  return status;
}

ChunkingHandler::MediaSampleTimestampGreater::MediaSampleTimestampGreater(
    const ChunkingHandler* const chunking_handler)
    : chunking_handler_(chunking_handler) {}

bool ChunkingHandler::MediaSampleTimestampGreater::operator()(
    const std::unique_ptr<StreamData>& lhs,
    const std::unique_ptr<StreamData>& rhs) const {
  DCHECK(lhs);
  DCHECK(rhs);
  return GetSampleTimeInSeconds(*lhs) > GetSampleTimeInSeconds(*rhs);
}

double ChunkingHandler::MediaSampleTimestampGreater::GetSampleTimeInSeconds(
    const StreamData& media_sample_stream_data) const {
  const size_t stream_index = media_sample_stream_data.stream_index;
  const auto& sample = media_sample_stream_data.media_sample;
  DCHECK(sample);
  // Order main samples by left boundary and non main samples by mid-point. This
  // ensures non main samples are properly chunked, i.e. if the portion of the
  // sample in the next chunk is bigger than the portion of the sample in the
  // previous chunk, the sample is placed in the next chunk.
  const uint64_t timestamp =
      stream_index == chunking_handler_->main_stream_index_
          ? sample->dts()
          : (sample->dts() + sample->duration() / 2);
  return static_cast<double>(timestamp) /
         chunking_handler_->time_scales_[stream_index];
}

bool ChunkingHandler::Scte35EventTimestampGreater::operator()(
    const std::unique_ptr<StreamData>& lhs,
    const std::unique_ptr<StreamData>& rhs) const {
  DCHECK(lhs);
  DCHECK(rhs);
  DCHECK(lhs->scte35_event);
  DCHECK(rhs->scte35_event);
  return lhs->scte35_event->start_time > rhs->scte35_event->start_time;
}

}  // namespace media
}  // namespace shaka
