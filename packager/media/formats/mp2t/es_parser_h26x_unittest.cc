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

// NAL unit types used for testing.
enum H265NaluType {
  kAud = Nalu::H265_AUD,
  kSps = Nalu::H265_SPS,
  kSei = Nalu::H265_PREFIX_SEI,
  // Something with |can_start_access_unit() == false|.
  kRsv = Nalu::H265_FD,
  // Non-key-frame video slice.
  kVcl = Nalu::H265_TRAIL_N,
  kVclKeyFrame = Nalu::H265_IDR_W_RADL,
  // Needs to be different than |kVCL| so we can tell the difference.
  kVclWithNuhLayer = Nalu::H265_TRAIL_R,
  // Used to separate expected access units.
  kSeparator = 0xff,
};

class FakeByteToUnitStreamConverter : public H26xByteToUnitStreamConverter {
 public:
  FakeByteToUnitStreamConverter()
      : H26xByteToUnitStreamConverter(Nalu::kH265) {}

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
// to ignore the contents of the NAL units.  This behaves the same as the
// H.264 and H.265 types.
class TestableEsParser : public EsParserH26x {
 public:
  TestableEsParser(const NewStreamInfoCB& new_stream_info_cb,
               const EmitSampleCB& emit_sample_cb)
      : EsParserH26x(Nalu::kH265,
                     scoped_ptr<H26xByteToUnitStreamConverter>(
                         new FakeByteToUnitStreamConverter()),
                     0,
                     emit_sample_cb),
        new_stream_info_cb_(new_stream_info_cb),
        decoder_config_check_pending_(false) {}

  bool ProcessNalu(const Nalu& nalu,
                   bool* is_key_frame,
                   int* pps_id_for_access_unit) override {
    if (nalu.type() == Nalu::H265_SPS) {
      decoder_config_check_pending_ = true;
    } else if (nalu.is_video_slice()) {
      // This should be the same as EsParserH265::ProcessNalu.
      *is_key_frame = nalu.type() == Nalu::H265_IDR_W_RADL ||
                      nalu.type() == Nalu::H265_IDR_N_LP;
      *pps_id_for_access_unit = kTestPpsId;
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
  const int kTestPpsId = 123;

  NewStreamInfoCB new_stream_info_cb_;
  bool decoder_config_check_pending_;
};

std::vector<uint8_t> CreateNalu(H265NaluType type, int i) {
  std::vector<uint8_t> ret;
  ret.resize(4);
  ret[0] = (type << 1);
  // nuh_layer_id == 1, nuh_temporal_id_plus1 == 1
  ret[1] = (type == kVclWithNuhLayer ? 9 : 1);
  // Add some extra data to tell consecutive frames apart.
  ret[2] = 0xff;
  ret[3] = i + 1;
  return ret;
}

}  // namespace

class EsParserH26xTest : public testing::Test {
 public:
  EsParserH26xTest() : sample_count_(0), has_stream_info_(false) {}

  // Runs a test by constructing NAL units of the given types and passing them
  // to the parser.  Access units should be separated by |kSeparator|, there
  // should be one at the start and not at the end.
  void RunTest(const H265NaluType* types, size_t types_count);

  void EmitSample(uint32_t pid, const scoped_refptr<MediaSample>& sample) {
    size_t sample_id = sample_count_;
    sample_count_++;
    if (sample_count_ == 1)
      EXPECT_TRUE(sample->is_key_frame());

    ASSERT_GT(samples_.size(), sample_id);
    const std::vector<uint8_t> sample_data(
        sample->data(), sample->data() + sample->data_size());
    EXPECT_EQ(samples_[sample_id], sample_data);
  }

  void NewVideoConfig(const scoped_refptr<StreamInfo>& config) {
    has_stream_info_ = true;
  }

