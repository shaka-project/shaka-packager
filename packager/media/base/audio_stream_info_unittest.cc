// Copyright 2024 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/media/base/audio_stream_info.h>

#include <gtest/gtest.h>

namespace shaka {
namespace media {

const int kSimpleProfile = 0;
const int kBaseProfile = 1;

TEST(AudioStreamInfo, IamfGetCodecStringForSimpleProfilesAndPcm) {
  const uint8_t audio_object_type =
      ((kSimpleProfile << 6) |              // primary_profile
       ((kSimpleProfile << 4) & 0x3F) |     // additional_profile
       ((kCodecPcm - kCodecAudio) & 0xF));  // IAMF codec

  std::string codec_string =
      AudioStreamInfo::GetCodecString(kCodecIAMF, audio_object_type);
  EXPECT_EQ("iamf.000.000.ipcm", codec_string);
}

TEST(AudioStreamInfo, IamfGetCodecStringForSimpleProfilesAndOpus) {
  const uint8_t audio_object_type =
      ((kSimpleProfile << 6) |               // primary_profile
       ((kSimpleProfile << 4) & 0x3F) |      // additional_profile
       ((kCodecOpus - kCodecAudio) & 0xF));  // IAMF codec

  std::string codec_string =
      AudioStreamInfo::GetCodecString(kCodecIAMF, audio_object_type);
  EXPECT_EQ("iamf.000.000.Opus", codec_string);
}

TEST(AudioStreamInfo, IamfGetCodecStringForSimpleProfilesAndMp4a) {
  const uint8_t audio_object_type =
      ((kSimpleProfile << 6) |              // primary_profile
       ((kSimpleProfile << 4) & 0x3F) |     // additional_profile
       ((kCodecAAC - kCodecAudio) & 0xF));  // IAMF codec

  std::string codec_string =
      AudioStreamInfo::GetCodecString(kCodecIAMF, audio_object_type);
  EXPECT_EQ("iamf.000.000.mp4a.40.2", codec_string);
}

TEST(AudioStreamInfo, IamfGetCodecStringForSimpleProfilesAndFlac) {
  const uint8_t audio_object_type =
      ((kSimpleProfile << 6) |               // primary_profile
       ((kSimpleProfile << 4) & 0x3F) |      // additional_profile
       ((kCodecFlac - kCodecAudio) & 0xF));  // IAMF codec

  std::string codec_string =
      AudioStreamInfo::GetCodecString(kCodecIAMF, audio_object_type);
  EXPECT_EQ("iamf.000.000.fLaC", codec_string);
}

TEST(AudioStreamInfo, IamfGetCodecStringForBaseProfilesAndPcm) {
  const uint8_t audio_object_type =
      ((kBaseProfile << 6) |                // primary_profile = 1
       ((kBaseProfile << 4) & 0x3F) |       // additional_profile = 1
       ((kCodecPcm - kCodecAudio) & 0xF));  // IAMF codec

  std::string codec_string =
      AudioStreamInfo::GetCodecString(kCodecIAMF, audio_object_type);
  EXPECT_EQ("iamf.001.001.ipcm", codec_string);
}

}  // namespace media
}  // namespace shaka
