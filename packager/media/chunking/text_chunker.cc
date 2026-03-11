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
      ts_ttx_heartbeat_shift_(kDefaultTtxHeartbeatShift),
      use_segment_coordinator_(false){};

TextChunker::TextChunker(double segment_duration_in_seconds,
                         int64_t start_segment_number,
                         int64_t ts_ttx_heartbeat_shift)
    : segment_duration_in_seconds_(segment_duration_in_seconds),
      segment_number_(start_segment_number),
      ts_ttx_heartbeat_shift_(ts_ttx_heartbeat_shift),
      use_segment_coordinator_(false){};

TextChunker::TextChunker(double segment_duration_in_seconds,
                         int64_t start_segment_number,
                         int64_t ts_ttx_heartbeat_shift,
                         bool use_segment_coordinator)
    : segment_duration_in_seconds_(segment_duration_in_seconds),
      segment_number_(start_segment_number),
      ts_ttx_heartbeat_shift_(ts_ttx_heartbeat_shift),
      use_segment_coordinator_(use_segment_coordinator){};

Status TextChunker::Process(std::unique_ptr<StreamData> data) {
  switch (data->stream_data_type) {
    case StreamDataType::kStreamInfo:
      return OnStreamInfo(std::move(data->stream_info));
    case StreamDataType::kTextSample:
      return OnTextSample(data->text_sample);
    case StreamDataType::kCueEvent:
      return OnCueEvent(data->cue_event);
    case StreamDataType::kSegmentInfo:
      if (use_segment_coordinator_) {
        return OnSegmentInfo(std::move(data->segment_info));
      } else {
        // Pass through for non-teletext streams
        return DispatchSegmentInfo(kStreamIndex, std::move(data->segment_info));
      }
    default:
      return Status(error::INTERNAL_ERROR,
                    "Invalid stream data type for this handler");
  }
}

