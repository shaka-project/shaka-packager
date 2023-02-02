// Copyright 2017 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/formats/webvtt/webvtt_to_mp4_handler.h"

#include <algorithm>
#include <map>

#include "packager/media/base/buffer_writer.h"
#include "packager/media/formats/mp4/box_buffer.h"
#include "packager/media/formats/mp4/box_definitions.h"
#include "packager/media/formats/webvtt/webvtt_utils.h"
#include "packager/status_macros.h"

namespace shaka {
namespace media {
namespace {
size_t kTrackId = 0;

enum class DisplayActionType { ADD, REMOVE };

struct DisplayAction {
  DisplayActionType type;
  const TextSample* sample;
};

std::multimap<int64_t, DisplayAction> CreateActionList(
    int64_t segment_start,
    int64_t segment_end,
    const std::list<std::shared_ptr<const TextSample>>& samples) {
  std::multimap<int64_t, DisplayAction> actions;

  for (const auto& sample : samples) {
    DCHECK(sample);

    // The add action should occur either in this segment or in a previous
    // segment.
    DCHECK_LT(sample->start_time(), segment_end);
    actions.insert(
        {sample->start_time(), {DisplayActionType::ADD, sample.get()}});

    // If the remove happens in a later segment, then we don't want to include
    // that action.
    if (sample->EndTime() < segment_end) {
      actions.insert(
          {sample->EndTime(), {DisplayActionType::REMOVE, sample.get()}});
    }
  }

  return actions;
}

void WriteSample(const TextSample& sample, BufferWriter* out) {
  mp4::VTTCueBox box;

  if (sample.id().length()) {
    box.cue_id.cue_id = sample.id();
  }
  box.cue_settings.settings = WebVttSettingsToString(sample.settings());
  box.cue_payload.cue_text = WebVttFragmentToString(sample.body());

  // If there is internal timing, i.e. WebVTT cue timestamp, then
  // cue_current_time should be populated
  // "which gives the VTT timestamp associated with the start time of sample."
  // TODO(rkuroiwa): Reuse TimestampToMilliseconds() to check if there is an
  // internal timestamp in the payload to set CueTimeBox.cue_current_time.
  box.Write(out);
}

void WriteSamples(const std::list<const TextSample*>& samples,
                  BufferWriter* writer) {
  DCHECK_GE(samples.size(), 0u);

  for (const auto& sample : samples) {
    WriteSample(*sample, writer);
  }
}

void WriteEmptySample(BufferWriter* writer) {
  mp4::VTTEmptyCueBox box;
  box.Write(writer);
}

std::shared_ptr<MediaSample> CreateMediaSample(const BufferWriter& buffer,
                                               int64_t start_time,
                                               int64_t end_time) {
  DCHECK_GE(start_time, 0);
  DCHECK_GT(end_time, start_time);

  const bool kIsKeyFrame = true;

  std::shared_ptr<MediaSample> sample =
      MediaSample::CopyFrom(buffer.Buffer(), buffer.Size(), kIsKeyFrame);
  sample->set_pts(start_time);
  sample->set_dts(start_time);
  sample->set_duration(end_time - start_time);

  return sample;
}
}  // namespace

Status WebVttToMp4Handler::InitializeInternal() {
  return Status::OK;
}

Status WebVttToMp4Handler::Process(std::unique_ptr<StreamData> stream_data) {
  switch (stream_data->stream_data_type) {
    case StreamDataType::kStreamInfo:
      return OnStreamInfo(std::move(stream_data));
    case StreamDataType::kCueEvent:
      return OnCueEvent(std::move(stream_data));
    case StreamDataType::kSegmentInfo:
      return OnSegmentInfo(std::move(stream_data));
    case StreamDataType::kTextSample:
      return OnTextSample(std::move(stream_data));
    default:
      return Status(error::INTERNAL_ERROR,
                    "Invalid stream data type (" +
                        StreamDataTypeToString(stream_data->stream_data_type) +
                        ") for this WebVttToMp4 handler");
  }
}

Status WebVttToMp4Handler::OnStreamInfo(
    std::unique_ptr<StreamData> stream_data) {
  DCHECK(stream_data);
  DCHECK(stream_data->stream_info);

  auto clone = stream_data->stream_info->Clone();
  clone->set_codec(kCodecWebVtt);
  clone->set_codec_string("wvtt");

  if (clone->stream_type() != kStreamText) {
    return Status(error::MUXER_FAILURE, "Incorrect stream type");
  }

  return Dispatch(
      StreamData::FromStreamInfo(stream_data->stream_index, std::move(clone)));
}

Status WebVttToMp4Handler::OnCueEvent(std::unique_ptr<StreamData> stream_data) {
  DCHECK(stream_data);
  DCHECK(stream_data->cue_event);

  if (current_segment_.size()) {
    return Status(error::INTERNAL_ERROR,
                  "Cue Events should come right after segment info.");
  }

  return Dispatch(std::move(stream_data));
}

Status WebVttToMp4Handler::OnSegmentInfo(
    std::unique_ptr<StreamData> stream_data) {
  DCHECK(stream_data);
  DCHECK(stream_data->segment_info);

  const auto& segment = stream_data->segment_info;

  int64_t segment_start = segment->start_timestamp;
  int64_t segment_duration = segment->duration;
  int64_t segment_end = segment_start + segment_duration;

  RETURN_IF_ERROR(DispatchCurrentSegment(segment_start, segment_end));
  current_segment_.clear();

  return Dispatch(std::move(stream_data));
}

Status WebVttToMp4Handler::OnTextSample(
    std::unique_ptr<StreamData> stream_data) {
  DCHECK(stream_data);
  DCHECK(stream_data->text_sample);

  auto& sample = stream_data->text_sample;

  // Ignore empty samples. This will create gaps, but we will handle that
  // later.
  if (sample->body().is_empty()) {
    return Status::OK;
  }

  // Add the new text sample to the cache of samples that belong in the
  // current segment.
  current_segment_.push_back(std::move(stream_data->text_sample));
  return Status::OK;
}

Status WebVttToMp4Handler::DispatchCurrentSegment(int64_t segment_start,
                                                  int64_t segment_end) {
  // Active will hold all the samples that are "on screen" for the current
  // section of time.
  std::list<const TextSample*> active;

  // Move through the segment, jumping between each change to the current state.
  // A change is defined as a group of one or more DisplayActions.
  int section_start = segment_start;

  // |actions| is a map of [time] -> [action].
  auto actions = CreateActionList(segment_start, segment_end, current_segment_);
  auto front = actions.begin();

  // As it is possible to have a segment with no samples, we can't base this
  // loop on the number of actions. So we need to keep iterating until we
  // have written enough sections to get to the end of the segment.
  while (section_start < segment_end) {
    // Apply all actions that occur at the start of this part of the segment.
    // Normally we would only want "== section_start" but as it is possible for
    // samples to span multiple segments, their start time will be before the
    // segment's start time. So we want to apply them too if they come before
    // the segment. Thus why we use "<=".
    while (front != actions.end() && front->first <= section_start) {
      auto& action = front->second;

      switch (action.type) {
        case DisplayActionType::ADD: {
          active.push_back(action.sample);
          break;
        }
        case DisplayActionType::REMOVE: {
          auto found = std::find(active.begin(), active.end(), action.sample);
          DCHECK(found != active.end());
          active.erase(found);
          break;
        }
        default: {
          NOTREACHED() << "Unsupported DisplayActionType "
                       << static_cast<int>(action.type);
          break;
        }
      }

      // We have "consumed" the action at the front. We can move on.
      front++;
    }

    // The end of the section will either be the start of the next section or
    // the end of the segment.
    int64_t section_end = front == actions.end() ? segment_end : front->first;
    DCHECK_GT(section_end, section_start);
    DCHECK_LE(section_end, segment_end);
    RETURN_IF_ERROR(MergeDispatchSamples(section_start, section_end, active));

    section_start = section_end;
  }

  DCHECK(front == actions.end()) << "We should have processed all actions.";

  return Status::OK;
}

Status WebVttToMp4Handler::MergeDispatchSamples(
    int64_t start_time,
    int64_t end_time,
    const std::list<const TextSample*>& state) {
  DCHECK_GT(end_time, start_time);

  box_writer_.Clear();

  if (state.size()) {
    WriteSamples(state, &box_writer_);
  } else {
    WriteEmptySample(&box_writer_);
  }

  return DispatchMediaSample(
      kTrackId, CreateMediaSample(box_writer_, start_time, end_time));
}
}  // namespace media
}  // namespace shaka
