// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <packager/media/formats/mp4/track_run_iterator.h>

#include <cstdint>
#include <memory>

#include <absl/flags/declare.h>
#include <absl/flags/flag.h>
#include <absl/log/log.h>
#include <gtest/gtest.h>

#include <packager/flag_saver.h>
#include <packager/media/formats/mp4/box_definitions.h>

ABSL_DECLARE_FLAG(bool, mp4_reset_initial_composition_offset_to_zero);

namespace {

// The sum of the elements in a vector initialized with SumAscending,
// less the value of the last element.
const int kSumAscending1 = 45;

const int kMovieScale = 1000;
const int kAudioScale = 48000;
const int kVideoScale = 25;

const uint8_t kFullSampleEncryptionFlag = 0;

const uint8_t kDefaultCryptByteBlock = 2;
const uint8_t kDefaultSkipByteBlock = 8;

const uint8_t kAuxInfo[] = {
    // Sample 1: IV (no subsumples).
    0x41, 0x54, 0x65, 0x73, 0x74, 0x49, 0x76, 0x31,
    // Sample 2: IV.
    0x41, 0x54, 0x65, 0x73, 0x74, 0x49, 0x76, 0x32,
    // Sample 2: Subsample count.
    0x00, 0x02,
    // Sample 2: Subsample 1.
    0x00, 0x01, 0x00, 0x00, 0x00, 0x02,
    // Sample 2: Subsample 2.
    0x00, 0x03, 0x00, 0x00, 0x00, 0x04,
};

const uint8_t kSampleEncryptionDataWithSubsamples[] = {
    // Sample count.
    0x00, 0x00, 0x00, 0x02,
    // Sample 1: IV.
    0x41, 0x54, 0x65, 0x73, 0x74, 0x49, 0x76, 0x31,
    // Sample 1: Subsample count.
    0x00, 0x01,
    // Sample 1: Subsample 1.
    0x00, 0x01, 0x00, 0x00, 0x00, 0x02,
    // Sample 2: IV.
    0x41, 0x54, 0x65, 0x73, 0x74, 0x49, 0x76, 0x32,
    // Sample 2: Subsample count.
    0x00, 0x02,
    // Sample 2: Subsample 1.
    0x00, 0x01, 0x00, 0x00, 0x00, 0x02,
    // Sample 2: Subsample 2.
    0x00, 0x03, 0x00, 0x00, 0x00, 0x04,
};

const uint8_t kSampleEncryptionDataWithoutSubsamples[] = {
    // Sample count.
    0x00, 0x00, 0x00, 0x02,
    // Sample 1: IV.
    0x41, 0x54, 0x65, 0x73, 0x74, 0x49, 0x76, 0x31,
    // Sample 2: IV.
    0x41, 0x54, 0x65, 0x73, 0x74, 0x49, 0x76, 0x32,
};

const uint8_t kSampleEncryptionDataWithConstantIvAndSubsamples[] = {
    // Sample count.
    0x00, 0x00, 0x00, 0x02,
    // Sample 1: Subsample count.
    0x00, 0x01,
    // Sample 1: Subsample 1.
    0x00, 0x01, 0x00, 0x00, 0x00, 0x02,
    // Sample 2: Subsample count.
    0x00, 0x02,
    // Sample 2: Subsample 1.
    0x00, 0x01, 0x00, 0x00, 0x00, 0x02,
    // Sample 2: Subsample 2.
    0x00, 0x03, 0x00, 0x00, 0x00, 0x04,
};

const uint8_t kSampleEncryptionDataWithConstantIvWithoutSubsamples[] = {
    // Sample count.
    0x00, 0x00, 0x00, 0x02,
};

const char kIv1[] = {0x41, 0x54, 0x65, 0x73, 0x74, 0x49, 0x76, 0x31};
const char kIv2[] = {0x41, 0x54, 0x65, 0x73, 0x74, 0x49, 0x76, 0x32};
const char kConstantIv[] = {0x41, 0x54, 0x65, 0x73, 0x74, 0x49, 0x76, 0x33};

const uint8_t kKeyId[] = {
    0x41, 0x47, 0x6f, 0x6f, 0x67, 0x6c, 0x65, 0x54,
    0x65, 0x73, 0x74, 0x4b, 0x65, 0x79, 0x49, 0x44,
};

}  // namespace

namespace shaka {
namespace media {
namespace mp4 {

class TrackRunIteratorTest : public testing::Test {
 public:
  TrackRunIteratorTest() { CreateMovie(); }

 protected:
  Movie moov_;
  std::unique_ptr<TrackRunIterator> iter_;

