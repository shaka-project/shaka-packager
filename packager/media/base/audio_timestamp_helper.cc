// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <packager/media/base/audio_timestamp_helper.h>

#include <absl/log/check.h>
#include <absl/log/log.h>

#include <packager/media/base/timestamp.h>

namespace shaka {
namespace media {

AudioTimestampHelper::AudioTimestampHelper(int32_t timescale,
                                           uint32_t samples_per_second)
    : base_timestamp_(kNoTimestamp), frame_count_(0) {
  DCHECK_GT(samples_per_second, 0u);
  double fps = samples_per_second;
  ticks_per_frame_ = timescale / fps;
}

void AudioTimestampHelper::SetBaseTimestamp(int64_t base_timestamp) {
  base_timestamp_ = base_timestamp;
  frame_count_ = 0;
}

int64_t AudioTimestampHelper::base_timestamp() const {
  return base_timestamp_;
}

void AudioTimestampHelper::AddFrames(int64_t frame_count) {
  DCHECK_GE(frame_count, 0);
  DCHECK(base_timestamp_ != kNoTimestamp);
  frame_count_ += frame_count;
}

int64_t AudioTimestampHelper::GetTimestamp() const {
  return ComputeTimestamp(frame_count_);
}

int64_t AudioTimestampHelper::GetFrameDuration(int64_t frame_count) const {
  DCHECK_GE(frame_count, 0);
  int64_t end_timestamp = ComputeTimestamp(frame_count_ + frame_count);
  return end_timestamp - GetTimestamp();
}

int64_t AudioTimestampHelper::GetFramesToTarget(int64_t target) const {
  DCHECK(base_timestamp_ != kNoTimestamp);
  DCHECK(target >= base_timestamp_);

  int64_t delta_in_ticks = (target - GetTimestamp());
  if (delta_in_ticks == 0)
    return 0;

  // Compute a timestamp relative to |base_timestamp_| since timestamps
  // created from |frame_count_| are computed relative to this base.
  // This ensures that the time to frame computation here is the proper inverse
  // of the frame to time computation in ComputeTimestamp().
  int64_t delta_from_base = target - base_timestamp_;

  // Compute frame count for the time delta. This computation rounds to
  // the nearest whole number of frames.
  double threshold = ticks_per_frame_ / 2;
  int64_t target_frame_count = (delta_from_base + threshold) / ticks_per_frame_;
  return target_frame_count - frame_count_;
}

int64_t AudioTimestampHelper::ComputeTimestamp(int64_t frame_count) const {
  DCHECK_GE(frame_count, 0);
  DCHECK(base_timestamp_ != kNoTimestamp);
  double frames_ticks = ticks_per_frame_ * frame_count;
  return base_timestamp_ + frames_ticks;
}

}  // namespace media
}  // namespace shaka
