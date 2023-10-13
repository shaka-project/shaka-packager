// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PACKAGER_MEDIA_BASE_AUDIO_TIMESTAMP_HELPER_H_
#define PACKAGER_MEDIA_BASE_AUDIO_TIMESTAMP_HELPER_H_

#include <cstdint>

#include <packager/macros/classes.h>

namespace shaka {
namespace media {

// Generates timestamps for a sequence of audio sample frames. This class should
// be used any place timestamps need to be calculated for a sequence of audio
// samples. It helps avoid timestamps inaccuracies caused by rounding/truncation
// in repeated sample count to timestamp conversions.
//
// The class is constructed with samples_per_second information so that it can
// convert audio sample frame counts into timestamps. After the object is
// constructed, SetBaseTimestamp() must be called to specify the starting
// timestamp of the audio sequence. As audio samples are received, their frame
// counts are added using AddFrames(). These frame counts are accumulated by
// this class so GetTimestamp() can be used to determine the timestamp for the
// samples that have been added. GetDuration() calculates the proper duration
// values for samples added to the current timestamp. GetFramesToTarget()
// determines the number of frames that need to be added/removed from the
// accumulated frames to reach a target timestamp.
class AudioTimestampHelper {
 public:
  explicit AudioTimestampHelper(int32_t timescale, uint32_t samples_per_second);

  // Sets the base timestamp to |base_timestamp| and the sets count to 0.
  void SetBaseTimestamp(int64_t base_timestamp);

  int64_t base_timestamp() const;
  int64_t frame_count() const { return frame_count_; }

  // Adds |frame_count| to the frame counter.
  // Note: SetBaseTimestamp() must be called with a value other than
  // kNoTimestamp() before this method can be called.
  void AddFrames(int64_t frame_count);

  // Get the current timestamp. This value is computed from the base_timestamp()
  // and the number of sample frames that have been added so far.
  int64_t GetTimestamp() const;

  // Gets the duration if |frame_count| frames were added to the current
  // timestamp reported by GetTimestamp(). This method ensures that
  // (GetTimestamp() + GetFrameDuration(n)) will equal the timestamp that
  // GetTimestamp() will return if AddFrames(n) is called.
  int64_t GetFrameDuration(int64_t frame_count) const;

  // Returns the number of frames needed to reach the target timestamp.
  // Note: |target| must be >= |base_timestamp_|.
  int64_t GetFramesToTarget(int64_t target) const;

 private:
  int64_t ComputeTimestamp(int64_t frame_count) const;

  double ticks_per_frame_;

  int64_t base_timestamp_;

  // Number of frames accumulated by AddFrames() calls.
  int64_t frame_count_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(AudioTimestampHelper);
};

}  // namespace media
}  // namespace shaka

#endif
