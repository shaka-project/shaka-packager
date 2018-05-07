// Copyright 2018 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/codecs/hls_audio_util.h"

#include <gtest/gtest.h>

#include "packager/media/base/buffer_writer.h"

namespace shaka {
namespace media {

TEST(HlsAudioUtilAudioSetupTest, AacAudioConfigLcProfile) {
  const uint8_t kAacLcConfig[] = {12, 10};
  const uint8_t kExpectedAudioSetupInformation[]{
      'z', 'a', 'a', 'c', 0, 0, 1, 2, 12, 10,
  };

  BufferWriter buffer_writer;
  ASSERT_TRUE(WriteAudioSetupInformation(kCodecAAC, kAacLcConfig,
                                         sizeof(kAacLcConfig), &buffer_writer));
  EXPECT_EQ(
      std::vector<uint8_t>(std::begin(kExpectedAudioSetupInformation),
                           std::end(kExpectedAudioSetupInformation)),
      std::vector<uint8_t>(buffer_writer.Buffer(),
                           buffer_writer.Buffer() + buffer_writer.Size()));
}

TEST(HlsAudioUtilAudioSetupTest, AacAudioConfigHeProfile) {
  const uint8_t kAacHeConfig[] = {0x2B, 0x92, 8, 0};
  const uint8_t kExpectedAudioSetupInformation[]{
      'z', 'a', 'c', 'h', 0, 0, 1, 4, 0x2B, 0x92, 8, 0,
  };

  BufferWriter buffer_writer;
  ASSERT_TRUE(WriteAudioSetupInformation(kCodecAAC, kAacHeConfig,
                                         sizeof(kAacHeConfig), &buffer_writer));
  EXPECT_EQ(
      std::vector<uint8_t>(std::begin(kExpectedAudioSetupInformation),
                           std::end(kExpectedAudioSetupInformation)),
      std::vector<uint8_t>(buffer_writer.Buffer(),
                           buffer_writer.Buffer() + buffer_writer.Size()));
}

TEST(HlsAudioUtilAudioSetupTest, AC3) {
  const uint8_t kAudioSpecificConfig[] = {
      'a', 'u', 'd', 'i', 'o', '_', 'c', 'o', 'n', 'f',
  };
  const uint8_t kExpectedAudioSetupInformation[]{
      'z', 'a', 'c', '3', 0,   0,   1,   10,  'a',
      'u', 'd', 'i', 'o', '_', 'c', 'o', 'n', 'f',
  };

  BufferWriter buffer_writer;
  ASSERT_TRUE(WriteAudioSetupInformation(kCodecAC3, kAudioSpecificConfig,
                                         sizeof(kAudioSpecificConfig),
                                         &buffer_writer));
  EXPECT_EQ(
      std::vector<uint8_t>(std::begin(kExpectedAudioSetupInformation),
                           std::end(kExpectedAudioSetupInformation)),
      std::vector<uint8_t>(buffer_writer.Buffer(),
                           buffer_writer.Buffer() + buffer_writer.Size()));
}

TEST(HlsAudioUtilAudioSetupTest, EAC3) {
  const uint8_t kAudioSpecificConfig[] = {
      'a', 'u', 'd', 'i', 'o', '_', 'c', 'o', 'n', 'f',
  };
  const uint8_t kExpectedAudioSetupInformation[]{
      'z', 'e', 'c', '3', 0,   0,   1,   10,  'a',
      'u', 'd', 'i', 'o', '_', 'c', 'o', 'n', 'f',
  };

  BufferWriter buffer_writer;
  ASSERT_TRUE(WriteAudioSetupInformation(kCodecEAC3, kAudioSpecificConfig,
                                         sizeof(kAudioSpecificConfig),
                                         &buffer_writer));
  EXPECT_EQ(
      std::vector<uint8_t>(std::begin(kExpectedAudioSetupInformation),
                           std::end(kExpectedAudioSetupInformation)),
      std::vector<uint8_t>(buffer_writer.Buffer(),
                           buffer_writer.Buffer() + buffer_writer.Size()));
}

TEST(HlsAudioUtilAudioSetupTest, FLAC_NotSupported) {
  const uint8_t kAudioSpecificConfig[] = {
      'a', 'u', 'd', 'i', 'o', '_', 'c', 'o', 'n', 'f',
  };

  BufferWriter buffer_writer;
  ASSERT_FALSE(WriteAudioSetupInformation(kCodecFlac, kAudioSpecificConfig,
                                          sizeof(kAudioSpecificConfig),
                                          &buffer_writer));
}

}  // namespace media
}  // namespace shaka