Status TextChunker::OnFlushRequest(size_t /*input_stream_index*/) {
  // Keep outputting segments until all the samples leave the system. Calling
  // |DispatchSegment| will remove samples over time.
  //
  // In coordinator mode, the final SegmentInfo from video/audio should have
  // already triggered the last segment dispatch with the correct duration.
  // This loop handles any remaining samples (edge cases or non-coordinator
  // mode).
  while (samples_in_current_segment_.size()) {
    if (segment_start_ < 0) {
      // No segments were ever started - nothing to flush
      break;
    }
    int64_t segment_end = segment_start_ + segment_duration_;
    AddOngoingCuesToCurrentSegment(segment_end);
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
  // start time. In coordinator mode, we wait for SegmentInfo to initialize
  // segment_start_ so that we align with video/audio boundaries.
  if (segment_start_ < 0 && !use_segment_coordinator_) {
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
    // Use SignedPtsDiff for wrap-safe comparison
    if (PtsIsBefore(sample_start, latest_media_heartbeat_time_)) {
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
  //
  // In coordinator mode, we skip this entirely - segment dispatch is driven
  // by OnSegmentInfo which receives actual video/audio segment boundaries.
  // This ensures text segments align perfectly with video/audio segments.
  if (!use_segment_coordinator_ && segment_start_ >= 0) {
    int64_t segment_end = segment_start_ + segment_duration_;
    while (!PtsIsBefore(sample_start, segment_end)) {
      // Add cropped copies of ongoing cues before dispatching
      AddOngoingCuesToCurrentSegment(segment_end);
      // |DispatchSegment| will advance |segment_start_|.
      RETURN_IF_ERROR(DispatchSegment(segment_duration_));
      segment_end = segment_start_ + segment_duration_;
    }
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

Status TextChunker::OnSegmentInfo(std::shared_ptr<const SegmentInfo> info) {
  DCHECK(use_segment_coordinator_)
      << "OnSegmentInfo should only be called when coordinator mode is enabled";

  // Skip subsegments - only align on full segments
  if (info->is_subsegment) {
    DVLOG(3) << "TextChunker: Skipping subsegment SegmentInfo";
    return Status::OK;
  }

  // Use start_timestamp + duration as the end boundary. This ensures we
  // dispatch the segment that just completed, not wait for the next
  // SegmentInfo. Without this, the final segment would never be dispatched
  // since there's no subsequent SegmentInfo to trigger it.
  int64_t segment_end_boundary = info->start_timestamp + info->duration;

  DVLOG(2) << "TextChunker received SegmentInfo: start="
           << info->start_timestamp << " duration=" << info->duration
           << " end_boundary=" << segment_end_boundary
           << " (current segment_start_=" << segment_start_ << ")";

  // If this is the first segment info, initialize segment_start_
  if (segment_start_ < 0) {
    segment_start_ = info->start_timestamp;
    DVLOG(2) << "TextChunker: Initialized segment_start_ from SegmentInfo: "
             << segment_start_;
  }

  // Handle PTS wrap-around: if segment_end_boundary appears to be earlier than
  // segment_start_ by more than half the 33-bit PTS range, it's likely
  // wrapped around and is actually later.
  // 33-bit PTS wraps at 2^33 = 8,589,934,592 ticks (~26.5 hours @ 90kHz)
  // Half range = ~13 hours = 4,294,967,296 ticks
  const int64_t kPtsWrapThreshold = 4294967296LL;  // Half of 2^33

  if (segment_end_boundary < segment_start_) {
    int64_t diff = segment_start_ - segment_end_boundary;
    if (diff > kPtsWrapThreshold) {
      // This looks like a wrap-around - the boundary has wrapped but our
      // segment_start_ hasn't yet. Treat this boundary as being later.
      DVLOG(2) << "TextChunker: Detected PTS wrap-around. End boundary "
               << segment_end_boundary
               << " appears earlier than segment_start_ " << segment_start_
               << " by " << diff << " ticks, but is likely "
               << "later due to wrap-around.";

      // Dispatch one final segment before the wrap and align to the boundary
      int64_t segment_end = segment_start_ + segment_duration_;
      AddOngoingCuesToCurrentSegment(segment_end);
      RETURN_IF_ERROR(DispatchSegment(segment_duration_));
      segment_start_ = info->start_timestamp;
    } else {
      // End boundary is genuinely earlier - this shouldn't happen in normal
      // flow but we'll log a warning and skip this boundary
      LOG(WARNING) << "TextChunker: Received SegmentInfo end boundary "
                   << segment_end_boundary << " that is earlier than current "
                   << "segment_start_ " << segment_start_ << " (diff: " << diff
                   << "). Skipping this SegmentInfo.";
      return Status::OK;
    }
  }

  // Dispatch all pending segments up to the end boundary
  while (segment_start_ < segment_end_boundary) {
    int64_t segment_end = segment_start_ + segment_duration_;

    // If the next calculated segment would go past the end boundary,
    // dispatch a shorter segment to align with the actual boundary
    if (segment_end > segment_end_boundary) {
      int64_t adjusted_duration = segment_end_boundary - segment_start_;
      if (adjusted_duration > 0) {
        DVLOG(3) << "TextChunker: Dispatching adjusted segment to align with "
                 << "end boundary. Duration: " << adjusted_duration
                 << " (normal: " << segment_duration_ << ")";
        // Add ongoing cues before dispatching (use end boundary as end)
        AddOngoingCuesToCurrentSegment(segment_end_boundary);
        RETURN_IF_ERROR(DispatchSegment(adjusted_duration));
      }
      break;
    }

    // Dispatch a full-duration segment
    DVLOG(3) << "TextChunker: Dispatching full segment aligned to end boundary";
    // Add ongoing cues before dispatching
    AddOngoingCuesToCurrentSegment(segment_end);
    RETURN_IF_ERROR(DispatchSegment(segment_duration_));
  }

  // Align next segment start to end boundary (start of next segment)
  segment_start_ = segment_end_boundary;

  return Status::OK;
}

Status TextChunker::DispatchSegment(int64_t duration) {
  DCHECK_GT(duration, 0) << "Segment duration should always be positive";

  int64_t segment_end = segment_start_ + duration;

  // Output only the samples that actually belong in this segment.
  // Use wrap-safe comparison since samples may have wrapped PTS.
  DVLOG(1) << "DispatchSegment, start=" << segment_start_
           << " end=" << segment_end;
  for (const auto& sample : samples_in_current_segment_) {
    // Only dispatch if sample starts before segment end
    if (PtsIsBefore(sample->start_time(), segment_end)) {
      DVLOG(2) << "DispatchTextSample, pts=" << sample->start_time()
               << " end=" << sample->EndTime();
      RETURN_IF_ERROR(DispatchTextSample(kStreamIndex, sample));
    } else {
      DVLOG(2) << "Skipping sample pts=" << sample->start_time()
               << " (after segment_end=" << segment_end << ")";
    }
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

  // Remove all samples that end before the (new) current segment started.
  // Use wrap-safe comparison.
  samples_in_current_segment_.remove_if(
      [new_segment_start](const std::shared_ptr<const TextSample>& sample) {
        // Remove if sample ends before or at new segment start (wrap-safe)
        return PtsIsBeforeOrEqual(sample->EndTime(), new_segment_start);
      });

  return Status::OK;
}

int64_t TextChunker::ScaleTime(double seconds) const {
  DCHECK_GT(time_scale_, 0) << "Need positive time scale to scale time.";
  return static_cast<int64_t>(seconds * time_scale_);
}

void TextChunker::AddOngoingCuesToCurrentSegment(int64_t segment_end) {
  // For each ongoing cue (started but no end time yet), create a cropped
  // copy that ends at the segment boundary and add to current segment.
  for (const auto& s : samples_without_end_) {
    if (s->role() == TextSampleRole::kCueStart) {
      // Only include if the cue started before this segment ends
      if (PtsIsBefore(s->start_time(), segment_end)) {
        // Crop the start time to segment_start_ if needed
        auto cue_start = s->start_time();
        if (PtsIsBefore(cue_start, segment_start_)) {
          cue_start = segment_start_;
        }
        auto cropped_cue = std::make_shared<TextSample>(
            "", cue_start, segment_end, s->settings(), s->body());
        DVLOG(3) << "AddOngoingCuesToCurrentSegment: cropped cue start="
                 << cue_start << " end=" << segment_end;
        samples_in_current_segment_.push_back(std::move(cropped_cue));
      }
    }
  }
}
}  // namespace media
}  // namespace shaka
