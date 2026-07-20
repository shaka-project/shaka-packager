// Copyright 2025 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/media/formats/mp4/mp4_muxer.h>

#include <cstdint>

#include <gtest/gtest.h>

#include <packager/media/base/media_sample.h>
#include <packager/media/base/muxer_options.h>
#include <packager/status.h>

namespace shaka {
namespace media {
namespace mp4 {

// Unit tests for MP4Muxer's EditList offset computation
// (UpdateEditListOffsetFromSample), which decides the EditList media_time and,
// via the fragmenter, the baseMediaDecodeTime bias for the first sample.
class MP4MuxerTest : public ::testing::Test {
 protected:
  // Runs UpdateEditListOffsetFromSample on a first sample with the given
  // pts/dts and returns its status. On success |offset| receives the resulting
  // edit_list_offset_.
  Status ComputeEditListOffset(int64_t pts, int64_t dts, int64_t* offset) {
    MuxerOptions options;
    MP4Muxer muxer(options);
    const uint8_t kData[] = {0x00};
    auto sample =
        MediaSample::CopyFrom(kData, sizeof(kData), /*is_key_frame=*/true);
    sample->set_pts(pts);
    sample->set_dts(dts);
    Status status = muxer.UpdateEditListOffsetFromSample(*sample);
    if (status.ok() && offset)
      *offset = muxer.edit_list_offset_.value();
    return status;
  }

  int64_t EditListOffset(int64_t pts, int64_t dts) {
    int64_t offset = -1;
    EXPECT_TRUE(ComputeEditListOffset(pts, dts, &offset).ok());
    return offset;
  }
};

TEST_F(MP4MuxerTest, NoOffsetNonNegativePtsNeedsNoEditList) {
  // pts == dts >= 0: nothing to shift.
  EXPECT_EQ(0, EditListOffset(/*pts=*/1000, /*dts=*/1000));
}

TEST_F(MP4MuxerTest, NegativePtsNoOffset) {
  // pts == dts < 0 (e.g. audio priming): shift so decode time becomes 0.
  EXPECT_EQ(500, EditListOffset(/*pts=*/-500, /*dts=*/-500));
}

TEST_F(MP4MuxerTest, BFramesNonNegativePts) {
  // pts > dts with pts >= 0 (reordering / B-frames): offset is pts - dts, which
  // makes the decode time equal to pts (non-negative).
  EXPECT_EQ(300, EditListOffset(/*pts=*/300, /*dts=*/0));
}

TEST_F(MP4MuxerTest, BFramesNegativePts) {
  // Regression test for
  // https://github.com/shaka-project/shaka-packager/issues/1265.
  // pts > dts but pts < 0. Using pts - dts (=1471) would make the
  // baseMediaDecodeTime negative (decode_time = first_dts + offset = pts). The
  // offset must instead be -dts so the decode time is exactly 0.
  EXPECT_EQ(1600, EditListOffset(/*pts=*/-129, /*dts=*/-1600));
}

TEST_F(MP4MuxerTest, PtsLessThanDtsFails) {
  int64_t offset = -1;
  EXPECT_FALSE(ComputeEditListOffset(/*pts=*/100, /*dts=*/200, &offset).ok());
}

}  // namespace mp4
}  // namespace media
}  // namespace shaka