  void CreateMovie() {
    moov_.header.timescale = kMovieScale;
    moov_.tracks.resize(3);
    moov_.extends.tracks.resize(2);
    moov_.tracks[0].header.track_id = 1;
    moov_.tracks[0].media.header.timescale = kAudioScale;
    SampleDescription& desc1 =
        moov_.tracks[0].media.information.sample_table.description;
    AudioSampleEntry aud_desc;
    aud_desc.format = FOURCC_mp4a;
    desc1.type = kAudio;
    desc1.audio_entries.push_back(aud_desc);
    moov_.extends.tracks[0].track_id = 1;
    moov_.extends.tracks[0].default_sample_description_index = 1;

    moov_.tracks[1].header.track_id = 2;
    moov_.tracks[1].media.header.timescale = kVideoScale;
    SampleDescription& desc2 =
        moov_.tracks[1].media.information.sample_table.description;
    VideoSampleEntry vid_desc;
    vid_desc.format = FOURCC_avc1;
    desc2.type = kVideo;
    desc2.video_entries.push_back(vid_desc);
    moov_.extends.tracks[1].track_id = 2;
    moov_.extends.tracks[1].default_sample_description_index = 1;

    moov_.tracks[2].header.track_id = 3;
    moov_.tracks[2].media.information.sample_table.description.type = kHint;
  }

  MovieFragment CreateFragment() {
    MovieFragment moof;
    moof.tracks.resize(2);
    moof.tracks[0].decode_time.decode_time = 0;
    moof.tracks[0].header.track_id = 1;
    moof.tracks[0].header.flags =
        TrackFragmentHeader::kDefaultSampleFlagsPresentMask;
    moof.tracks[0].header.default_sample_duration = 1024;
    moof.tracks[0].header.default_sample_size = 4;
    moof.tracks[0].runs.resize(2);
    moof.tracks[0].runs[0].sample_count = 10;
    moof.tracks[0].runs[0].data_offset = 100;
    SetAscending(&moof.tracks[0].runs[0].sample_sizes);

    moof.tracks[0].runs[1].sample_count = 10;
    moof.tracks[0].runs[1].data_offset = 10000;

    moof.tracks[1].header.track_id = 2;
    moof.tracks[1].header.flags = 0;
    moof.tracks[1].decode_time.decode_time = 10;
    moof.tracks[1].runs.resize(1);
    moof.tracks[1].runs[0].sample_count = 10;
    moof.tracks[1].runs[0].data_offset = 200;
    SetAscending(&moof.tracks[1].runs[0].sample_sizes);
    SetAscending(&moof.tracks[1].runs[0].sample_durations);
    moof.tracks[1].runs[0].sample_flags.resize(10);
    for (size_t i = 1; i < moof.tracks[1].runs[0].sample_flags.size(); i++) {
      moof.tracks[1].runs[0].sample_flags[i] =
          TrackFragmentHeader::kNonKeySampleMask;
    }

    return moof;
  }

  // Update the first sample description of a Track to indicate encryption
  void AddEncryption(FourCC protection_scheme, Track* track) {
    SampleDescription* stsd =
        &track->media.information.sample_table.description;
    ProtectionSchemeInfo* sinf;
    if (!stsd->video_entries.empty()) {
      sinf = &stsd->video_entries[0].sinf;
    } else {
      sinf = &stsd->audio_entries[0].sinf;
    }

    sinf->type.type = protection_scheme;
    sinf->info.track_encryption.default_is_protected = 1;
    // Use constant IV for CBCS protection scheme.
    if (protection_scheme == FOURCC_cbcs) {
      sinf->info.track_encryption.default_per_sample_iv_size = 0;
      sinf->info.track_encryption.default_constant_iv.assign(
          kConstantIv, kConstantIv + std::size(kConstantIv));
      sinf->info.track_encryption.default_crypt_byte_block =
          kDefaultCryptByteBlock;
      sinf->info.track_encryption.default_skip_byte_block =
          kDefaultSkipByteBlock;
    } else {
      sinf->info.track_encryption.default_per_sample_iv_size = 8;
    }
    sinf->info.track_encryption.default_kid.assign(kKeyId,
                                                   kKeyId + std::size(kKeyId));
  }

  // Add aux info covering the first track run to a TrackFragment, and update
  // the run to ensure it matches length and subsample information.
  void AddAuxInfoHeaders(int offset, TrackFragment* frag) {
    frag->auxiliary_offset.offsets.push_back(offset);
    frag->auxiliary_size.sample_count = 2;
    frag->auxiliary_size.sample_info_sizes.push_back(8);
    frag->auxiliary_size.sample_info_sizes.push_back(22);
    frag->runs[0].sample_count = 2;
    frag->runs[0].sample_sizes[1] = 10;
  }

  void AddSampleEncryption(uint8_t use_subsample_flag, TrackFragment* frag) {
    frag->sample_encryption.iv_size = 8;
    frag->sample_encryption.flags = use_subsample_flag;
    if (use_subsample_flag) {
      frag->sample_encryption.sample_encryption_data.assign(
          kSampleEncryptionDataWithSubsamples,
          kSampleEncryptionDataWithSubsamples +
              std::size(kSampleEncryptionDataWithSubsamples));
    } else {
      frag->sample_encryption.sample_encryption_data.assign(
          kSampleEncryptionDataWithoutSubsamples,
          kSampleEncryptionDataWithoutSubsamples +
              std::size(kSampleEncryptionDataWithoutSubsamples));
    }

    // Update sample sizes and aux info header.
    frag->runs.resize(1);
    frag->runs[0].sample_count = 2;
    frag->auxiliary_offset.offsets.push_back(0);
    frag->auxiliary_size.sample_count = 2;
    if (use_subsample_flag) {
      // Update sample sizes to match with subsample entries above.
      frag->runs[0].sample_sizes[0] = 3;
      frag->runs[0].sample_sizes[1] = 10;
      // Set aux info header.
      frag->auxiliary_size.sample_info_sizes.push_back(16);
      frag->auxiliary_size.sample_info_sizes.push_back(22);
    } else {
      frag->auxiliary_size.default_sample_info_size = 8;
    }
  }

