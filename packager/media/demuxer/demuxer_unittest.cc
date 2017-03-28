// Copyright 2017 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/demuxer/demuxer.h"

#include <gtest/gtest.h>

namespace shaka {
namespace media {

TEST(DemuxerTest, FileNotFound) {
  Demuxer demuxer("file_not_exist.mp4");
  EXPECT_EQ(error::FILE_FAILURE, demuxer.Run().error_code());
}

// TODO(kqyang): Add more tests.

}  // namespace media
}  // namespace shaka
