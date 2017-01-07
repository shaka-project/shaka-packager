// Copyright 2015 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/formats/webm/segmenter_test_base.h"

#include "packager/media/file/memory_file.h"
#include "packager/media/formats/webm/webm_constants.h"
#include "packager/version/version.h"

namespace shaka {
namespace media {
namespace {

// The contents of a frame does not mater.
const uint8_t kTestMediaSampleData[] = {0xde, 0xad, 0xbe, 0xef, 0x00};
const uint8_t kTestMediaSampleSideData[] = {
    // First 8 bytes of side_data is the BlockAddID element in big endian.
    0x12, 0x34, 0x56, 0x78, 0x9a, 0x00, 0x00, 0x00,
    0x73, 0x69, 0x64, 0x65, 0x00};

const int kTrackId = 1;
const uint32_t kTimeScale = 1000;
const uint64_t kDuration = 8000;
const Codec kCodec = kCodecVP8;
const std::string kCodecString = "vp8";
const std::string kLanguage = "en";
const uint16_t kWidth = 100;
const uint16_t kHeight = 100;
const uint16_t kPixelWidth = 100;
const uint16_t kPixelHeight = 100;
const int16_t kTrickPlayRate = 1;
const uint8_t kNaluLengthSize = 0;

}  // namespace

SegmentTestBase::SegmentTestBase() {}

void SegmentTestBase::SetUp() {
  SetPackagerVersionForTesting("test");

  output_file_name_ = std::string(kMemoryFilePrefix) + "output-file.webm";
  cur_time_timescale_ = 0;
}

void SegmentTestBase::TearDown() {
  MemoryFile::DeleteAll();
}

scoped_refptr<MediaSample> SegmentTestBase::CreateSample(
    KeyFrameFlag key_frame_flag,
    uint64_t duration,
    SideDataFlag side_data_flag) {
  scoped_refptr<MediaSample> sample;
  const bool is_key_frame = key_frame_flag == kKeyFrame;
  if (side_data_flag == kGenerateSideData) {
    sample = MediaSample::CopyFrom(
        kTestMediaSampleData, sizeof(kTestMediaSampleData),
        kTestMediaSampleSideData, sizeof(kTestMediaSampleSideData),
        is_key_frame);
  } else {
    sample = MediaSample::CopyFrom(kTestMediaSampleData,
                                   sizeof(kTestMediaSampleData), is_key_frame);
  }
  sample->set_dts(cur_time_timescale_);
  sample->set_pts(cur_time_timescale_);
  sample->set_duration(duration);

  cur_time_timescale_ += duration;
  return sample;
}

MuxerOptions SegmentTestBase::CreateMuxerOptions() const {
  MuxerOptions ret;
  ret.output_file_name = output_file_name_;
  ret.segment_duration = 30;  // seconds
  ret.fragment_duration = 30;  // seconds
  ret.segment_sap_aligned = false;
  ret.fragment_sap_aligned = false;
  // Use memory files for temp storage.  Normally this would be a bad idea
  // since it wouldn't support large files, but for tests the files are small.
  ret.temp_dir = std::string(kMemoryFilePrefix) + "temp/";
  return ret;
}

VideoStreamInfo* SegmentTestBase::CreateVideoStreamInfo() const {
  return new VideoStreamInfo(kTrackId, kTimeScale, kDuration, kCodec,
                             kCodecString, NULL, 0, kWidth, kHeight,
                             kPixelWidth, kPixelHeight, kTrickPlayRate,
                             kNaluLengthSize, kLanguage, false);
}

std::string SegmentTestBase::OutputFileName() const {
  return output_file_name_;
}

SegmentTestBase::ClusterParser::ClusterParser() : in_cluster_(false) {}

SegmentTestBase::ClusterParser::~ClusterParser() {}

void SegmentTestBase::ClusterParser::PopulateFromCluster(
    const std::string& file_name) {
  cluster_sizes_.clear();
  std::string file_contents;
  ASSERT_TRUE(File::ReadFileToString(file_name.c_str(), &file_contents));

  const uint8_t* data = reinterpret_cast<const uint8_t*>(file_contents.c_str());
  const size_t size = file_contents.size();
  WebMListParser cluster_parser(kWebMIdCluster, this);
  size_t position = 0;
  while (position < size) {
    int read = cluster_parser.Parse(data + position,
                                    static_cast<int>(size - position));
    ASSERT_LT(0, read);

    cluster_parser.Reset();
    position += read;
  }
}

void SegmentTestBase::ClusterParser::PopulateFromSegment(
        const std::string& file_name) {
  cluster_sizes_.clear();
  std::string file_contents;
  ASSERT_TRUE(File::ReadFileToString(file_name.c_str(), &file_contents));

  const uint8_t* data = reinterpret_cast<const uint8_t*>(file_contents.c_str());
  const size_t size = file_contents.size();
  WebMListParser header_parser(kWebMIdEBMLHeader, this);
  int offset = header_parser.Parse(data, static_cast<int>(size));
  ASSERT_LT(0, offset);

  WebMListParser segment_parser(kWebMIdSegment, this);
  ASSERT_LT(
      0, segment_parser.Parse(data + offset, static_cast<int>(size) - offset));
}

int SegmentTestBase::ClusterParser::GetFrameCountForCluster(size_t i) const {
  DCHECK(i < cluster_sizes_.size());
  return cluster_sizes_[i];
}

size_t SegmentTestBase::ClusterParser::cluster_count() const {
  return cluster_sizes_.size();
}

WebMParserClient* SegmentTestBase::ClusterParser::OnListStart(int id) {
  if (id == kWebMIdCluster) {
    if (in_cluster_)
      return NULL;

    cluster_sizes_.push_back(0);
    in_cluster_ = true;
  }

  return this;
}

bool SegmentTestBase::ClusterParser::OnListEnd(int id) {
  if (id == kWebMIdCluster) {
    if (!in_cluster_)
      return false;
    in_cluster_ = false;
  }

  return true;
}

bool SegmentTestBase::ClusterParser::OnUInt(int id, int64_t val) {
  return true;
}

bool SegmentTestBase::ClusterParser::OnFloat(int id, double val) {
  return true;
}

bool SegmentTestBase::ClusterParser::OnBinary(int id,
                                             const uint8_t* data,
                                             int size) {
  if (in_cluster_ && (id == kWebMIdSimpleBlock || id == kWebMIdBlock)) {
    cluster_sizes_.back()++;
  }

  return true;
}

bool SegmentTestBase::ClusterParser::OnString(int id, const std::string& str) {
  return true;
}

}  // namespace media
}  // namespace shaka
