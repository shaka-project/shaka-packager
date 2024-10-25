// Copyright 2024 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/media/codecs/iamf_audio_util.h>

#include <gtest/gtest.h>

#include <packager/media/base/stream_info.h>
#include <packager/media/test/test_data_util.h>

namespace shaka {
namespace media {

namespace {

const int kSimpleProfile = 0;
const int kBaseProfile = 1;

const std::vector<uint8_t> kIacbBase = {
    0x01,  // configurationVersion
    0x20   // configOBUs_size
};

const std::vector<uint8_t> kSimpleIaSequenceObu = {
    // OBU header
    0xf8,  // obu_type (5 bits), obu_redundant_copy (1 bit),
           // obu_trimming_status_flag (1 bit), obu_extension_flag (1 bit)
    0x06,  // obu_size
    // IASequenceHeaderOBU
    0x69, 0x61, 0x6d, 0x66,  // ia_code
    0x00,                    // primary_profile = simple
    0x00                     // additional_profile = simple
};

const std::vector<uint8_t> kOpusCodecConfigObu = {
    // OBU header
    0x00,  // obu_type (5 bits), obu_redundant_copy (1 bit),
           // obu_trimming_status_flag (1 bit), obu_extension_flag (1 bit)
    0x0A,  // obu_size
    // CodecConfigOBU
    0xc8, 0x01,              // codec_config_id
    0x4f, 0x70, 0x75, 0x73,  // codec_id = 'Opus'
    0x46, 0x41, 0x4B, 0x45   // 'F''A''K''E' remainder codec config OBU
};

const std::vector<uint8_t> kFakeAudioElementObu = {
    // OBU header
    0x08,  // obu_type (5 bits), obu_redundant_copy (1 bit),
           // obu_trimming_status_flag (1 bit), obu_extension_flag (1 bit)
    0x04,  // obu_size
    // 'F''A''K''E' AudioElementOBU
    0x46, 0x41, 0x4B, 0x45};

const std::vector<uint8_t> kFakeMixPresentationObu = {
    // OBU header
    0x10,  // obu_type (5 bits), obu_redundant_copy (1 bit),
           // obu_trimming_status_flag (1 bit), obu_extension_flag (1 bit)
    0x04,  // obu_size
    // 'F''A''K''E' MixPresentationOBU
    0x46, 0x41, 0x4B, 0x45};

}  // namespace

TEST(IamfAudioUtilTest, GetCodecStringInfoWithSimpleProfiles) {
  std::vector<uint8_t> iacb;
  iacb.insert(iacb.end(), kIacbBase.begin(), kIacbBase.end());
  iacb.insert(iacb.end(), kSimpleIaSequenceObu.begin(),
              kSimpleIaSequenceObu.end());
  iacb.insert(iacb.end(), kOpusCodecConfigObu.begin(),
              kOpusCodecConfigObu.end());
  iacb.insert(iacb.end(), kFakeAudioElementObu.begin(),
              kFakeAudioElementObu.end());
  iacb.insert(iacb.end(), kFakeMixPresentationObu.begin(),
              kFakeMixPresentationObu.end());

  uint8_t codec_string_info;
  uint8_t expected_codec_string_info =
      ((kSimpleProfile << 6) |               // primary_profile
       ((kSimpleProfile << 4) & 0x3F) |      // additional_profile
       ((kCodecOpus - kCodecAudio) & 0xF));  // IAMF codec

  ASSERT_TRUE(GetIamfCodecStringInfo(iacb, codec_string_info));
  EXPECT_EQ(expected_codec_string_info, codec_string_info);
}

TEST(IamfAudioUtilTest, CodecStringInfoTestWithBaseProfiles) {
  const std::vector<uint8_t> base_ia_sequence_obu = {
      // OBU header
      0xf8,  // obu_type (5 bits), obu_redundant_copy (1 bit),
             // obu_trimming_status_flag (1 bit), obu_extension_flag (1 bit)
      0x06,  // obu_size
      // IASequenceHeaderOBU
      0x69, 0x61, 0x6d, 0x66,  // ia_code
      0x01,                    // primary_profile = base
      0x01                     // additional_profile = base
  };

  std::vector<uint8_t> iacb;
  iacb.insert(iacb.end(), kIacbBase.begin(), kIacbBase.end());
  iacb.insert(iacb.end(), base_ia_sequence_obu.begin(),
              base_ia_sequence_obu.end());
  iacb.insert(iacb.end(), kOpusCodecConfigObu.begin(),
              kOpusCodecConfigObu.end());
  iacb.insert(iacb.end(), kFakeAudioElementObu.begin(),
              kFakeAudioElementObu.end());
  iacb.insert(iacb.end(), kFakeMixPresentationObu.begin(),
              kFakeMixPresentationObu.end());

  uint8_t codec_string_info;
  uint8_t expected_codec_string_info =
      ((kBaseProfile << 6) |                 // primary_profile = 1
       ((kBaseProfile << 4) & 0x3F) |        // additional_profile = 1
       ((kCodecOpus - kCodecAudio) & 0xF));  // IAMF codec

  ASSERT_TRUE(GetIamfCodecStringInfo(iacb, codec_string_info));
  EXPECT_EQ(expected_codec_string_info, codec_string_info);
}

TEST(IamfAudioUtilTest, CodecStringInfoWithPcm) {
  const std::vector<uint8_t> pcm_codec_config_obu = {
      // OBU header
      0x00,  // obu_type (5 bits), obu_redundant_copy (1 bit),
             // obu_trimming_status_flag (1 bit), obu_extension_flag (1 bit)
      0x0A,  // obu_size
      // CodecConfigOBU
      0xc8, 0x01,              // codec_config_id
      0x69, 0x70, 0x63, 0x6d,  // codec_id = 'ipcm'
      0x46, 0x41, 0x4B, 0x45   // 'F''A''K''E' remainder codec config OBU
  };

  std::vector<uint8_t> iacb;
  iacb.insert(iacb.end(), kIacbBase.begin(), kIacbBase.end());
  iacb.insert(iacb.end(), kSimpleIaSequenceObu.begin(),
              kSimpleIaSequenceObu.end());
  iacb.insert(iacb.end(), pcm_codec_config_obu.begin(),
              pcm_codec_config_obu.end());
  iacb.insert(iacb.end(), kFakeAudioElementObu.begin(),
              kFakeAudioElementObu.end());
  iacb.insert(iacb.end(), kFakeMixPresentationObu.begin(),
              kFakeMixPresentationObu.end());

  uint8_t codec_string_info;
  uint8_t expected_codec_string_info =
      ((kSimpleProfile << 6) |              // primary_profile
       ((kSimpleProfile << 4) & 0x3F) |     // additional_profile
       ((kCodecPcm - kCodecAudio) & 0xF));  // IAMF codec

  ASSERT_TRUE(GetIamfCodecStringInfo(iacb, codec_string_info));
  EXPECT_EQ(expected_codec_string_info, codec_string_info);
}

TEST(IamfAudioUtilTest, CodecStringInfoWithMp4a) {
  const std::vector<uint8_t> mp4a_codec_config_obu = {
      // OBU header
      0x00,  // obu_type (5 bits), obu_redundant_copy (1 bit),
             // obu_trimming_status_flag (1 bit), obu_extension_flag (1 bit)
      0x0A,  // obu_size
      // CodecConfigOBU
      0xc8, 0x01,              // codec_config_id
      0x6d, 0x70, 0x34, 0x61,  // codec_id = 'mp4a'
      0x46, 0x41, 0x4B, 0x45   // 'F''A''K''E' remainder codec config OBU
  };

  std::vector<uint8_t> iacb;
  iacb.insert(iacb.end(), kIacbBase.begin(), kIacbBase.end());
  iacb.insert(iacb.end(), kSimpleIaSequenceObu.begin(),
              kSimpleIaSequenceObu.end());
  iacb.insert(iacb.end(), mp4a_codec_config_obu.begin(),
              mp4a_codec_config_obu.end());
  iacb.insert(iacb.end(), kFakeAudioElementObu.begin(),
              kFakeAudioElementObu.end());
  iacb.insert(iacb.end(), kFakeMixPresentationObu.begin(),
              kFakeMixPresentationObu.end());

  uint8_t codec_string_info;
  uint8_t expected_codec_string_info =
      ((kSimpleProfile << 6) |              // primary_profile
       ((kSimpleProfile << 4) & 0x3F) |     // additional_profile
       ((kCodecAAC - kCodecAudio) & 0xF));  // IAMF codec

  ASSERT_TRUE(GetIamfCodecStringInfo(iacb, codec_string_info));
  EXPECT_EQ(expected_codec_string_info, codec_string_info);
}

TEST(IamfAudioUtilTest, CodecStringInfoWithFlac) {
  const std::vector<uint8_t> flac_codec_config_obu = {
      // OBU header
      0x00,  // obu_type (5 bits), obu_redundant_copy (1 bit),
             // obu_trimming_status_flag (1 bit), obu_extension_flag (1 bit)
      0x0A,  // obu_size
      // CodecConfigOBU
      0xc8, 0x01,              // codec_config_id
      0x66, 0x4C, 0x61, 0x43,  // codec_id = 'fLaC'
      0x46, 0x41, 0x4B, 0x45   // 'F''A''K''E' remainder codec config OBU
  };

  std::vector<uint8_t> iacb;
  iacb.insert(iacb.end(), kIacbBase.begin(), kIacbBase.end());
  iacb.insert(iacb.end(), kSimpleIaSequenceObu.begin(),
              kSimpleIaSequenceObu.end());
  iacb.insert(iacb.end(), flac_codec_config_obu.begin(),
              flac_codec_config_obu.end());
  iacb.insert(iacb.end(), kFakeAudioElementObu.begin(),
              kFakeAudioElementObu.end());
  iacb.insert(iacb.end(), kFakeMixPresentationObu.begin(),
              kFakeMixPresentationObu.end());

  uint8_t codec_string_info;
  uint8_t expected_codec_string_info =
      ((kSimpleProfile << 6) |               // primary_profile
       ((kSimpleProfile << 4) & 0x3F) |      // additional_profile
       ((kCodecFlac - kCodecAudio) & 0xF));  // IAMF codec

  ASSERT_TRUE(GetIamfCodecStringInfo(iacb, codec_string_info));
  EXPECT_EQ(expected_codec_string_info, codec_string_info);
}

}  // namespace media
}  // namespace shaka
