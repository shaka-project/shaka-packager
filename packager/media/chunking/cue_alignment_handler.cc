// Copyright 2018 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/media/chunking/cue_alignment_handler.h>

#include <algorithm>

#include <absl/log/check.h>

#include <packager/macros/logging.h>
#include <packager/macros/status.h>

namespace shaka {
namespace media {
namespace {
// The max number of samples that are allowed to be buffered before we shutdown
// because there is likely a problem with the content or how the pipeline was
// configured. This is about 20 seconds of buffer for audio with 48kHz.
const size_t kMaxBufferSize = 1000;

int64_t GetScaledTime(const StreamInfo& info, const StreamData& data) {
  DCHECK(data.text_sample || data.media_sample);

  if (data.text_sample) {
    return data.text_sample->start_time();
  }

  if (info.stream_type() == kStreamText) {
    // This class does not support splitting MediaSample at cue points, which is
    // required for text stream. This class expects MediaSample to be converted
    // to TextSample before passing to this class.
    NOTIMPLEMENTED()
        << "A text streams should use text samples, not media samples.";
  }

  if (info.stream_type() == kStreamAudio) {
    // Return the mid-point for audio because if the portion of the sample
    // after the cue point is bigger than the portion of the sample before
    // the cue point, the sample is placed after the cue.
    return data.media_sample->pts() + data.media_sample->duration() / 2;
  }

  DCHECK_EQ(info.stream_type(), kStreamVideo);
  return data.media_sample->pts();
}

double TimeInSeconds(const StreamInfo& info, const StreamData& data) {
  const int64_t scaled_time = GetScaledTime(info, data);
  const int32_t time_scale = info.time_scale();

  return static_cast<double>(scaled_time) / time_scale;
}

double TextEndTimeInSeconds(const StreamInfo& info, const StreamData& data) {
  DCHECK(data.text_sample);

  const int64_t scaled_time = data.text_sample->EndTime();
  const int32_t time_scale = info.time_scale();

  return static_cast<double>(scaled_time) / time_scale;
}

Status GetNextCue(double hint,
                  SyncPointQueue* sync_points,
                  std::shared_ptr<const CueEvent>* out_cue) {
  DCHECK(sync_points);
  DCHECK(out_cue);

  *out_cue = sync_points->GetNext(hint);

  // |*out_cue| will only be null if the job was cancelled.
  return *out_cue ? Status::OK
                  : Status(error::CANCELLED, "SyncPointQueue is cancelled.");
}
}  // namespace

CueAlignmentHandler::CueAlignmentHandler(SyncPointQueue* sync_points)
    : sync_points_(sync_points) {}

Status CueAlignmentHandler::InitializeInternal() {
  sync_points_->AddThread();
  stream_states_.resize(num_input_streams());

  // Get the first hint for the stream. Use a negative hint so that if there is
  // suppose to be a sync point at zero, we will still respect it.
  hint_ = sync_points_->GetHint(-1);

  return Status::OK;
}

Status CueAlignmentHandler::Process(std::unique_ptr<StreamData> data) {
  switch (data->stream_data_type) {
    case StreamDataType::kStreamInfo:
      return OnStreamInfo(std::move(data));
    case StreamDataType::kTextSample:
    case StreamDataType::kMediaSample:
      return OnSample(std::move(data));
    default:
      VLOG(3) << "Dropping unsupported data type "
              << static_cast<int>(data->stream_data_type);
      return Status::OK;
  }
}

Status CueAlignmentHandler::OnFlushRequest(size_t stream_index) {
  stream_states_[stream_index].to_be_flushed = true;

  // We need to wait for all stream to flush before we can flush each stream.
  // This allows cached buffers to be cleared and cues to be properly
  // synchronized and set on all streams.
  for (const StreamState& stream_state : stream_states_) {
    if (!stream_state.to_be_flushed) {
      return Status::OK;
    }
  }

  // Do a once over all the streams to ensure that their states are as we expect
  // them. Video and non-video streams have different allowances here. Video
  // should absolutely have no cues or samples where as non-video streams may
  // have cues or samples.
  for (StreamState& stream : stream_states_) {
    DCHECK(stream.to_be_flushed);

    if (stream.info->stream_type() == kStreamVideo) {
      DCHECK_EQ(stream.samples.size(), 0u)
          << "Video streams should not store samples";
      DCHECK_EQ(stream.cues.size(), 0u)
          << "Video streams should not store cues";
    }
  }

  // It is possible that we did not get all the cues. |hint_| will get updated
  // when we call |UseNextSyncPoint|.
  while (sync_points_->HasMore(hint_)) {
    std::shared_ptr<const CueEvent> next_cue;
    RETURN_IF_ERROR(GetNextCue(hint_, sync_points_, &next_cue));
    RETURN_IF_ERROR(UseNewSyncPoint(std::move(next_cue)));
  }

  // Now that there are new cues, it may be possible to dispatch some of the
  // samples that may be left waiting.
  for (StreamState& stream : stream_states_) {
    RETURN_IF_ERROR(RunThroughSamples(&stream));
    DCHECK_EQ(stream.samples.size(), 0u);

    // Ignore extra cues at the end, except for text, as they will result in
    // empty DASH Representations, which is not spec compliant.
    // For text, if the cue is before the max end time, it will still be
    // dispatched as the text samples intercepted by the cue can be split into
    // two at the cue point.
    for (auto& cue : stream.cues) {
      // |max_text_sample_end_time_seconds| is always 0 for non-text samples.
      if (cue->cue_event->time_in_seconds <
          stream.max_text_sample_end_time_seconds) {
        RETURN_IF_ERROR(Dispatch(std::move(cue)));
      } else {
        VLOG(1) << "Ignore extra cue in stream " << cue->stream_index
                << " with time " << cue->cue_event->time_in_seconds
                << "s in the end.";
      }
    }
    stream.cues.clear();
  }

  return FlushAllDownstreams();
}

Status CueAlignmentHandler::OnStreamInfo(std::unique_ptr<StreamData> data) {
  StreamState& stream_state = stream_states_[data->stream_index];
  // Keep a copy of the stream info so that we can check type and check
  // timescale.
  stream_state.info = data->stream_info;

  return Dispatch(std::move(data));
}

Status CueAlignmentHandler::OnVideoSample(std::unique_ptr<StreamData> sample) {
  DCHECK(sample);
  DCHECK(sample->media_sample);

  const size_t stream_index = sample->stream_index;
  StreamState& stream = stream_states_[stream_index];

  const double sample_time = TimeInSeconds(*stream.info, *sample);
  const bool is_key_frame = sample->media_sample->is_key_frame();

  if (is_key_frame && sample_time >= hint_) {
    auto next_sync = sync_points_->PromoteAt(sample_time);

    if (!next_sync) {
      LOG(ERROR) << "Failed to promote sync point at " << sample_time
                 << ". This happens only if video streams are not GOP-aligned.";
      return Status(error::INVALID_ARGUMENT,
                    "Streams are not properly GOP-aligned.");
    }

    RETURN_IF_ERROR(UseNewSyncPoint(std::move(next_sync)));
    DCHECK_EQ(stream.cues.size(), 1u);
    RETURN_IF_ERROR(Dispatch(std::move(stream.cues.front())));
    stream.cues.pop_front();
  }

  return Dispatch(std::move(sample));
}

Status CueAlignmentHandler::OnNonVideoSample(
    std::unique_ptr<StreamData> sample) {
  DCHECK(sample);
  DCHECK(sample->media_sample || sample->text_sample);

  const size_t stream_index = sample->stream_index;
  StreamState& stream_state = stream_states_[stream_index];

  // Accept the sample. This will output it if it comes before the hint point or
  // will cache it if it comes after the hint point.
  RETURN_IF_ERROR(AcceptSample(std::move(sample), &stream_state));

  // If all the streams are waiting on a hint, it means that none has next sync
  // point determined. It also means that there are no video streams and we need
  // to wait for all streams to converge on a hint so that we can get the next
  // sync point.
  if (EveryoneWaitingAtHint()) {
    std::shared_ptr<const CueEvent> next_sync;
    RETURN_IF_ERROR(GetNextCue(hint_, sync_points_, &next_sync));
    RETURN_IF_ERROR(UseNewSyncPoint(next_sync));
  }

  return Status::OK;
}

Status CueAlignmentHandler::OnSample(std::unique_ptr<StreamData> sample) {
  // There are two modes:
  //  1. There is a video input.
  //  2. There are no video inputs.
  //
  // When there is a video input, we rely on the video input get the next sync
  // point and release all the samples.
  //
  // When there are no video inputs, we rely on the sync point queue to block
  // us until there is a sync point.

  const size_t stream_index = sample->stream_index;

  if (sample->text_sample) {
    StreamState& stream = stream_states_[stream_index];
    stream.max_text_sample_end_time_seconds =
        std::max(stream.max_text_sample_end_time_seconds,
                 TextEndTimeInSeconds(*stream.info, *sample));
  }

  const StreamType stream_type =
      stream_states_[stream_index].info->stream_type();
  const bool is_video = stream_type == kStreamVideo;

  return is_video ? OnVideoSample(std::move(sample))
                  : OnNonVideoSample(std::move(sample));
}

Status CueAlignmentHandler::UseNewSyncPoint(
    std::shared_ptr<const CueEvent> new_sync) {
  hint_ = sync_points_->GetHint(new_sync->time_in_seconds);
  DCHECK_GT(hint_, new_sync->time_in_seconds);

  for (size_t stream_index = 0; stream_index < stream_states_.size();
       stream_index++) {
    StreamState& stream = stream_states_[stream_index];
    stream.cues.push_back(StreamData::FromCueEvent(stream_index, new_sync));

    RETURN_IF_ERROR(RunThroughSamples(&stream));
  }

  return Status::OK;
}

bool CueAlignmentHandler::EveryoneWaitingAtHint() const {
  for (const StreamState& stream_state : stream_states_) {
    if (stream_state.samples.empty()) {
      return false;
    }
  }
  return true;
}

Status CueAlignmentHandler::AcceptSample(std::unique_ptr<StreamData> sample,
                                         StreamState* stream) {
  DCHECK(sample);
  DCHECK(sample->media_sample || sample->text_sample);
  DCHECK(stream);

  // Need to cache the stream index as we will lose the pointer when we add
  // the sample to the queue.
  const size_t stream_index = sample->stream_index;

  stream->samples.push_back(std::move(sample));

  if (stream->samples.size() > kMaxBufferSize) {
    LOG(ERROR) << "Stream " << stream_index << " has buffered "
               << stream->samples.size() << " when the max is "
               << kMaxBufferSize;
    return Status(error::INVALID_ARGUMENT,
                  "Streams are not properly multiplexed.");
  }

  return RunThroughSamples(stream);
}

Status CueAlignmentHandler::RunThroughSamples(StreamState* stream) {
  // Step through all our samples until we find where we can insert the cue.
  // Think of this as a merge sort.
  while (stream->cues.size() && stream->samples.size()) {
    const double cue_time = stream->cues.front()->cue_event->time_in_seconds;
    const double sample_time =
        TimeInSeconds(*stream->info, *stream->samples.front());

    if (sample_time < cue_time) {
      RETURN_IF_ERROR(Dispatch(std::move(stream->samples.front())));
      stream->samples.pop_front();
    } else {
      RETURN_IF_ERROR(Dispatch(std::move(stream->cues.front())));
      stream->cues.pop_front();
    }
  }

  // If we still have samples, then it means that we sent out the cue and can
  // now work up to the hint. So now send all samples that come before the hint
  // downstream.
  while (stream->samples.size() &&
         TimeInSeconds(*stream->info, *stream->samples.front()) < hint_) {
    RETURN_IF_ERROR(Dispatch(std::move(stream->samples.front())));
    stream->samples.pop_front();
  }

  return Status::OK;
}
}  // namespace media
}  // namespace shaka
