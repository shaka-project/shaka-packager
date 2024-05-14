// Copyright 2017 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/media/chunking/text_chunker.h>

#include <absl/log/check.h>

#include <packager/macros/status.h>

namespace shaka {
namespace media {
namespace {
const size_t kStreamIndex = 0;
}  // namespace

TextChunker::TextChunker(double segment_duration_in_seconds,
                         int64_t start_segment_number)
    : segment_duration_in_seconds_(segment_duration_in_seconds),
      segment_number_(start_segment_number),
      ts_ttx_heartbeat_shift_(kDefaultTtxHeartbeatShift) {};

TextChunker::TextChunker(double segment_duration_in_seconds,
                         int64_t start_segment_number,
                         int64_t ts_ttx_heartbeat_shift)
    : segment_duration_in_seconds_(segment_duration_in_seconds),
      segment_number_(start_segment_number),
      ts_ttx_heartbeat_shift_(ts_ttx_heartbeat_shift) {};

Status TextChunker::Process(std::unique_ptr<StreamData> data) {
  switch (data->stream_data_type) {
    case StreamDataType::kStreamInfo:
      return OnStreamInfo(std::move(data->stream_info));
    case StreamDataType::kTextSample:
      return OnTextSample(data->text_sample);
    case StreamDataType::kCueEvent:
      return OnCueEvent(data->cue_event);
    default:
      return Status(error::INTERNAL_ERROR,
                    "Invalid stream data type for this handler");
  }
}

Status TextChunker::OnFlushRequest(size_t /*input_stream_index*/) {
  // Keep outputting segments until all the samples leave the system. Calling
  // |DispatchSegment| will remove samples over time.
  while (samples_in_current_segment_.size()) {
    RETURN_IF_ERROR(DispatchSegment(segment_duration_));
  }

  return FlushAllDownstreams();
}

Status TextChunker::OnStreamInfo(std::shared_ptr<const StreamInfo> info) {
  time_scale_ = info->time_scale();
  segment_duration_ = ScaleTime(segment_duration_in_seconds_);

  return DispatchStreamInfo(kStreamIndex, std::move(info));
}

Status TextChunker::OnCueEvent(std::shared_ptr<const CueEvent> event) {
  // We are going to end the current segment prematurely using the cue event's
  // time as the new segment end.

  // Because the cue should have been inserted into the stream such that no
  // later sample could start before it does, we know that there should
  // be no later samples starting before the cue event.

  // Convert the event's time to be scaled to the time of each sample.
  const int64_t event_time = ScaleTime(event->time_in_seconds);
  // Output all full segments before the segment that the cue event interrupts.
  while (segment_start_ + segment_duration_ < event_time) {
    RETURN_IF_ERROR(DispatchSegment(segment_duration_));
  }

  const int64_t shorten_duration = event_time - segment_start_;
  RETURN_IF_ERROR(DispatchSegment(shorten_duration));
  return DispatchCueEvent(kStreamIndex, std::move(event));
}

Status TextChunker::OnTextSample(std::shared_ptr<const TextSample> sample) {
  // Output all segments that come before our new sample start_time.
  // However, if role is MediaHeartBeat, remove 2s to avoid premature segment
  // generation.

  int64_t sample_start = sample->start_time();
  const auto role = sample->role();
  DVLOG(2) << "OnTextSample: role=" << static_cast<int>(role)
           << " pts=" << sample_start << " end=" << sample->EndTime()
           << " is_empty=" << sample->is_empty()
           << " sub_stream_index=" << sample->sub_stream_index();

  // If we have not seen a sample yet, base all segments off the first sample's
  // start time.
  if (segment_start_ < 0) {
    // Force the first segment to start at the segment that would have started
    // before the sample. This should allow segments from different streams to
    // align.
    segment_start_ = (sample_start / segment_duration_) * segment_duration_;
    DVLOG(1) << "first segment start=" << segment_start_;
  }

  switch (role) {
    case TextSampleRole::kCue: {
      DVLOG(2) << "PTS=" << sample_start << " cue with end "
               << sample->EndTime();
      break;
    }
    case TextSampleRole::kCueStart: {
      DVLOG(2) << "PTS=" << sample_start << " cue start wo end";
      break;
    }
    case TextSampleRole::kCueEnd: {
      DVLOG(2) << "PTS=" << sample_start << " cue end";
      // Convert any cues without end to full cues (but only once)
      auto end_time = sample->EndTime();
      for (auto s : samples_without_end_) {
        int64_t cue_start = s->start_time();
        if (cue_start < segment_start_) {
          cue_start = segment_start_;
        }
        auto nS =
            std::make_shared<TextSample>("", cue_start, end_time, s->settings(),
                                         s->body(), TextSampleRole::kCue);
        DVLOG(3) << "cue shortened. startTime=" << s->start_time()
                 << " endTime=" << end_time;
        samples_in_current_segment_.push_back(nS);
      }
      samples_without_end_.clear();
      break;
    }
    case TextSampleRole::kTextHeartBeat: {
      break;
    }
    case TextSampleRole::kMediaHeartBeat: {
      sample_start -= ts_ttx_heartbeat_shift_;
      latest_media_heartbeat_time_ = sample_start;
      DVLOG(3) << "PTS=" << sample_start << " media heartbeat";
      break;
    }
    default: {
      // LOG(ERROR) << "Unknown role encountered. pts=" << sample_start;
    }
  }

  if (role != TextSampleRole::kMediaHeartBeat) {
    if (sample_start < latest_media_heartbeat_time_) {
      LOG(WARNING) << "Potentially bad text segment: text pts=" << sample_start
                   << " before latest media pts="
                   << latest_media_heartbeat_time_;
    }
  }

  // To avoid waiting for live teletext cues to get an end time/duration
  // they are triggered with a long fixed duration.
  // Here we should detect such cues and put them in a special list.
  // Once an end cue event with duration comes, we should change the duration
  // to the correct value. If an end of a segment duration is triggered
  // before that, we should split the segment so that the first copy ends
  // at the segment boundary, and the second copy starts at the segment
  // boundary. We could keep the long duration of the second part and
  // use the long duration as an indication that it is a cue which has
  // not yet received its proper end time.

  // We need to write all the segments that would have ended before the new
  // sample started. For segment without end, we check if they have started
  // and if so, make cropped copy that goes to the end.
  // We also crop such a cue at the start if needed.

  while (sample_start >= segment_start_ + segment_duration_) {
    int64_t segment_end = segment_start_ + segment_duration_;
    for (auto s : samples_without_end_) {
      if (s->role() == TextSampleRole::kCueStart) {
        if (s->start_time() < segment_end) {
          // Make a new cue in current segment.
          auto cue_start = s->start_time();
          if (cue_start < segment_start_) {
            cue_start = segment_start_;
          }
          auto nS = std::make_shared<TextSample>("", cue_start, segment_end,
                                                 s->settings(), s->body());
          samples_in_current_segment_.push_back(std::move(nS));
        }
      }
    }
    // |DispatchSegment| will advance |segment_start_|.
    RETURN_IF_ERROR(DispatchSegment(segment_duration_));
  }

  switch (role) {
    case TextSampleRole::kCue: {
      samples_in_current_segment_.push_back(std::move(sample));
      break;
    }
    case TextSampleRole::kCueStart: {
      samples_without_end_.push_back(std::move(sample));
      break;
    }
    default: {
      // Do nothing
    }
  }

  return Status::OK;
}

Status TextChunker::DispatchSegment(int64_t duration) {
  DCHECK_GT(duration, 0) << "Segment duration should always be positive";

  // Output all the samples that are part of the segment.
  DVLOG(1) << "DispatchSegment, start=" << segment_start_
           << " end=" << segment_start_ + duration;
  for (const auto& sample : samples_in_current_segment_) {
    DVLOG(2) << "DispatchTextSample, pts=" << sample->start_time()
             << " end=" << sample->EndTime();
    RETURN_IF_ERROR(DispatchTextSample(kStreamIndex, sample));
  }

  // Output the segment info.
  std::shared_ptr<SegmentInfo> info = std::make_shared<SegmentInfo>();
  info->start_timestamp = segment_start_;
  info->duration = duration;
  info->segment_number = segment_number_++;

  RETURN_IF_ERROR(DispatchSegmentInfo(kStreamIndex, std::move(info)));

  // Move onto the next segment.
  const int64_t new_segment_start = segment_start_ + duration;
  segment_start_ = new_segment_start;
  // DVLOG(3) << "New segment start=" << segment_start_
  //          << " end=" << segment_start_ + duration;

  // Remove all samples that end before the (new) current segment started.
  samples_in_current_segment_.remove_if(
      [new_segment_start](const std::shared_ptr<const TextSample>& sample) {
        // For the sample to even be in this list, it should have started
        // before the (new) current segment.
        DCHECK_LT(sample->start_time(), new_segment_start);
        // auto should_remove = sample->EndTime() <= new_segment_start;
        // DVLOG(3) << "sample start=" << sample->start_time()
        //          << " end=" << sample->EndTime()
        //          << " remove=" << should_remove;
        return sample->EndTime() <= new_segment_start;
      });

  return Status::OK;
}

int64_t TextChunker::ScaleTime(double seconds) const {
  DCHECK_GT(time_scale_, 0) << "Need positive time scale to scale time.";
  return static_cast<int64_t>(seconds * time_scale_);
}
}  // namespace media
}  // namespace shaka
