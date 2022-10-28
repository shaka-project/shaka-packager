// Copyright 2016 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <gtest/gtest.h>

#include <vector>

#include "packager/base/bind.h"
#include "packager/base/logging.h"
#include "packager/media/base/media_sample.h"
#include "packager/media/base/stream_info.h"
#include "packager/media/base/timestamp.h"
#include "packager/media/codecs/h26x_byte_to_unit_stream_converter.h"
#include "packager/media/formats/mp2t/es_parser_h26x.h"

namespace shaka {
namespace media {
namespace mp2t {

namespace {

const int kH264RefIdc = 1 << 5;

// NAL unit types used for testing.
enum H26xNaluType {
  kH264Aud = Nalu::H264_AUD,
  kH264Sps = Nalu::H264_SPS | kH264RefIdc,
  kH264Sei = Nalu::H264_SEIMessage,
  // Something with |can_start_access_unit() == false|.
  kH264Rsv = Nalu::H264_FillerData,
  // Non-key-frame video slice.
  kH264Vcl = Nalu::H264_NonIDRSlice,
  // For testing purpose, the first 2 bits contains the frame num.
  kH264VclFrame0 = Nalu::H264_NonIDRSlice | (0 << 6),
  kH264VclFrame1 = Nalu::H264_NonIDRSlice | (1 << 6),
  kH264VclFrame2 = Nalu::H264_NonIDRSlice | (2 << 6),
  kH264VclFrame3 = Nalu::H264_NonIDRSlice | (3 << 6),
  kH264VclKeyFrame = Nalu::H264_IDRSlice | kH264RefIdc,

  kH265Aud = Nalu::H265_AUD,
  kH265Sps = Nalu::H265_SPS,
  kH265Sei = Nalu::H265_PREFIX_SEI,
  // Something with |can_start_access_unit() == false|.
  kH265Rsv = Nalu::H265_FD,
  // Non-key-frame video slice.
  kH265Vcl = Nalu::H265_TRAIL_N,
  kH265VclKeyFrame = Nalu::H265_IDR_W_RADL,
  // Needs to be different than |kH265VCL| so we can tell the difference.
  kH265VclWithNuhLayer = Nalu::H265_TRAIL_R,

  // Used to separate expected access units.
  kSeparator = 0xff,
};

class FakeByteToUnitStreamConverter : public H26xByteToUnitStreamConverter {
 public:
  explicit FakeByteToUnitStreamConverter(Nalu::CodecType codec_type)
      : H26xByteToUnitStreamConverter(codec_type) {}

  bool GetDecoderConfigurationRecord(
      std::vector<uint8_t>* decoder_config) const override {
    return true;
  }

  bool ProcessNalu(const Nalu& nalu) override {
    // This processed nothing, base class should copy everything.
    return false;
  }
};

// This is the code-under-test.  This implements the required abstract methods
// to ignore the contents of the NAL units.
class TestableEsParser : public EsParserH26x {
 public:
  TestableEsParser(Nalu::CodecType codec_type,
                   const NewStreamInfoCB& new_stream_info_cb,
                   const EmitSampleCB& emit_sample_cb)
      : EsParserH26x(codec_type,
                     std::unique_ptr<H26xByteToUnitStreamConverter>(
                         new FakeByteToUnitStreamConverter(codec_type)),
                     0,
                     emit_sample_cb),
        codec_type_(codec_type),
        new_stream_info_cb_(new_stream_info_cb),
        decoder_config_check_pending_(false) {}

  bool ProcessNalu(const Nalu& nalu,
                   VideoSliceInfo* video_slice_info) override {
    if (codec_type_ == Nalu::kH264 ? (nalu.type() == Nalu::H264_SPS)
                                   : (nalu.type() == Nalu::H265_SPS)) {
      video_slice_info->valid = false;
      decoder_config_check_pending_ = true;
    } else if (nalu.is_vcl()) {
      video_slice_info->valid = true;
      // This should be the same as EsParserH26x::ProcessNalu.
      if (codec_type_ == Nalu::kH264) {
        video_slice_info->is_key_frame = nalu.type() == Nalu::H264_IDRSlice;
      } else {
        video_slice_info->is_key_frame = nalu.type() == Nalu::H265_IDR_W_RADL ||
                                         nalu.type() == Nalu::H265_IDR_N_LP;
      }
      video_slice_info->pps_id = kTestPpsId;
      // for testing purpose, the frame_num is coded in the first byte of
      // payload.
      video_slice_info->frame_num = nalu.data()[nalu.header_size()];
    }
    return true;
  }