  void AddSampleEncryptionWithConstantIv(uint8_t use_subsample_flag,
                                         TrackFragment* frag) {
    frag->sample_encryption.iv_size = 0;
    frag->sample_encryption.flags = use_subsample_flag;
    if (use_subsample_flag) {
      frag->sample_encryption.sample_encryption_data.assign(
          kSampleEncryptionDataWithConstantIvAndSubsamples,
          kSampleEncryptionDataWithConstantIvAndSubsamples +
              std::size(kSampleEncryptionDataWithConstantIvAndSubsamples));
    } else {
      frag->sample_encryption.sample_encryption_data.assign(
          kSampleEncryptionDataWithConstantIvWithoutSubsamples,
          kSampleEncryptionDataWithConstantIvWithoutSubsamples +
              std::size(kSampleEncryptionDataWithConstantIvWithoutSubsamples));
    }

    // Update sample sizes and aux info header.
    frag->runs.resize(1);
    frag->runs[0].sample_count = 2;
    if (use_subsample_flag) {
      // Update sample sizes to match with subsample entries above.
      frag->runs[0].sample_sizes[0] = 3;
      frag->runs[0].sample_sizes[1] = 10;
      // Set aux info header.
      frag->auxiliary_offset.offsets.push_back(0);
      frag->auxiliary_size.sample_count = 2;
      frag->auxiliary_size.sample_info_sizes.push_back(16);
      frag->auxiliary_size.sample_info_sizes.push_back(22);
    } else {
      // No aux info needed for constant iv and full sample encryption.
    }
  }

