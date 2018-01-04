// Copyright 2017 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/formats/webvtt/webvtt_to_mp4_handler.h"

#include <algorithm>

#include "packager/media/base/buffer_writer.h"
#include "packager/media/formats/mp4/box_buffer.h"
#include "packager/media/formats/mp4/box_definitions.h"

namespace shaka {
namespace media {

class DisplayAction {
 public:
  DisplayAction(uint64_t id, uint64_t time) : id_(id), time_(time) {}
  virtual ~DisplayAction() = default;

  uint64_t id() const { return id_; }
  uint64_t time() const { return time_; }
  virtual void ActOn(std::list<const TextSample*>* display) const = 0;

 private:
  uint64_t id_;
  uint64_t time_;
};

namespace {
const uint64_t kTrackId = 0;

class AddToDisplayAction : public DisplayAction {
 public:
  explicit AddToDisplayAction(uint64_t id,
                              std::shared_ptr<const TextSample>& sample)
      : DisplayAction(id, sample->start_time()), sample_(sample) {}
  void ActOn(std::list<const TextSample*>* display) const override {
    display->push_back(sample_.get());
  }

 private:
  std::shared_ptr<const TextSample> sample_;
};

class RemoveFromDisplayAction : public DisplayAction {
 public:
  explicit RemoveFromDisplayAction(uint64_t id,
                                   std::shared_ptr<const TextSample>& sample)
      : DisplayAction(id, sample->EndTime()), sample_(sample) {}
  void ActOn(std::list<const TextSample*>* display) const override {
    display->remove(sample_.get());
  }

 private:
  std::shared_ptr<const TextSample> sample_;
};
}  // namespace

bool DisplayActionCompare::operator()(
    const std::shared_ptr<DisplayAction>& left,
    const std::shared_ptr<DisplayAction>& right) const {
  return left->time() == right->time() ? left->id() > right->id()
                                       : left->time() > right->time();
}

Status WebVttToMp4Handler::InitializeInternal() {
  return Status::OK;
}

Status WebVttToMp4Handler::Process(std::unique_ptr<StreamData> stream_data) {
  if (StreamDataType::kStreamInfo == stream_data->stream_data_type) {
    return DispatchStreamInfo(kTrackId, std::move(stream_data->stream_info));
  }
  if (stream_data->stream_data_type == StreamDataType::kTextSample) {
    std::shared_ptr<const TextSample> sample = stream_data->text_sample;

    std::shared_ptr<DisplayAction> add(
        new AddToDisplayAction(NextActionId(), sample));
    std::shared_ptr<DisplayAction> remove(
        new RemoveFromDisplayAction(NextActionId(), sample));

    actions_.push(add);
    actions_.push(remove);

    ProcessUpToTime(add->time());

    return Status::OK;
  }
  return Status(error::INTERNAL_ERROR,
                "Invalid stream data type for this handler");
}

Status WebVttToMp4Handler::OnFlushRequest(size_t input_stream_index) {
  const uint64_t kEndOfTime = std::numeric_limits<uint64_t>::max();
  ProcessUpToTime(kEndOfTime);

  return FlushDownstream(0);
}

void WebVttToMp4Handler::WriteCue(const std::string& id,
                                  const std::string& settings,
                                  const std::string& payload,
                                  BufferWriter* out) {
  mp4::VTTCueBox box;

  if (id.length()) {
    box.cue_id.cue_id = id;
  }
  if (settings.length()) {
    box.cue_settings.settings = settings;
  }
  if (payload.length()) {
    box.cue_payload.cue_text = payload;
  }
  // If there is internal timing, i.e. WebVTT cue timestamp, then
  // cue_current_time should be populated
  // "which gives the VTT timestamp associated with the start time of sample."
  // TODO(rkuroiwa): Reuse TimestampToMilliseconds() to check if there is an
  // internal timestamp in the payload to set CueTimeBox.cue_current_time.
  box.Write(out);
}

void WebVttToMp4Handler::ProcessUpToTime(uint64_t cutoff_time) {
  // We can only process as far as the last add as no new events will be
  // added that come before that time.
  while (actions_.size() && actions_.top()->time() < cutoff_time) {
    // STAGE 1: Write out the current state
    // Get the time range for which the current active state is valid.
    const uint64_t previous_change = next_change_;
    next_change_ = actions_.top()->time();
    // The only time that |previous_change| and |next_change_| should ever break
    // the rule |next_change_ > previous_change| is at the start where
    // |previous_change| and |next_change_| are both zero.
    DCHECK((previous_change == 0 && next_change_ == 0) ||
           next_change_ > previous_change);

    // Send out the active group. If there is nothing in the active group, then
    // this segment is ignored.
    if (active_.size()) {
      MergeAndSendSamples(active_, previous_change, next_change_);
    }

    // STAGE 2: Move to the next state.
    while (actions_.size() && actions_.top()->time() == next_change_) {
      actions_.top()->ActOn(&active_);
      actions_.pop();
    }
  }
}

Status WebVttToMp4Handler::MergeAndSendSamples(
    const std::list<const TextSample*>& samples,
    uint64_t start_time,
    uint64_t end_time) {
  DCHECK_GT(end_time, start_time);

  box_writer_.Clear();

  for (const TextSample* sample : samples) {
    DCHECK_LE(sample->start_time(), start_time);
    DCHECK_GE(sample->EndTime(), end_time);
    WriteCue(sample->id(), sample->settings(), sample->payload(), &box_writer_);
  }

  std::shared_ptr<MediaSample> sample =
      MediaSample::CopyFrom(box_writer_.Buffer(), box_writer_.Size(), true);
  sample->set_pts(start_time);
  sample->set_dts(start_time);
  sample->set_duration(end_time - start_time);
  return DispatchMediaSample(kTrackId, std::move(sample));
}

uint64_t WebVttToMp4Handler::NextActionId() {
  return next_id_++;
}
}  // namespace media
}  // namespace shaka
