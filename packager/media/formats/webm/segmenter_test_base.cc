// Copyright 2015 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/formats/webm/segmenter_test_base.h"

#include "packager/media/base/muxer_util.h"
#include "packager/media/file/memory_file.h"
#include "packager/media/formats/webm/webm_constants.h"

namespace edash_packager {
namespace media {
namespace {

// The contents of a frame does not mater.
const uint8_t kTestMediaSampleData[] = {0xDE, 0xAD, 0xBE, 0xEF, 0x00};
const size_t kTestMediaSampleDataSize = sizeof(kTestMediaSampleData);

const int kTrackId = 1;
const uint32_t kTimeScale = 1000;
const uint64_t kDuration = 8000;
const VideoCodec kVideoCodec = kCodecVP8;
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
  output_file_name_ = "memory://output-file.webm";
  segment_template_ = "memory://output-template-$Number$.webm";
  cur_time_timescale_ = 0;
  single_segment_ = true;
}

void SegmentTestBase::TearDown() {
  MemoryFile::DeleteAll();
}

scoped_refptr<MediaSample> SegmentTestBase::CreateSample(bool is_key_frame,
                                                         uint64_t duration) {
  scoped_refptr<MediaSample> sample = MediaSample::CopyFrom(
      kTestMediaSampleData, kTestMediaSampleDataSize, is_key_frame);
  sample->set_dts(cur_time_timescale_);
  sample->set_pts(cur_time_timescale_);
  sample->set_duration(duration);

  cur_time_timescale_ += duration;
  return sample;
}

MuxerOptions SegmentTestBase::CreateMuxerOptions() const {
  MuxerOptions ret;
  ret.single_segment = single_segment_;
  ret.output_file_name = output_file_name_;
  ret.segment_template = segment_template_;
  ret.segment_duration = 30;  // seconds
  ret.fragment_duration = 30;  // seconds
  ret.segment_sap_aligned = false;
  ret.fragment_sap_aligned = false;
  // Use memory files for temp storage.  Normally this would be a bad idea
  // since it wouldn't support large files, but for tests the files are small.
  ret.temp_dir = "memory://temp/";
  return ret;
}

VideoStreamInfo* SegmentTestBase::CreateVideoStreamInfo() const {
  return new VideoStreamInfo(kTrackId, kTimeScale, kDuration, kVideoCodec,
                             kCodecString, kLanguage, kWidth, kHeight,
                             kPixelWidth, kPixelHeight, kTrickPlayRate,
                             kNaluLengthSize, NULL, 0, false);
}

std::string SegmentTestBase::OutputFileName() const {
  return output_file_name_;
}

std::string SegmentTestBase::TemplateFileName(int number) const {
  return GetSegmentName(segment_template_, 0, number, 0);
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
    int read = cluster_parser.Parse(data + position, size - position);
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
  int offset = header_parser.Parse(data, size);
  ASSERT_LT(0, offset);

  WebMListParser segment_parser(kWebMIdSegment, this);
  ASSERT_LT(0, segment_parser.Parse(data + offset, size - offset));
}

int SegmentTestBase::ClusterParser::GetFrameCountForCluster(size_t i) const {
  DCHECK(i < cluster_sizes_.size());
  return cluster_sizes_[i];
}

int SegmentTestBase::ClusterParser::cluster_count() const {
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
  if (in_cluster_ && id == kWebMIdSimpleBlock) {
    cluster_sizes_[cluster_sizes_.size() - 1]++;
  }

  return true;
}

bool SegmentTestBase::ClusterParser::OnString(int id, const std::string& str) {
  return true;
}

}  // namespace media
}  // namespace edash_packager

