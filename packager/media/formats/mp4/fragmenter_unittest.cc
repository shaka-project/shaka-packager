// Copyright 2026 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/media/formats/mp4/fragmenter.h>

#include <cstdint>
#include <memory>
#include <vector>

#include <gtest/gtest.h>

#include <packager/media/base/encryption_config.h>
#include <packager/media/base/fourccs.h>
#include <packager/media/base/media_sample.h>
#include <packager/media/base/video_stream_info.h>
#include <packager/media/formats/mp4/box_definitions.h>
#include <packager/status/status_test_util.h>

namespace shaka {
namespace media {
namespace mp4 {
namespace {

const uint32_t kTrackId = 1;
const int32_t kTimeScale = 1000;
const int64_t kDuration = 10000;
const char kCodecString[] = "avc1.010101";
const uint8_t kCodecConfig[] = {0x01, 0x02, 0x03};
const uint16_t kWidth = 640;
const uint16_t kHeight = 360;
const uint32_t kPixelWidth = 1;
const uint32_t kPixelHeight = 1;
const uint8_t kColorPrimaries = 0;
const uint8_t kMatrixCoefficients = 0;
const uint8_t kTransferCharacteristics = 0;
const int16_t kTrickPlayFactor = 0;
const uint8_t kNaluLengthSize = 4;
const char kLanguage[] = "und";
const bool kEncrypted = true;

const uint8_t kSampleData[] = {0x00, 0x01, 0x02, 0x03};
const int64_t kSampleDuration = 100;

// The 1-based index of the clear sample description, which only exists when a
// clear lead makes the muxer emit an encrypted/clear pair.
const uint32_t kClearSampleDescriptionIndex = 2;

std::shared_ptr<VideoStreamInfo> CreateEncryptedVideoStreamInfo() {
  auto stream_info = std::make_shared<VideoStreamInfo>(
      kTrackId, kTimeScale, kDuration, kCodecH264,
      H26xStreamFormat::kUnSpecified, kCodecString, kCodecConfig,
      sizeof(kCodecConfig), kWidth, kHeight, kPixelWidth, kPixelHeight,
      kColorPrimaries, kMatrixCoefficients, kTransferCharacteristics,
      kTrickPlayFactor, kNaluLengthSize, kLanguage, kEncrypted);

  EncryptionConfig encryption_config;
  encryption_config.protection_scheme = FOURCC_cenc;
  stream_info->set_encryption_config(encryption_config);
  return stream_info;
}

// A sample with no DecryptConfig, i.e. one that was not sample-encrypted. This
// is what AES-128 produces, since it encrypts whole segments instead, and what
// the clear lead of a CENC stream produces.
std::shared_ptr<MediaSample> CreateClearSample() {
  auto sample = MediaSample::CopyFrom(kSampleData, sizeof(kSampleData),
                                      /* is_key_frame= */ true);
  sample->set_dts(0);
  sample->set_pts(0);
  sample->set_duration(kSampleDuration);
  return sample;
}

// Runs a fragment of unencrypted samples through a Fragmenter for a track with
// |num_sample_descriptions| entries in its sample description table, and
// returns the resulting tfhd sample_description_index.
uint32_t GetSampleDescriptionIndex(size_t num_sample_descriptions) {
  TrackFragment traf;
  Fragmenter fragmenter(CreateEncryptedVideoStreamInfo(), &traf,
                        /* edit_list_offset= */ 0, num_sample_descriptions);
  EXPECT_OK(fragmenter.AddSample(*CreateClearSample()));
  EXPECT_OK(fragmenter.FinalizeFragment());
  return traf.header.sample_description_index;
}

}  // namespace

// A track with a clear lead has both an encrypted and a clear sample
// description, so a fragment that is not encrypted points at the clear one.
TEST(FragmenterTest, ClearFragmentUsesClearSampleDescription) {
  EXPECT_EQ(kClearSampleDescriptionIndex, GetSampleDescriptionIndex(2));
}

// Without that pair there is only one entry to point at. AES-128 leaves the
// samples clear and generates no encrypted sample description, so every one of
// its fragments took the branch above and referenced an entry past the end of
// the table, which players resolving it choke on.
// See https://github.com/shaka-project/shaka-packager/issues/1616.
TEST(FragmenterTest, ClearFragmentStaysInRangeWithOneSampleDescription) {
  EXPECT_EQ(1u, GetSampleDescriptionIndex(1));
}

}  // namespace mp4
}  // namespace media
}  // namespace shaka
