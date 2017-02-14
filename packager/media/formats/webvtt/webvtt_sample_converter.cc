// Copyright 2015 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/formats/webvtt/webvtt_sample_converter.h"

#include <algorithm>
#include <string>

#include "packager/base/strings/string_util.h"
#include "packager/base/strings/stringprintf.h"
#include "packager/media/base/buffer_writer.h"
#include "packager/media/base/media_sample.h"
#include "packager/media/formats/mp4/box_buffer.h"
#include "packager/media/formats/mp4/box_definitions.h"

namespace shaka {
namespace media {

namespace {

std::shared_ptr<MediaSample> CreateEmptyCueSample(uint64_t start_time,
                                                  uint64_t end_time) {
  DCHECK_GT(end_time, start_time);
  mp4::VTTEmptyCueBox empty_cue_box;

  std::vector<uint8_t> serialized;
  AppendBoxToVector(&empty_cue_box, &serialized);

  std::shared_ptr<MediaSample> empty_cue_sample = MediaSample::CopyFrom(
      serialized.data(), serialized.size(), false);
  empty_cue_sample->set_pts(start_time);
  empty_cue_sample->set_duration(end_time - start_time);
  return empty_cue_sample;
}

void StripTrailingNewlines(const std::string& input, std::string* output) {
  const size_t found = input.find_last_not_of('\n');
  if (found != std::string::npos) {
    *output = input.substr(0, found + 1);
  } else {
    *output = input;
  }
}

mp4::VTTCueBox CueBoxFromCue(const Cue& cue) {
  mp4::VTTCueBox cue_box;
  if (!cue.identifier.empty()) {
    cue_box.cue_id.cue_id = cue.identifier;
  }

  if (!cue.settings.empty()) {
    cue_box.cue_settings.settings = cue.settings;
  }

  StripTrailingNewlines(cue.payload, &cue_box.cue_payload.cue_text);
  return cue_box;
}

std::string TimeToWebVttTimeStamp(uint64_t time_in_ms) {
  const int milliseconds = time_in_ms % 1000;
  const uint64_t seconds_left = time_in_ms / 1000;
  const int seconds = seconds_left % 60;
  const uint64_t minutes_left = seconds_left / 60;
  const int minutes = minutes_left % 60;
  const int hours = minutes_left / 60;

  return base::StringPrintf("%02d:%02d:%02d.%03d", hours, minutes, seconds,
                            milliseconds);
}

std::shared_ptr<MediaSample> CreateVTTCueBoxesSample(
    const std::list<const Cue*>& cues,
    uint64_t start_time,
    uint64_t end_time) {
  // TODO(rkuroiwa): Source IDs must be assigned to the cues and the same cue
  // should have the same ID in different samples. Probably requires a mapping
  // from cues to IDs.
  CHECK(!cues.empty());

  std::vector<uint8_t> data;
  std::string cue_current_time = TimeToWebVttTimeStamp(start_time);

  BufferWriter writer;
  for (const Cue* cue : cues) {
    mp4::VTTCueBox cue_box = CueBoxFromCue(*cue);
    // If there is internal timing, i.e. WebVTT cue timestamp, then
    // cue_current_time should be populated
    // "which gives the VTT timestamp associated with the start time of sample."
    // TODO(rkuroiwa): Reuse TimestampToMilliseconds() to check if there is an
    // internal timestamp in the payload to set CueTimeBox.cue_current_time.
    cue_box.Write(&writer);
  }

  std::shared_ptr<MediaSample> sample =
      MediaSample::CopyFrom(writer.Buffer(), writer.Size(), false);
  sample->set_pts(start_time);
  sample->set_duration(end_time - start_time);
  return sample;
}

// This function returns the minimum of cue_start_time, cue_end_time,
// current_minimum should be bigger than sweep_line.
uint64_t GetMinimumPastSweepLine(uint64_t cue_start_time,
                                 uint64_t cue_end_time,
                                 uint64_t sweep_line,
                                 uint64_t current_minimum) {
  DCHECK_GE(current_minimum, sweep_line);
  if (cue_end_time <= sweep_line)
    return current_minimum;

  // Anything below is cue_end_time > sweep_line.
  if (cue_start_time > sweep_line) {
    // The start time of this cue is past the sweepline, return the min.
    return std::min(cue_start_time, current_minimum);
  } else {
    // The sweep line is at the start or in the middle of a cue.
    return std::min(cue_end_time, current_minimum);
  }
}

} // namespace

void AppendBoxToVector(mp4::Box* box, std::vector<uint8_t>* output_vector) {
  BufferWriter writer;
  box->Write(&writer);
  output_vector->insert(output_vector->end(),
                        writer.Buffer(),
                        writer.Buffer() + writer.Size());
}

WebVttSampleConverter::WebVttSampleConverter() : next_cue_start_time_(0u) {}
WebVttSampleConverter::~WebVttSampleConverter() {}

// Note that this |sample| is either a cue or a comment. It does not have any
// info on whether the next cue is overlapping or not.
void WebVttSampleConverter::PushCue(const Cue& cue) {
  if (!cue.comment.empty()) {
    // A comment. Put it in the buffer and skip.
    mp4::VTTAdditionalTextBox comment;
    StripTrailingNewlines(cue.comment, &comment.cue_additional_text);
    additional_texts_.push_back(comment);
    // TODO(rkuriowa): Handle comments as samples.

    return;
  }

  cues_.push_back(cue);
  if (cues_.size() == 1) {
    // Cannot make a decision with just one sample. Cache it and wait for
    // another one.
    next_cue_start_time_ = cues_.front().start_time;
    return;
  }

  CHECK_GE(cues_.size(), 2u);
  // TODO(rkuroiwa): This isn't wrong but all the cues where
  // endtime < latest cue start time
  // can be processed. Change the logic so that if there are cues that meet the
  // condition above, create samples immediately and remove them.
  // Note: This doesn't mean that all the cues can be removed, just the ones
  // that meet the condition.
  bool processed_cues = HandleAllCuesButLatest();
  if (!processed_cues)
    return;

  // Remove all the cues except the latest one.
  auto erase_last_iterator = --cues_.end();
  cues_.erase(cues_.begin(), erase_last_iterator);
}

void WebVttSampleConverter::Flush() {
  if (cues_.empty())
    return;
  if (cues_.size() == 1) {
    std::list<const Cue*> temp_list;
    temp_list.push_back(&cues_.front());
    CHECK_EQ(next_cue_start_time_, cues_.front().start_time);
    ready_samples_.push_back(CreateVTTCueBoxesSample(
        temp_list,
        next_cue_start_time_,
        cues_.front().start_time + cues_.front().duration));
    cues_.clear();
    return;
  }

  bool processed_cue = HandleAllCues();
  CHECK(processed_cue)
      << "No cues were processed but the cues should have been flushed.";
  cues_.clear();
}

size_t WebVttSampleConverter::ReadySamplesSize() {
  return ready_samples_.size();
}

std::shared_ptr<MediaSample> WebVttSampleConverter::PopSample() {
  CHECK(!ready_samples_.empty());
  std::shared_ptr<MediaSample> ret = ready_samples_.front();
  ready_samples_.pop_front();
  return ret;
}

// TODO(rkuroiwa): Some samples may be ready. Example:
// Cues:
// |--------- 1 ---------|
//   |-- 2 --|
//                  |-- 3 --|
//
// Samples:
// |A|   B   |   C  |
// Samples A, B, and C can be created when Cue 3 is pushed.
// Change algorithm to create A,B,C samples right away.
// Note that this requires change to the caller on which cues
// to remove.
bool WebVttSampleConverter::HandleAllCuesButLatest() {
  DCHECK_GE(cues_.size(), 2u);
  const Cue& latest_cue = cues_.back();

  // Don't process the cues until the latest cue doesn't overlap with all the
  // previous cues.
  uint64_t max_cue_end_time = 0;  // Not including the latest.
  auto latest_cue_it = --cues_.end();
  for (auto cue_it = cues_.begin(); cue_it != latest_cue_it; ++cue_it) {
    const Cue& cue = *cue_it;
    const uint64_t cue_end_time = cue.start_time + cue.duration;
    if (cue_end_time > latest_cue.start_time)
      return false;

    if (max_cue_end_time < cue_end_time)
      max_cue_end_time = cue_end_time;
  }
  // Reaching here means that the latest cue does not overlap with all
  // the previous cues.

  // Because sweep_stop_time is assigned to next_cue_start_time_ it is not
  // set to latest_cue.start_time here; there may be a gap between
  // latest_cue.start_time and previous_cue_end_time.
  // The correctness of SweepCues() doesn't change whether the sweep stops
  // right before the latest cue or right before the gap.
  const uint64_t sweep_stop_time = max_cue_end_time;
  const uint64_t sweep_line_start = cues_.front().start_time;
  bool processed_cues =
      SweepCues(sweep_line_start, sweep_stop_time);
  next_cue_start_time_ = sweep_stop_time;
  if (next_cue_start_time_ < latest_cue.start_time) {
    ready_samples_.push_back(CreateEmptyCueSample(next_cue_start_time_,
                                                  latest_cue.start_time));
    next_cue_start_time_ = latest_cue.start_time;
  }
  return processed_cues;
}

bool WebVttSampleConverter::HandleAllCues() {
  uint64_t latest_time = 0u;
  for (const Cue& cue : cues_) {
    if (cue.start_time + cue.duration > latest_time)
      latest_time = cue.start_time + cue.duration;
  }
  const uint64_t sweep_line_start = cues_.front().start_time;
  const uint64_t sweep_stop_time = latest_time;
  bool processed = SweepCues(sweep_line_start, sweep_stop_time);
  next_cue_start_time_ = sweep_stop_time;
  return processed;
}

bool WebVttSampleConverter::SweepCues(uint64_t sweep_line,
                                      uint64_t sweep_stop_time) {
  bool processed_cues = false;
  // This is a sweep line algorithm. For every iteration, it determines active
  // cues and makes a sample.
  // At the end of an interation |next_start_time| should be set to the minimum
  // of all the start and end times of the cues that is after |sweep_line|.
  // |sweep_line| is set to |next_start_time| before the next iteration.
  while (sweep_line < sweep_stop_time) {
    std::list<const Cue*> cues_for_a_sample;
    uint64_t next_start_time = sweep_stop_time;

    // Put all the cues that should be displayed at sweep_line, in
    // cues_for_a_sample.
    // next_start_time is also updated in this loop by checking all the cues.
    for (const Cue& cue : cues_) {
      if (cue.start_time >= sweep_stop_time)
        break;
      if (cue.start_time >= next_start_time)
        break;

      const uint64_t cue_end_time = cue.start_time + cue.duration;
      if (cue_end_time <= sweep_line)
        continue;
      next_start_time = GetMinimumPastSweepLine(
          cue.start_time, cue_end_time, sweep_line, next_start_time);

      if (cue.start_time <= sweep_line) {
        DCHECK_GT(cue_end_time, sweep_line);
        cues_for_a_sample.push_back(&cue);
      }
    }

    DCHECK(!cues_for_a_sample.empty()) << "For now the only use case of this "
                                          "function is to sweep non-empty "
                                          "cues.";
    if (!cues_for_a_sample.empty()) {
      ready_samples_.push_back(CreateVTTCueBoxesSample(
          cues_for_a_sample, sweep_line, next_start_time));
      processed_cues = true;
    }

    sweep_line = next_start_time;
  }

  DCHECK_EQ(sweep_line, sweep_stop_time);
  return processed_cues;
}

}  // namespace media
}  // namespace shaka
