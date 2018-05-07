// Copyright 2018 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/base/id3_tag.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "packager/media/base/buffer_writer.h"

using ::testing::ElementsAreArray;

namespace shaka {
namespace media {

TEST(Id3TagTest, WriteToVector) {
  Id3Tag id3_tag;

  std::vector<uint8_t> output;
  id3_tag.WriteToVector(&output);

  const uint8_t kExpectedOutput[] = {
      'I', 'D', '3', 4, 0, 0, 0, 0, 0, 0,
  };
  EXPECT_THAT(output, ElementsAreArray(kExpectedOutput));
}

TEST(Id3TagTest, WriteToBuffer) {
  Id3Tag id3_tag;

  BufferWriter buffer_writer;
  id3_tag.WriteToBuffer(&buffer_writer);

  const uint8_t kExpectedOutput[] = {
      'I', 'D', '3', 4, 0, 0, 0, 0, 0, 0,
  };
  EXPECT_EQ(
      std::vector<uint8_t>(std::begin(kExpectedOutput),
                           std::end(kExpectedOutput)),
      std::vector<uint8_t>(buffer_writer.Buffer(),
                           buffer_writer.Buffer() + buffer_writer.Size()));
}

TEST(Id3TagTest, AddPrivateFrameOnce) {
  Id3Tag id3_tag;
  id3_tag.AddPrivateFrame("testing.owner", "data");

  std::vector<uint8_t> output;
  id3_tag.WriteToVector(&output);

  const uint8_t kExpectedOutput[] = {
      'I', 'D', '3', 4,   0,   0,   0,   0,   0,   28,  // Header
      'P', 'R', 'I', 'V', 0,   0,   0,   18,  0,   0,  't', 'e', 's', 't',
      'i', 'n', 'g', '.', 'o', 'w', 'n', 'e', 'r', 0,  'd', 'a', 't', 'a',
  };
  EXPECT_THAT(output, ElementsAreArray(kExpectedOutput));
}

TEST(Id3TagTest, AddPrivateFrameTwice) {
  Id3Tag id3_tag;
  id3_tag.AddPrivateFrame("testing.owner1", "data1");
  id3_tag.AddPrivateFrame("testing.owner2", "data2");

  std::vector<uint8_t> output;
  id3_tag.WriteToVector(&output);

  const uint8_t kExpectedOutput[] = {
      'I', 'D', '3', 4,   0,   0,   0,   0,   0,   60,  // Header
      'P', 'R', 'I', 'V', 0,   0,   0,   20,  0,   0,  't', 'e', 's', 't', 'i',
      'n', 'g', '.', 'o', 'w', 'n', 'e', 'r', '1', 0,  'd', 'a', 't', 'a', '1',
      'P', 'R', 'I', 'V', 0,   0,   0,   20,  0,   0,  't', 'e', 's', 't', 'i',
      'n', 'g', '.', 'o', 'w', 'n', 'e', 'r', '2', 0,  'd', 'a', 't', 'a', '2',
  };
  EXPECT_THAT(output, ElementsAreArray(kExpectedOutput));
}

TEST(Id3TagTest, AddBigPrivateFrameOnce) {
  const size_t kTestOwnerSize = 200;
  const size_t kTestDataSize = 200;
  std::string test_owner(kTestOwnerSize, 0);
  std::string test_data(kTestDataSize, 0);

  Id3Tag id3_tag;
  id3_tag.AddPrivateFrame(test_owner, test_data);

  std::vector<uint8_t> output;
  id3_tag.WriteToVector(&output);

  const uint8_t kExpectedOutputHead[] = {
      'I', 'D', '3', 4,   0, 0, 0, 0,  3, 27,  // Header
      'P', 'R', 'I', 'V', 0, 0, 3, 17, 0, 0,
  };
  std::vector<uint8_t> expected_output(std::begin(kExpectedOutputHead),
                                       std::end(kExpectedOutputHead));
  expected_output.resize(expected_output.size() + kTestOwnerSize + 1 +
                         kTestDataSize);
  EXPECT_EQ(expected_output, output);
}

}  // namespace media

}  // namespace shaka
