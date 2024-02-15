// Copyright 2015 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/media/formats/webm/segmenter_test_base.h>

#include <absl/log/check.h>

#include <packager/file/memory_file.h>
#include <packager/media/formats/webm/webm_constants.h>
#include <packager/version/version.h>

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
const int64_t kDurationInSeconds = 8;
const Codec kCodec = kCodecVP8;
const std::string kCodecString = "vp8";
const std::string kLanguage = "en";
const uint16_t kWidth = 100;
const uint16_t kHeight = 100;
const uint16_t kPixelWidth = 100;
const uint16_t kPixelHeight = 100;
const uint8_t kTransferCharacteristics = 0;
const int16_t kTrickPlayFactor = 1;
const uint8_t kNaluLengthSize = 0;

}  // namespace

SegmentTestBase::SegmentTestBase() {}

void SegmentTestBase::SetUp() {
  SetPackagerVersionForTesting("test");

  output_file_name_ = std::string(kMemoryFilePrefix) + "output-file.webm";
  cur_timestamp_ = 0;
}

void SegmentTestBase::TearDown() {
  MemoryFile::DeleteAll();
}

std::shared_ptr<MediaSample> SegmentTestBase::CreateSample(
    KeyFrameFlag key_frame_flag,
    int64_t duration,
    SideDataFlag side_data_flag) {
  std::shared_ptr<MediaSample> sample;
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
  sample->set_dts(cur_timestamp_);
  sample->set_pts(cur_timestamp_);
  sample->set_duration(duration);

  cur_timestamp_ += duration;
  return sample;
}

MuxerOptions SegmentTestBase::CreateMuxerOptions() const {
  MuxerOptions ret;
  ret.output_file_name = output_file_name_;
  // Use memory files for temp storage.  Normally this would be a bad idea
  // since it wouldn't support large files, but for tests the files are small.
  ret.temp_dir = std::string(kMemoryFilePrefix) + "temp/";
  return ret;
}

VideoStreamInfo* SegmentTestBase::CreateVideoStreamInfo(
    int32_t time_scale) const {
  return new VideoStreamInfo(
      kTrackId, time_scale, kDurationInSeconds * time_scale, kCodec,
      H26xStreamFormat::kUnSpecified, kCodecString, NULL, 0, kWidth, kHeight,
      kPixelWidth, kPixelHeight, kTransferCharacteristics, kTrickPlayFactor,
      kNaluLengthSize, kLanguage, false);
}

std::string SegmentTestBase::OutputFileName() const {
  return output_file_name_;
}

SegmentTestBase::ClusterParser::ClusterParser() {}

SegmentTestBase::ClusterParser::~ClusterParser() {}

void SegmentTestBase::ClusterParser::PopulateFromCluster(
    const std::string& file_name) {
  frame_timecodes_.clear();
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
  frame_timecodes_.clear();
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

size_t SegmentTestBase::ClusterParser::GetFrameCountForCluster(
    size_t cluster_index) const {
  DCHECK_LT(cluster_index, frame_timecodes_.size());
  return frame_timecodes_[cluster_index].size();
}

int64_t SegmentTestBase::ClusterParser::GetFrameTimecode(
    size_t cluster_index,
    size_t frame_index) const {
  DCHECK_LT(cluster_index, frame_timecodes_.size());
  DCHECK_LT(frame_index, frame_timecodes_[cluster_index].size());
  return frame_timecodes_[cluster_index][frame_index];
}

size_t SegmentTestBase::ClusterParser::cluster_count() const {
  return frame_timecodes_.size();
}

WebMParserClient* SegmentTestBase::ClusterParser::OnListStart(int id) {
  if (id == kWebMIdCluster) {
    if (in_cluster_)
      return NULL;

    frame_timecodes_.emplace_back();
    cluster_timecode_ = -1;
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
  if (id == kWebMIdTimecode)
    cluster_timecode_ = val;
  return true;
}

bool SegmentTestBase::ClusterParser::OnFloat(int /*id*/, double /*val*/) {
  return true;
}

bool SegmentTestBase::ClusterParser::OnBinary(int id,
                                              const uint8_t* data,
                                              int /*size*/) {
  if (in_cluster_ && (id == kWebMIdSimpleBlock || id == kWebMIdBlock)) {
    if (cluster_timecode_ == -1) {
      LOG(WARNING) << "Cluster timecode not yet available";
      return false;
    }
    int timecode = data[1] << 8 | data[2];
    frame_timecodes_.back().push_back(cluster_timecode_ + timecode);
  }

  return true;
}

bool SegmentTestBase::ClusterParser::OnString(int /*id*/,
                                              const std::string& /*str*/) {
  return true;
}

}  // namespace media
}  // namespace shaka
