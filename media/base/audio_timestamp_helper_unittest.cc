// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/audio_timestamp_helper.h"
#include "media/base/buffers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

static const int kDefaultSampleRate = 44100;

class AudioTimestampHelperTest : public ::testing::Test {
 public:
  AudioTimestampHelperTest() : helper_(kDefaultSampleRate) {
    helper_.SetBaseTimestamp(base::TimeDelta());
  }

  // Adds frames to the helper and returns the current timestamp in
  // microseconds.
  int64 AddFrames(int frames) {
    helper_.AddFrames(frames);
    return helper_.GetTimestamp().InMicroseconds();
  }

  int64 FramesToTarget(int target_in_microseconds) {
    return helper_.GetFramesToTarget(
        base::TimeDelta::FromMicroseconds(target_in_microseconds));
  }

  void TestGetFramesToTargetRange(int frame_count, int start, int end) {
    for (int i = start; i <= end; ++i) {
      EXPECT_EQ(frame_count, FramesToTarget(i)) << " Failure for timestamp "
                                                << i << " us.";
    }
  }

 protected:
  AudioTimestampHelper helper_;

  DISALLOW_COPY_AND_ASSIGN(AudioTimestampHelperTest);
};

TEST_F(AudioTimestampHelperTest, Basic) {
  EXPECT_EQ(0, helper_.GetTimestamp().InMicroseconds());

  // Verify that the output timestamp is always rounded down to the
  // nearest microsecond. 1 frame @ 44100 is ~22.67573 microseconds,
  // which is why the timestamp sometimes increments by 23 microseconds
  // and other times it increments by 22 microseconds.
  EXPECT_EQ(0, AddFrames(0));
  EXPECT_EQ(22, AddFrames(1));
  EXPECT_EQ(45, AddFrames(1));
  EXPECT_EQ(68, AddFrames(1));
  EXPECT_EQ(90, AddFrames(1));
  EXPECT_EQ(113, AddFrames(1));

  // Verify that adding frames one frame at a time matches the timestamp
  // returned if the same number of frames are added all at once.
  base::TimeDelta timestamp_1  = helper_.GetTimestamp();
  helper_.SetBaseTimestamp(kNoTimestamp());
  EXPECT_TRUE(kNoTimestamp() == helper_.base_timestamp());
  helper_.SetBaseTimestamp(base::TimeDelta());
  EXPECT_EQ(0, helper_.GetTimestamp().InMicroseconds());

  helper_.AddFrames(5);
  EXPECT_EQ(113, helper_.GetTimestamp().InMicroseconds());
  EXPECT_TRUE(timestamp_1 == helper_.GetTimestamp());
}


TEST_F(AudioTimestampHelperTest, GetDuration) {
  helper_.SetBaseTimestamp(base::TimeDelta::FromMicroseconds(100));

  int frame_count = 5;
  int64 expected_durations[] = { 113, 113, 114, 113, 113, 114 };
  for (size_t i = 0; i < arraysize(expected_durations); ++i) {
    base::TimeDelta duration = helper_.GetFrameDuration(frame_count);
    EXPECT_EQ(expected_durations[i], duration.InMicroseconds());

    base::TimeDelta timestamp_1 = helper_.GetTimestamp() + duration;
    helper_.AddFrames(frame_count);
    base::TimeDelta timestamp_2 = helper_.GetTimestamp();
    EXPECT_TRUE(timestamp_1 == timestamp_2);
  }
}

TEST_F(AudioTimestampHelperTest, GetFramesToTarget) {
  // Verify GetFramesToTarget() rounding behavior.
  // 1 frame @ 44100 is ~22.67573 microseconds,

  // Test values less than half of the frame duration.
  TestGetFramesToTargetRange(0, 0, 11);

  // Test values between half the frame duration & the
  // full frame duration.
  TestGetFramesToTargetRange(1, 12, 22);

  // Verify that the same number of frames is returned up
  // to the next half a frame.
  TestGetFramesToTargetRange(1, 23, 34);

  // Verify the next 3 ranges.
  TestGetFramesToTargetRange(2, 35, 56);
  TestGetFramesToTargetRange(3, 57, 79);
  TestGetFramesToTargetRange(4, 80, 102);
  TestGetFramesToTargetRange(5, 103, 124);

  // Add frames to the helper so negative frame counts can be tested.
  helper_.AddFrames(5);

  // Note: The timestamp ranges must match the positive values
  // tested above to verify that the code is rounding properly.
  TestGetFramesToTargetRange(0, 103, 124);
  TestGetFramesToTargetRange(-1, 80, 102);
  TestGetFramesToTargetRange(-2, 57, 79);
  TestGetFramesToTargetRange(-3, 35, 56);
  TestGetFramesToTargetRange(-4, 12, 34);
  TestGetFramesToTargetRange(-5, 0, 11);
}

}  // namespace media