  bool UpdateVideoDecoderConfig(int pps_id) override {
    if (decoder_config_check_pending_) {
      EXPECT_EQ(kTestPpsId, pps_id);
      new_stream_info_cb_.Run(nullptr);
      decoder_config_check_pending_ = false;
    }
    return true;
  }

 private:
  int64_t CalculateSampleDuration(int pps_id) override {
    // Typical 40ms - frame duration with 25 FPS
    return 0.04 * 90000;
  }

  const int kTestPpsId = 123;

  Nalu::CodecType codec_type_;
  NewStreamInfoCB new_stream_info_cb_;
  bool decoder_config_check_pending_;
};

std::vector<uint8_t> CreateNalu(Nalu::CodecType codec_type,
                                H26xNaluType type,
                                uint8_t i) {
  std::vector<uint8_t> ret;
  if (codec_type == Nalu::kH264) {
    ret.resize(3);
    // For testing purpose, the first 2 bits contains the frame num and encoded
    // in the first byte of the payload.
    ret[0] = (type & 0x3f);
    ret[1] = (type >> 6);
    ret[2] = i + 1;
  } else {
    ret.resize(4);
    ret[0] = (type << 1);
    // nuh_layer_id == 1, nuh_temporal_id_plus1 == 1
    ret[1] = (type == kH265VclWithNuhLayer ? 9 : 1);
    // Add some extra data to tell consecutive frames apart.
    ret[2] = 0xff;
    ret[3] = i + 1;
  }
  return ret;
}

}  // namespace

class EsParserH26xTest : public testing::Test {
 public:
  EsParserH26xTest() : sample_count_(0), has_stream_info_(false) {}

  // Runs a test by constructing NAL units of the given types and passing them
  // to the parser.  Access units should be separated by |kSeparator|, there
  // should be one at the start and not at the end.
  void RunTest(Nalu::CodecType codec_type,
               const H26xNaluType* types,
               size_t types_count);

  // Returns the vector of samples data j
  std::vector<std::vector<uint8_t>> BuildSamplesData(Nalu::CodecType codec_type,
                                                     const H26xNaluType* types,
                                                     size_t types_count);

  void EmitSample(std::shared_ptr<MediaSample> sample) {
    size_t sample_id = sample_count_;
    sample_count_++;
    if (sample_count_ == 1) {
      EXPECT_TRUE(sample->is_key_frame());
    }

    ASSERT_GT(samples_.size(), sample_id);
    const std::vector<uint8_t> sample_data(
        sample->data(), sample->data() + sample->data_size());
    EXPECT_EQ(samples_[sample_id], sample_data);
    media_samples_.push_back(sample);
  }

  void NewVideoConfig(std::shared_ptr<StreamInfo> config) {
    has_stream_info_ = true;
  }