 protected:
  std::vector<std::vector<uint8_t>> samples_;
  size_t sample_count_;
  bool has_stream_info_;
};

void EsParserH26xTest::RunTest(const H265NaluType* types,
                               size_t types_count) {
  // Duration of one 25fps video frame in 90KHz clock units.
  const uint32_t kMpegTicksPerFrame = 3600;
  const uint8_t kStartCode[] = {0x00, 0x00, 0x01};

  TestableEsParser es_parser(
      base::Bind(&EsParserH26xTest::NewVideoConfig, base::Unretained(this)),
      base::Bind(&EsParserH26xTest::EmitSample, base::Unretained(this)));

  bool seen_key_frame = false;
  std::vector<uint8_t> cur_sample_data;
  ASSERT_EQ(kSeparator, types[0]);
  for (size_t k = 1; k < types_count; k++) {
    if (types[k] == kSeparator) {
      // We should not be emitting samples until we see a key frame.
      if (seen_key_frame)
        samples_.push_back(cur_sample_data);
      cur_sample_data.clear();
    } else {
      if (types[k] == kVclKeyFrame)
        seen_key_frame = true;

      std::vector<uint8_t> es_data = CreateNalu(types[k], k);
      cur_sample_data.push_back(0);
      cur_sample_data.push_back(0);
      cur_sample_data.push_back(0);
      cur_sample_data.push_back(es_data.size());
      cur_sample_data.insert(cur_sample_data.end(), es_data.begin(),
                             es_data.end());
      es_data.insert(es_data.begin(), kStartCode,
                     kStartCode + arraysize(kStartCode));

      const int64_t pts = k * kMpegTicksPerFrame;
      const int64_t dts = k * kMpegTicksPerFrame;
      // This may process the previous sample; but since we don't know whether
      // we are at the end yet, this will not process the current sample until
      // later.
      size_t offset = 0;
      size_t size = 1;
      while (offset < es_data.size()) {
        // Insert the data in parts to test partial data searches.
        size = std::min(size + 1, es_data.size() - offset);
        ASSERT_TRUE(es_parser.Parse(&es_data[offset], size, pts, dts));
        offset += size;
      }
    }
  }
  if (seen_key_frame)
    samples_.push_back(cur_sample_data);

  es_parser.Flush();
}

TEST_F(EsParserH26xTest, BasicSupport) {
  const H265NaluType kData[] = {
    kSeparator, kAud, kSps, kVclKeyFrame,
    kSeparator, kAud, kVcl,
    kSeparator, kAud, kVcl,
  };

  RunTest(kData, arraysize(kData));
  EXPECT_EQ(3u, sample_count_);
  EXPECT_TRUE(has_stream_info_);
}

TEST_F(EsParserH26xTest, DeterminesAccessUnitsWithoutAUD) {
  const H265NaluType kData[] = {
    kSeparator, kSps, kVclKeyFrame,
    kSeparator, kVcl,
    kSeparator, kVcl,
    kSeparator, kSei, kVcl,
  };

  RunTest(kData, arraysize(kData));
  EXPECT_EQ(4u, sample_count_);
  EXPECT_TRUE(has_stream_info_);
}

TEST_F(EsParserH26xTest, DoesNotStartOnRsv) {
  const H265NaluType kData[] = {
    kSeparator, kSps, kVclKeyFrame, kRsv,
    kSeparator, kAud, kVcl,
    kSeparator, kSei, kVcl,
  };

  RunTest(kData, arraysize(kData));
  EXPECT_EQ(3u, sample_count_);
  EXPECT_TRUE(has_stream_info_);
}

TEST_F(EsParserH26xTest, SupportsNonZeroNuhLayerId) {
  const H265NaluType kData[] = {
    kSeparator, kSps, kVclKeyFrame,
    kSeparator, kAud, kVcl, kSei, kSei, kVclWithNuhLayer, kRsv,
    kSeparator, kSei, kVcl,
    kSeparator, kAud, kVcl, kSps, kRsv, kVclWithNuhLayer,
    kSeparator, kVcl,
  };

  RunTest(kData, arraysize(kData));
  EXPECT_EQ(5u, sample_count_);
  EXPECT_TRUE(has_stream_info_);
}

TEST_F(EsParserH26xTest, WaitsForKeyFrame) {
  const H265NaluType kData[] = {
    kSeparator, kVcl,
    kSeparator, kVcl,
    kSeparator, kSps, kVclKeyFrame,
    kSeparator, kVcl,
    kSeparator, kVcl,
  };

  RunTest(kData, arraysize(kData));
  EXPECT_EQ(3u, sample_count_);
  EXPECT_TRUE(has_stream_info_);
}

TEST_F(EsParserH26xTest, EmitsFramesWithNoStreamInfo) {
  const H265NaluType kData[] = {
    kSeparator, kVclKeyFrame,
    kSeparator, kVcl,
    kSeparator, kVcl,
  };

  RunTest(kData, arraysize(kData));
  EXPECT_EQ(3u, sample_count_);
  EXPECT_FALSE(has_stream_info_);
}

TEST_F(EsParserH26xTest, EmitsLastFrameWhenDoesntEndOnVCL) {
  // This tests that it will emit the last frame and last frame will include
  // the correct data and nothing extra.
  const H265NaluType kData[] = {
    kSeparator, kVclKeyFrame,
    kSeparator, kVcl,
    kSeparator, kVcl, kSei,
  };

  RunTest(kData, arraysize(kData));
  EXPECT_EQ(3u, sample_count_);
  EXPECT_FALSE(has_stream_info_);
}

TEST_F(EsParserH26xTest, EmitsLastFrameWithNuhLayerId) {
  const H265NaluType kData[] = {
    kSeparator, kVclKeyFrame,
    kSeparator, kVcl,
    kSeparator, kVcl, kVclWithNuhLayer, kSei,
  };

  RunTest(kData, arraysize(kData));
  EXPECT_EQ(3u, sample_count_);
  EXPECT_FALSE(has_stream_info_);
}

}  // namespace mp2t
}  // namespace media
}  // namespace shaka