  void SetAscending(std::vector<uint32_t>* vec) {
    vec->resize(10);
    for (size_t i = 0; i < vec->size(); i++)
      (*vec)[i] = static_cast<uint32_t>(i + 1);
  }
};

TEST_F(TrackRunIteratorTest, NoRunsTest) {
  iter_.reset(new TrackRunIterator(&moov_));
  ASSERT_TRUE(iter_->Init(MovieFragment()));
  EXPECT_FALSE(iter_->IsRunValid());
  EXPECT_FALSE(iter_->IsSampleValid());
}

TEST_F(TrackRunIteratorTest, BasicOperationTest) {
  iter_.reset(new TrackRunIterator(&moov_));
  MovieFragment moof = CreateFragment();

  // Test that runs are sorted correctly, and that properties of the initial
  // sample of the first run are correct
  ASSERT_TRUE(iter_->Init(moof));
  EXPECT_TRUE(iter_->IsRunValid());
  EXPECT_FALSE(iter_->is_encrypted());
  EXPECT_EQ(iter_->track_id(), 1u);
  EXPECT_EQ(iter_->sample_offset(), 100);
  EXPECT_EQ(iter_->sample_size(), 1);
  EXPECT_EQ(iter_->dts(), 0);
  EXPECT_EQ(iter_->cts(), 0);
  EXPECT_EQ(iter_->duration(), 1024);
  EXPECT_TRUE(iter_->is_keyframe());

  // Advance to the last sample in the current run, and test its properties
  for (int i = 0; i < 9; i++)
    iter_->AdvanceSample();
  EXPECT_EQ(iter_->track_id(), 1u);
  EXPECT_EQ(iter_->sample_offset(), 100 + kSumAscending1);
  EXPECT_EQ(iter_->sample_size(), 10);
  EXPECT_EQ(iter_->dts(), 1024 * 9);
  EXPECT_EQ(iter_->duration(), 1024);
  EXPECT_TRUE(iter_->is_keyframe());

  // Test end-of-run
  iter_->AdvanceSample();
  EXPECT_FALSE(iter_->IsSampleValid());

  // Test last sample of next run
  iter_->AdvanceRun();
  EXPECT_TRUE(iter_->is_keyframe());
  for (int i = 0; i < 9; i++)
    iter_->AdvanceSample();
  EXPECT_EQ(iter_->track_id(), 2u);
  EXPECT_EQ(iter_->sample_offset(), 200 + kSumAscending1);
  EXPECT_EQ(iter_->sample_size(), 10);
  int64_t base_dts = kSumAscending1 + moof.tracks[1].decode_time.decode_time;
  EXPECT_EQ(iter_->dts(), base_dts);
  EXPECT_EQ(iter_->duration(), 10);
  EXPECT_FALSE(iter_->is_keyframe());

  // Test final run
  iter_->AdvanceRun();
  EXPECT_EQ(iter_->track_id(), 1u);
  EXPECT_EQ(iter_->dts(), 1024 * 10);
  iter_->AdvanceSample();
  EXPECT_EQ(moof.tracks[0].runs[1].data_offset +
                moof.tracks[0].header.default_sample_size,
            iter_->sample_offset());
  iter_->AdvanceRun();
  EXPECT_FALSE(iter_->IsRunValid());
}

TEST_F(TrackRunIteratorTest, TrackExtendsDefaultsTest) {
  moov_.extends.tracks[0].default_sample_duration = 50;
  moov_.extends.tracks[0].default_sample_size = 3;
  moov_.extends.tracks[0].default_sample_flags =
      TrackFragmentHeader::kNonKeySampleMask;
  iter_.reset(new TrackRunIterator(&moov_));
  MovieFragment moof = CreateFragment();
  moof.tracks[0].header.flags = 0;
  moof.tracks[0].header.default_sample_size = 0;
  moof.tracks[0].header.default_sample_duration = 0;
  moof.tracks[0].runs[0].sample_sizes.clear();
  ASSERT_TRUE(iter_->Init(moof));
  iter_->AdvanceSample();
  EXPECT_FALSE(iter_->is_keyframe());
  EXPECT_EQ(iter_->sample_size(), 3);
  EXPECT_EQ(iter_->sample_offset(), moof.tracks[0].runs[0].data_offset + 3);
  EXPECT_EQ(iter_->duration(), 50);
  EXPECT_EQ(iter_->dts(), 50);
}

TEST_F(TrackRunIteratorTest, FirstSampleFlagTest) {
  // Ensure that keyframes are flagged correctly in the face of BMFF boxes which
  // explicitly specify the flags for the first sample in a run and rely on
  // defaults for all subsequent samples
  iter_.reset(new TrackRunIterator(&moov_));
  MovieFragment moof = CreateFragment();
  moof.tracks[1].header.flags =
      TrackFragmentHeader::kDefaultSampleFlagsPresentMask;
  moof.tracks[1].header.default_sample_flags =
      TrackFragmentHeader::kNonKeySampleMask;
  moof.tracks[1].runs[0].sample_flags.resize(1);
  ASSERT_TRUE(iter_->Init(moof));
  iter_->AdvanceRun();
  EXPECT_TRUE(iter_->is_keyframe());
  iter_->AdvanceSample();
  EXPECT_FALSE(iter_->is_keyframe());
}

TEST_F(TrackRunIteratorTest, EmptyEditTest) {
  iter_.reset(new TrackRunIterator(&moov_));

  EditListEntry entry;
  entry.segment_duration = 2 * kMovieScale;
  entry.media_time = -1;
  entry.media_rate_integer = 1;
  entry.media_rate_fraction = 0;
  moov_.tracks[1].edit.list.edits.push_back(entry);

  MovieFragment moof = CreateFragment();
  moof.tracks[1].decode_time.decode_time = 0;

  ASSERT_TRUE(iter_->Init(moof));
  iter_->AdvanceRun();
  EXPECT_EQ(iter_->dts(), 2 * kVideoScale);
  EXPECT_EQ(iter_->cts(), 2 * kVideoScale);
}

TEST_F(TrackRunIteratorTest, NormalEditTest) {
  iter_.reset(new TrackRunIterator(&moov_));

  const int kMediaTime = 5;

  EditListEntry entry;
  entry.segment_duration = 0;
  entry.media_time = kMediaTime;
  entry.media_rate_integer = 1;
  entry.media_rate_fraction = 0;
  moov_.tracks[1].edit.list.edits.push_back(entry);

  MovieFragment moof = CreateFragment();
  moof.tracks[1].decode_time.decode_time = 0;

  ASSERT_TRUE(iter_->Init(moof));
  iter_->AdvanceRun();
  EXPECT_EQ(iter_->dts(), -kMediaTime);
  EXPECT_EQ(iter_->cts(), -kMediaTime);
}

TEST_F(TrackRunIteratorTest, ReorderingTest) {
  FlagSaver<bool> saver(&FLAGS_mp4_reset_initial_composition_offset_to_zero);
  absl::SetFlag(&FLAGS_mp4_reset_initial_composition_offset_to_zero, false);

  // Test frame reordering. The frames have the following
  // decode timestamps:
  //
  //   0ms 40ms   120ms     240ms
  //   | 0 | 1  - | 2  -  - |
  //
  // ...and these composition timestamps, after edit list adjustment:
  //
  //   0ms 40ms       160ms  240ms
  //   | 0 | 2  -  -  | 1 - |
  iter_.reset(new TrackRunIterator(&moov_));

  // Add CTS offsets. Without bias, the CTS offsets for the first three frames
  // would simply be [0, 3, -2]. Since CTS offsets should be non-negative for
  // maximum compatibility, these values are biased up to [2, 5, 0].
  MovieFragment moof = CreateFragment();
  std::vector<int64_t>& cts_offsets =
      moof.tracks[1].runs[0].sample_composition_time_offsets;
  cts_offsets.resize(10);
  cts_offsets[0] = 2;
  cts_offsets[1] = 5;
  cts_offsets[2] = 0;
  moof.tracks[1].decode_time.decode_time = 0;

  ASSERT_TRUE(iter_->Init(moof));
  iter_->AdvanceRun();
  EXPECT_EQ(iter_->dts(), 0);
  EXPECT_EQ(iter_->cts(), 2);
  EXPECT_EQ(iter_->duration(), 1);
  iter_->AdvanceSample();
  EXPECT_EQ(iter_->dts(), 1);
  EXPECT_EQ(iter_->cts(), 6);
  EXPECT_EQ(iter_->duration(), 2);
  iter_->AdvanceSample();
  EXPECT_EQ(iter_->dts(), 3);
  EXPECT_EQ(iter_->cts(), 3);
  EXPECT_EQ(iter_->duration(), 3);
}

TEST_F(TrackRunIteratorTest, ReorderingTest_WithEditList) {
  FlagSaver<bool> saver(&FLAGS_mp4_reset_initial_composition_offset_to_zero);
  absl::SetFlag(&FLAGS_mp4_reset_initial_composition_offset_to_zero, false);

  // See the test above for background.
  iter_.reset(new TrackRunIterator(&moov_));
  MovieFragment moof = CreateFragment();
  std::vector<int64_t>& cts_offsets =
      moof.tracks[1].runs[0].sample_composition_time_offsets;
  cts_offsets.resize(10);
  cts_offsets[0] = 2;
  cts_offsets[1] = 5;
  cts_offsets[2] = 0;
  moof.tracks[1].decode_time.decode_time = 0;

  EditListEntry entry;
  entry.segment_duration = 0;
  entry.media_time = 2;
  entry.media_rate_integer = 1;
  entry.media_rate_fraction = 0;
  moov_.tracks[1].edit.list.edits.push_back(entry);

  ASSERT_TRUE(iter_->Init(moof));
  iter_->AdvanceRun();
  EXPECT_EQ(iter_->dts(), -2);
  EXPECT_EQ(iter_->cts(), 0);
  EXPECT_EQ(iter_->duration(), 1);
  iter_->AdvanceSample();
  EXPECT_EQ(iter_->dts(), -1);
  EXPECT_EQ(iter_->cts(), 4);
  EXPECT_EQ(iter_->duration(), 2);
  iter_->AdvanceSample();
  EXPECT_EQ(iter_->dts(), 1);
  EXPECT_EQ(iter_->cts(), 1);
  EXPECT_EQ(iter_->duration(), 3);
}

TEST_F(TrackRunIteratorTest, ReorderingTest_ResetInitialCompositionOffset) {
  FlagSaver<bool> saver(&FLAGS_mp4_reset_initial_composition_offset_to_zero);
  absl::SetFlag(&FLAGS_mp4_reset_initial_composition_offset_to_zero, true);

  // See the test above for background.
  iter_.reset(new TrackRunIterator(&moov_));
  MovieFragment moof = CreateFragment();
  std::vector<int64_t>& cts_offsets =
      moof.tracks[1].runs[0].sample_composition_time_offsets;
  cts_offsets.resize(10);
  cts_offsets[0] = 2;
  cts_offsets[1] = 5;
  cts_offsets[2] = 0;
  moof.tracks[1].decode_time.decode_time = 0;

  ASSERT_TRUE(iter_->Init(moof));
  iter_->AdvanceRun();
  EXPECT_EQ(iter_->dts(), -2);
  EXPECT_EQ(iter_->cts(), 0);
  EXPECT_EQ(iter_->duration(), 1);
  iter_->AdvanceSample();
  EXPECT_EQ(iter_->dts(), -1);
  EXPECT_EQ(iter_->cts(), 4);
  EXPECT_EQ(iter_->duration(), 2);
  iter_->AdvanceSample();
  EXPECT_EQ(iter_->dts(), 1);
  EXPECT_EQ(iter_->cts(), 1);
  EXPECT_EQ(iter_->duration(), 3);
}

TEST_F(TrackRunIteratorTest, IgnoreUnknownAuxInfoTest) {
  iter_.reset(new TrackRunIterator(&moov_));
  MovieFragment moof = CreateFragment();
  moof.tracks[1].auxiliary_offset.offsets.push_back(50);
  moof.tracks[1].auxiliary_size.default_sample_info_size = 2;
  moof.tracks[1].auxiliary_size.sample_count = 2;
  moof.tracks[1].runs[0].sample_count = 2;
  ASSERT_TRUE(iter_->Init(moof));
  iter_->AdvanceRun();
  EXPECT_FALSE(iter_->AuxInfoNeedsToBeCached());
}

TEST_F(TrackRunIteratorTest,
       DecryptConfigTestWithSampleEncryptionAndSubsample) {
  AddEncryption(FOURCC_cenc, &moov_.tracks[1]);
  iter_.reset(new TrackRunIterator(&moov_));

  MovieFragment moof = CreateFragment();
  AddSampleEncryption(SampleEncryption::kUseSubsampleEncryption,
                      &moof.tracks[1]);

  ASSERT_TRUE(iter_->Init(moof));
  // The run for track 2 will be the second, which is parsed according to
  // data_offset.
  iter_->AdvanceRun();
  EXPECT_EQ(iter_->track_id(), 2u);

  EXPECT_TRUE(iter_->is_encrypted());
  // No need to cache aux info as it is already available in SampleEncryption.
  EXPECT_FALSE(iter_->AuxInfoNeedsToBeCached());
  EXPECT_EQ(iter_->aux_info_size(), 0);
  EXPECT_EQ(iter_->sample_offset(), 200);
  EXPECT_EQ(iter_->GetMaxClearOffset(), moof.tracks[1].runs[0].data_offset);
  std::unique_ptr<DecryptConfig> config = iter_->GetDecryptConfig();
  EXPECT_EQ(std::vector<uint8_t>(kKeyId, kKeyId + std::size(kKeyId)),
            config->key_id());
  EXPECT_EQ(std::vector<uint8_t>(kIv1, kIv1 + std::size(kIv1)), config->iv());
  EXPECT_EQ(config->subsamples().size(), 1u);
  EXPECT_EQ(config->subsamples()[0].clear_bytes, 1u);
  EXPECT_EQ(config->subsamples()[0].cipher_bytes, 2u);
  iter_->AdvanceSample();
  config = iter_->GetDecryptConfig();
  EXPECT_EQ(std::vector<uint8_t>(kIv2, kIv2 + std::size(kIv2)), config->iv());
  EXPECT_EQ(config->subsamples().size(), 2u);
  EXPECT_EQ(config->subsamples()[0].clear_bytes, 1u);
  EXPECT_EQ(config->subsamples()[0].cipher_bytes, 2u);
  EXPECT_EQ(config->subsamples()[1].clear_bytes, 3u);
  EXPECT_EQ(config->subsamples()[1].cipher_bytes, 4u);
}

TEST_F(TrackRunIteratorTest,
       DecryptConfigTestWithSampleEncryptionAndNoSubsample) {
  AddEncryption(FOURCC_cenc, &moov_.tracks[1]);
  iter_.reset(new TrackRunIterator(&moov_));

  MovieFragment moof = CreateFragment();
  AddSampleEncryption(kFullSampleEncryptionFlag, &moof.tracks[1]);

  ASSERT_TRUE(iter_->Init(moof));
  // The run for track 2 will be the second, which is parsed according to
  // data_offset.
  iter_->AdvanceRun();
  EXPECT_EQ(iter_->track_id(), 2u);

  EXPECT_TRUE(iter_->is_encrypted());
  // No need to cache aux info as it is already available in SampleEncryption.
  EXPECT_FALSE(iter_->AuxInfoNeedsToBeCached());
  EXPECT_EQ(iter_->aux_info_size(), 0);
  EXPECT_EQ(iter_->sample_offset(), 200);
  EXPECT_EQ(iter_->GetMaxClearOffset(), moof.tracks[1].runs[0].data_offset);
  std::unique_ptr<DecryptConfig> config = iter_->GetDecryptConfig();
  EXPECT_EQ(std::vector<uint8_t>(kKeyId, kKeyId + std::size(kKeyId)),
            config->key_id());
  EXPECT_EQ(std::vector<uint8_t>(kIv1, kIv1 + std::size(kIv1)), config->iv());
  EXPECT_EQ(config->subsamples().size(), 0u);
  iter_->AdvanceSample();
  config = iter_->GetDecryptConfig();
  EXPECT_EQ(std::vector<uint8_t>(kIv2, kIv2 + std::size(kIv2)), config->iv());
  EXPECT_EQ(config->subsamples().size(), 0u);
}

TEST_F(TrackRunIteratorTest,
       DecryptConfigTestWithSampleEncryptionAndConstantIvAndSubsample) {
  AddEncryption(FOURCC_cbcs, &moov_.tracks[1]);
  iter_.reset(new TrackRunIterator(&moov_));

  MovieFragment moof = CreateFragment();
  AddSampleEncryptionWithConstantIv(SampleEncryption::kUseSubsampleEncryption,
                                    &moof.tracks[1]);

  ASSERT_TRUE(iter_->Init(moof));
  // The run for track 2 will be the second, which is parsed according to
  // data_offset.
  iter_->AdvanceRun();
  EXPECT_EQ(iter_->track_id(), 2u);

  EXPECT_TRUE(iter_->is_encrypted());
  EXPECT_EQ(iter_->sample_offset(), 200);
  EXPECT_EQ(iter_->GetMaxClearOffset(), moof.tracks[1].runs[0].data_offset);
  std::unique_ptr<DecryptConfig> config = iter_->GetDecryptConfig();
  EXPECT_EQ(FOURCC_cbcs, config->protection_scheme());
  EXPECT_EQ(kDefaultCryptByteBlock, config->crypt_byte_block());
  EXPECT_EQ(kDefaultSkipByteBlock, config->skip_byte_block());
  EXPECT_EQ(std::vector<uint8_t>(kKeyId, kKeyId + std::size(kKeyId)),
            config->key_id());
  EXPECT_EQ(
      std::vector<uint8_t>(kConstantIv, kConstantIv + std::size(kConstantIv)),
      config->iv());
  EXPECT_EQ(config->subsamples().size(), 1u);
  EXPECT_EQ(config->subsamples()[0].clear_bytes, 1u);
  EXPECT_EQ(config->subsamples()[0].cipher_bytes, 2u);
  iter_->AdvanceSample();
  config = iter_->GetDecryptConfig();
  EXPECT_EQ(
      std::vector<uint8_t>(kConstantIv, kConstantIv + std::size(kConstantIv)),
      config->iv());
  EXPECT_EQ(config->subsamples().size(), 2u);
  EXPECT_EQ(config->subsamples()[0].clear_bytes, 1u);
  EXPECT_EQ(config->subsamples()[0].cipher_bytes, 2u);
  EXPECT_EQ(config->subsamples()[1].clear_bytes, 3u);
  EXPECT_EQ(config->subsamples()[1].cipher_bytes, 4u);
}

TEST_F(TrackRunIteratorTest,
       DecryptConfigTestWithSampleEncryptionAndConstantIvAndNoSubsample) {
  AddEncryption(FOURCC_cbcs, &moov_.tracks[1]);
  iter_.reset(new TrackRunIterator(&moov_));

  MovieFragment moof = CreateFragment();
  AddSampleEncryptionWithConstantIv(kFullSampleEncryptionFlag, &moof.tracks[1]);

  ASSERT_TRUE(iter_->Init(moof));
  // The run for track 2 will be the second, which is parsed according to
  // data_offset.
  iter_->AdvanceRun();
  EXPECT_EQ(iter_->track_id(), 2u);

  EXPECT_TRUE(iter_->is_encrypted());
  EXPECT_EQ(iter_->sample_offset(), 200);
  EXPECT_EQ(iter_->GetMaxClearOffset(), moof.tracks[1].runs[0].data_offset);
  std::unique_ptr<DecryptConfig> config = iter_->GetDecryptConfig();
  EXPECT_EQ(FOURCC_cbcs, config->protection_scheme());
  EXPECT_EQ(std::vector<uint8_t>(kKeyId, kKeyId + std::size(kKeyId)),
            config->key_id());
  EXPECT_EQ(
      std::vector<uint8_t>(kConstantIv, kConstantIv + std::size(kConstantIv)),
      config->iv());
  EXPECT_EQ(config->subsamples().size(), 0u);
  iter_->AdvanceSample();
  config = iter_->GetDecryptConfig();
  EXPECT_EQ(
      std::vector<uint8_t>(kConstantIv, kConstantIv + std::size(kConstantIv)),
      config->iv());
  EXPECT_EQ(config->subsamples().size(), 0u);
}

TEST_F(TrackRunIteratorTest, DecryptConfigTestWithAuxInfo) {
  AddEncryption(FOURCC_cenc, &moov_.tracks[1]);
  iter_.reset(new TrackRunIterator(&moov_));

  MovieFragment moof = CreateFragment();
  AddAuxInfoHeaders(50, &moof.tracks[1]);

  ASSERT_TRUE(iter_->Init(moof));

  // The run for track 2 will be first, since its aux info offset is the first
  // element in the file.
  EXPECT_EQ(iter_->track_id(), 2u);
  EXPECT_TRUE(iter_->is_encrypted());
  ASSERT_TRUE(iter_->AuxInfoNeedsToBeCached());
  EXPECT_EQ(static_cast<uint32_t>(iter_->aux_info_size()), std::size(kAuxInfo));
  EXPECT_EQ(iter_->aux_info_offset(), 50);
  EXPECT_EQ(iter_->GetMaxClearOffset(), 50);
  EXPECT_FALSE(iter_->CacheAuxInfo(NULL, 0));
  EXPECT_FALSE(iter_->CacheAuxInfo(kAuxInfo, 3));
  EXPECT_TRUE(iter_->AuxInfoNeedsToBeCached());
  EXPECT_TRUE(iter_->CacheAuxInfo(kAuxInfo, std::size(kAuxInfo)));
  EXPECT_FALSE(iter_->AuxInfoNeedsToBeCached());
  EXPECT_EQ(iter_->sample_offset(), 200);
  EXPECT_EQ(iter_->GetMaxClearOffset(), moof.tracks[0].runs[0].data_offset);
  std::unique_ptr<DecryptConfig> config = iter_->GetDecryptConfig();
  EXPECT_EQ(std::vector<uint8_t>(kKeyId, kKeyId + std::size(kKeyId)),
            config->key_id());
  EXPECT_EQ(std::vector<uint8_t>(kIv1, kIv1 + std::size(kIv1)), config->iv());
  EXPECT_TRUE(config->subsamples().empty());
  iter_->AdvanceSample();
  config = iter_->GetDecryptConfig();
  EXPECT_EQ(config->subsamples().size(), 2u);
  EXPECT_EQ(config->subsamples()[0].clear_bytes, 1u);
  EXPECT_EQ(config->subsamples()[1].cipher_bytes, 4u);
}

// It is legal for aux info blocks to be shared among multiple formats.
TEST_F(TrackRunIteratorTest, SharedAuxInfoTest) {
  AddEncryption(FOURCC_cenc, &moov_.tracks[0]);
  AddEncryption(FOURCC_cenc, &moov_.tracks[1]);
  iter_.reset(new TrackRunIterator(&moov_));

  MovieFragment moof = CreateFragment();
  moof.tracks[0].runs.resize(1);
  AddAuxInfoHeaders(50, &moof.tracks[0]);
  AddAuxInfoHeaders(50, &moof.tracks[1]);
  moof.tracks[0].auxiliary_size.default_sample_info_size = 8;

  ASSERT_TRUE(iter_->Init(moof));
  EXPECT_EQ(iter_->track_id(), 1u);
  EXPECT_EQ(iter_->aux_info_offset(), 50);
  EXPECT_TRUE(iter_->CacheAuxInfo(kAuxInfo, std::size(kAuxInfo)));
  std::unique_ptr<DecryptConfig> config = iter_->GetDecryptConfig();
  ASSERT_EQ(std::size(kIv1), config->iv().size());
  EXPECT_TRUE(!memcmp(kIv1, config->iv().data(), config->iv().size()));
  iter_->AdvanceSample();
  EXPECT_EQ(iter_->GetMaxClearOffset(), 50);
  iter_->AdvanceRun();
  EXPECT_EQ(iter_->GetMaxClearOffset(), 50);
  EXPECT_EQ(iter_->aux_info_offset(), 50);
  EXPECT_TRUE(iter_->CacheAuxInfo(kAuxInfo, std::size(kAuxInfo)));
  EXPECT_EQ(iter_->GetMaxClearOffset(), 200);
  ASSERT_EQ(std::size(kIv1), config->iv().size());
  EXPECT_TRUE(!memcmp(kIv1, config->iv().data(), config->iv().size()));
  iter_->AdvanceSample();
  EXPECT_EQ(iter_->GetMaxClearOffset(), 201);
}

// Sensible files are expected to place auxiliary information for a run
// immediately before the main data for that run. Alternative schemes are
// possible, however, including the somewhat reasonable behavior of placing all
// aux info at the head of the 'mdat' box together, and the completely
// unreasonable behavior demonstrated here:
//  byte 50: track 2, run 1 aux info
//  byte 100: track 1, run 1 data
//  byte 200: track 2, run 1 data
//  byte 201: track 1, run 2 aux info (*inside* track 2, run 1 data)
//  byte 10000: track 1, run 2 data
//  byte 20000: track 1, run 1 aux info
TEST_F(TrackRunIteratorTest, UnexpectedOrderingTest) {
  AddEncryption(FOURCC_cenc, &moov_.tracks[0]);
  AddEncryption(FOURCC_cenc, &moov_.tracks[1]);
  iter_.reset(new TrackRunIterator(&moov_));

  MovieFragment moof = CreateFragment();
  AddAuxInfoHeaders(20000, &moof.tracks[0]);
  moof.tracks[0].auxiliary_offset.offsets.push_back(201);
  moof.tracks[0].auxiliary_size.sample_count += 2;
  moof.tracks[0].auxiliary_size.default_sample_info_size = 8;
  moof.tracks[0].runs[1].sample_count = 2;
  AddAuxInfoHeaders(50, &moof.tracks[1]);
  moof.tracks[1].runs[0].sample_sizes[0] = 5;

  ASSERT_TRUE(iter_->Init(moof));
  EXPECT_EQ(iter_->track_id(), 2u);
  EXPECT_EQ(iter_->aux_info_offset(), 50);
  EXPECT_EQ(iter_->sample_offset(), 200);
  EXPECT_TRUE(iter_->CacheAuxInfo(kAuxInfo, std::size(kAuxInfo)));
  EXPECT_EQ(iter_->GetMaxClearOffset(), 100);
  iter_->AdvanceRun();
  EXPECT_EQ(iter_->track_id(), 1u);
  EXPECT_EQ(iter_->aux_info_offset(), 20000);
  EXPECT_EQ(iter_->sample_offset(), 100);
  EXPECT_TRUE(iter_->CacheAuxInfo(kAuxInfo, std::size(kAuxInfo)));
  EXPECT_EQ(iter_->GetMaxClearOffset(), 100);
  iter_->AdvanceSample();
  EXPECT_EQ(iter_->GetMaxClearOffset(), 101);
  iter_->AdvanceRun();
  EXPECT_EQ(iter_->track_id(), 1u);
  EXPECT_EQ(iter_->aux_info_offset(), 201);
  EXPECT_EQ(iter_->sample_offset(), 10000);
  EXPECT_EQ(iter_->GetMaxClearOffset(), 201);
  EXPECT_TRUE(iter_->CacheAuxInfo(kAuxInfo, std::size(kAuxInfo)));
  EXPECT_EQ(iter_->GetMaxClearOffset(), 10000);
}

}  // namespace mp4
}  // namespace media
}  // namespace shaka