 protected:
  std::vector<std::vector<uint8_t>> samples_;
  std::vector<std::shared_ptr<MediaSample>> media_samples_;
  size_t sample_count_;
  bool has_stream_info_;
};

// Return AnnexB samples data and stores NAL Unit samples data in |samples_|,
// which is what will be returned from |EsParser|.
std::vector<std::vector<uint8_t>> EsParserH26xTest::BuildSamplesData(
    Nalu::CodecType codec_type,
    const H26xNaluType* types,
    size_t types_count) {
  std::vector<std::vector<uint8_t>> samples_data;

  const uint8_t kStartCode[] = {0x00, 0x00, 0x01};

  bool seen_key_frame = false;
  std::vector<uint8_t> nal_unit_sample_data;
  std::vector<uint8_t> annex_b_sample_data;
  CHECK_EQ(kSeparator, types[0]);
  for (size_t k = 1; k < types_count; k++) {
    if (types[k] == kSeparator) {
      // We should not be emitting samples until we see a key frame.
      if (seen_key_frame)
        samples_.push_back(nal_unit_sample_data);
      if (!annex_b_sample_data.empty())
        samples_data.push_back(annex_b_sample_data);
      nal_unit_sample_data.clear();
      annex_b_sample_data.clear();
    } else {
      if (codec_type == Nalu::kH264) {
        if (types[k] == kH264VclKeyFrame)
          seen_key_frame = true;
      } else {
        if (types[k] == kH265VclKeyFrame)
          seen_key_frame = true;
      }

      std::vector<uint8_t> es_data =
          CreateNalu(codec_type, types[k], static_cast<uint8_t>(k));

      nal_unit_sample_data.push_back(0);
      nal_unit_sample_data.push_back(0);
      nal_unit_sample_data.push_back(0);
      nal_unit_sample_data.push_back(static_cast<uint8_t>(es_data.size()));
      nal_unit_sample_data.insert(nal_unit_sample_data.end(), es_data.begin(),
                                  es_data.end());

      es_data.insert(es_data.begin(), kStartCode,
                     kStartCode + arraysize(kStartCode));
      annex_b_sample_data.insert(annex_b_sample_data.end(), es_data.begin(),
                                 es_data.end());
    }
  }
  if (seen_key_frame)
    samples_.push_back(nal_unit_sample_data);
  if (!annex_b_sample_data.empty())
    samples_data.push_back(annex_b_sample_data);

  return samples_data;
}

void EsParserH26xTest::RunTest(Nalu::CodecType codec_type,
                               const H26xNaluType* types,
                               size_t types_count) {
  // Duration of one 25fps video frame in 90KHz clock units.
  const uint32_t kMpegTicksPerFrame = 3600;

  TestableEsParser es_parser(
      codec_type,
      base::Bind(&EsParserH26xTest::NewVideoConfig, base::Unretained(this)),
      base::Bind(&EsParserH26xTest::EmitSample, base::Unretained(this)));

  int64_t timestamp = 0;
  for (const auto& sample_data :
       BuildSamplesData(codec_type, types, types_count)) {
    // This may process the previous sample; but since we don't know whether
    // we are at the end yet, this will not process the current sample until
    // later.
    size_t offset = 0;
    size_t size = 1;
    while (offset < sample_data.size()) {
      // Insert the data in parts to test partial data searches.
      size = std::min(size + 1, sample_data.size() - offset);
      ASSERT_TRUE(es_parser.Parse(&sample_data[offset], static_cast<int>(size),
                                  timestamp, timestamp));
      offset += size;
    }
    timestamp += kMpegTicksPerFrame;
  }
  es_parser.Flush();
}

TEST_F(EsParserH26xTest, H265BasicSupport) {
  const H26xNaluType kData[] = {
    kSeparator, kH265Aud, kH265Sps, kH265VclKeyFrame,
    kSeparator, kH265Aud, kH265Vcl,
    kSeparator, kH265Aud, kH265Vcl,
  };

  RunTest(Nalu::kH265, kData, arraysize(kData));
  EXPECT_EQ(3u, sample_count_);
  EXPECT_TRUE(has_stream_info_);
}

TEST_F(EsParserH26xTest, H265DeterminesAccessUnitsWithoutAUD) {
  const H26xNaluType kData[] = {
    kSeparator, kH265Sps, kH265VclKeyFrame,
    kSeparator, kH265Vcl,
    kSeparator, kH265Vcl,
    kSeparator, kH265Sei, kH265Vcl,
  };

  RunTest(Nalu::kH265, kData, arraysize(kData));
  EXPECT_EQ(4u, sample_count_);
  EXPECT_TRUE(has_stream_info_);
}

TEST_F(EsParserH26xTest, H265DoesNotStartOnRsv) {
  const H26xNaluType kData[] = {
    kSeparator, kH265Sps, kH265VclKeyFrame, kH265Rsv,
    kSeparator, kH265Aud, kH265Vcl,
    kSeparator, kH265Sei, kH265Vcl,
  };

  RunTest(Nalu::kH265, kData, arraysize(kData));
  EXPECT_EQ(3u, sample_count_);
  EXPECT_TRUE(has_stream_info_);
}

TEST_F(EsParserH26xTest, H265SupportsNonZeroNuhLayerId) {
  const H26xNaluType kData[] = {
    kSeparator, kH265Sps, kH265VclKeyFrame,
    kSeparator, kH265Aud, kH265Vcl, kH265Sei, kH265VclWithNuhLayer, kH265Rsv,
    kSeparator, kH265Sei, kH265Vcl,
    kSeparator, kH265Aud, kH265Vcl, kH265Sps, kH265Rsv, kH265VclWithNuhLayer,
    kSeparator, kH265Vcl,
  };

  RunTest(Nalu::kH265, kData, arraysize(kData));
  EXPECT_EQ(5u, sample_count_);
  EXPECT_TRUE(has_stream_info_);
}

TEST_F(EsParserH26xTest, H265WaitsForKeyFrame) {
  const H26xNaluType kData[] = {
    kSeparator, kH265Vcl,
    kSeparator, kH265Vcl,
    kSeparator, kH265Sps, kH265VclKeyFrame,
    kSeparator, kH265Vcl,
    kSeparator, kH265Vcl,
  };

  RunTest(Nalu::kH265, kData, arraysize(kData));
  EXPECT_EQ(3u, sample_count_);
  EXPECT_TRUE(has_stream_info_);
}

TEST_F(EsParserH26xTest, H265EmitsFramesWithNoStreamInfo) {
  const H26xNaluType kData[] = {
    kSeparator, kH265VclKeyFrame,
    kSeparator, kH265Vcl, kH265Rsv,
    kSeparator, kH265Sei, kH265Vcl,
  };

  RunTest(Nalu::kH265, kData, arraysize(kData));
  EXPECT_EQ(3u, sample_count_);
  EXPECT_FALSE(has_stream_info_);
}

TEST_F(EsParserH26xTest, H265EmitsLastFrameWithNuhLayerId) {
  const H26xNaluType kData[] = {
    kSeparator, kH265VclKeyFrame,
    kSeparator, kH265Vcl,
    kSeparator, kH265Vcl, kH265Sei, kH265VclWithNuhLayer, kH265Rsv,
  };

  RunTest(Nalu::kH265, kData, arraysize(kData));
  EXPECT_EQ(3u, sample_count_);
  EXPECT_FALSE(has_stream_info_);
}

TEST_F(EsParserH26xTest, H264BasicSupport) {
  const H26xNaluType kData[] = {
    kSeparator, kH264Aud, kH264Sps, kH264VclKeyFrame,
    kSeparator, kH264Aud, kH264Vcl,
    kSeparator, kH264Aud, kH264Vcl,
  };

  RunTest(Nalu::kH264, kData, arraysize(kData));
  EXPECT_EQ(3u, sample_count_);
  EXPECT_TRUE(has_stream_info_);
  EXPECT_EQ(3u, media_samples_.size());
  for (size_t i = 0; i < media_samples_.size(); i++) {
    EXPECT_GT(media_samples_[i]->duration(), 0u);
  }
}

// This is not compliant to H264 spec, but VLC generates streams like this. See
// https://github.com/shaka-project/shaka-packager/issues/526 for details.
TEST_F(EsParserH26xTest, H264AudInAccessUnit) {
  // clang-format off
  const H26xNaluType kData[] = {
    kSeparator, kH264Aud, kH264Sps, kH264Aud, kH264VclKeyFrame,
    kSeparator, kH264Aud, kH264Vcl,
    kSeparator, kH264Aud, kH264Vcl,
    kSeparator, kH264Aud, kH264Sps, kH264Aud, kH264VclKeyFrame,
    kSeparator, kH264Aud, kH264Vcl,
    kSeparator, kH264Aud, kH264Vcl,
    kSeparator, kH264Aud, kH264Sps, kH264Aud, kH264VclKeyFrame,
    kSeparator, kH264Aud, kH264Vcl,
    kSeparator, kH264Aud, kH264Vcl,
    kSeparator, kH264Aud, kH264Sps, kH264Aud, kH264VclKeyFrame,
    kSeparator, kH264Aud, kH264Vcl,
    kSeparator, kH264Aud, kH264Vcl,
  };
  // clang-format on

  TestableEsParser es_parser(
      Nalu::kH264,
      base::Bind(&EsParserH26xTest::NewVideoConfig, base::Unretained(this)),
      base::Bind(&EsParserH26xTest::EmitSample, base::Unretained(this)));

  size_t sample_index = 0;
  for (const auto& sample_data :
       BuildSamplesData(Nalu::kH264, kData, arraysize(kData))) {
    // Duration of one 25fps video frame in 90KHz clock units.
    const uint32_t kMpegTicksPerFrame = 3600;
    const int64_t timestamp = kMpegTicksPerFrame * sample_index;
    ASSERT_TRUE(es_parser.Parse(sample_data.data(),
                                static_cast<int>(sample_data.size()), timestamp,
                                timestamp));
    sample_index++;

    // The number of emitted samples are less than the number of samples that
    // are pushed to the EsParser since samples could be cached internally
    // before being emitted.
    // The delay is at most 2 in our current implementation.
    const size_t kExpectedMaxDelay = 2;
    EXPECT_NEAR(sample_index, sample_count_, kExpectedMaxDelay);
  }

  es_parser.Flush();
  EXPECT_EQ(sample_index, sample_count_);
  EXPECT_TRUE(has_stream_info_);
}

TEST_F(EsParserH26xTest, H264DeterminesAccessUnitsWithoutAUD) {
  const H26xNaluType kData[] = {
    kSeparator, kH264Sps, kH264VclKeyFrame,
    kSeparator, kH264VclFrame1, kH264VclFrame1,
    kSeparator, kH264VclFrame2, kH264VclFrame2, kH264VclFrame2,
    kSeparator, kH264Sei, kH264VclFrame3,
  };

  RunTest(Nalu::kH264, kData, arraysize(kData));
  EXPECT_EQ(4u, sample_count_);
  EXPECT_TRUE(has_stream_info_);
}

TEST_F(EsParserH26xTest, H264DoesNotStartOnRsv) {
  const H26xNaluType kData[] = {
    kSeparator, kH264Sps, kH264VclKeyFrame, kH264Rsv,
    kSeparator, kH264Aud, kH264VclFrame1,
    kSeparator, kH264Sei, kH264VclFrame2,
  };

  RunTest(Nalu::kH264, kData, arraysize(kData));
  EXPECT_EQ(3u, sample_count_);
  EXPECT_TRUE(has_stream_info_);
}

TEST_F(EsParserH26xTest, H264ContainsOnlyOneFrame) {
  const H26xNaluType kData[] = {
      kSeparator,
      kH264Aud,
      kH264Sps,
      kH264VclKeyFrame,
  };

  RunTest(Nalu::kH264, kData, arraysize(kData));
  EXPECT_TRUE(has_stream_info_);
  EXPECT_EQ(1u, sample_count_);
  EXPECT_EQ(1u, media_samples_.size());
  EXPECT_GT(media_samples_[0]->duration(), 0u);
}

TEST_F(EsParserH26xTest, H265ContainsOnlyOneFrame) {
  const H26xNaluType kData[] = {
      kSeparator, kH265Aud, kH265Sps, kH265VclKeyFrame,
  };

  RunTest(Nalu::kH265, kData, arraysize(kData));
  EXPECT_TRUE(has_stream_info_);
  EXPECT_EQ(1u, sample_count_);
  EXPECT_EQ(1u, media_samples_.size());
  EXPECT_GT(media_samples_[0]->duration(), 0u);
}

}  // namespace mp2t
}  // namespace media
}  // namespace shaka
