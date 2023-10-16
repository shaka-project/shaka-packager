// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <packager/media/formats/mp2t/es_parser_h264.h>

#include <algorithm>
#include <functional>
#include <vector>

#include <absl/log/check.h>
#include <absl/log/log.h>
#include <gtest/gtest.h>

#include <packager/macros/logging.h>
#include <packager/media/base/media_sample.h>
#include <packager/media/base/timestamp.h>
#include <packager/media/base/video_stream_info.h>
#include <packager/media/codecs/h264_parser.h>
#include <packager/media/test/test_data_util.h>

namespace shaka {
namespace media {
class VideoStreamInfo;

namespace mp2t {

namespace {

struct Packet {
  // Offset in the stream.
  size_t offset;

  // Size of the packet.
  size_t size;
};

// Compute the size of each packet assuming packets are given in stream order
// and the last packet covers the end of the stream.
void ComputePacketSize(std::vector<Packet>& packets, size_t stream_size) {
  for (size_t k = 0; k < packets.size() - 1; k++) {
    DCHECK_GE(packets[k + 1].offset, packets[k].offset);
    packets[k].size = packets[k + 1].offset - packets[k].offset;
  }
  packets[packets.size() - 1].size =
      stream_size - packets[packets.size() - 1].offset;
}

// Get the offset of the start of each access unit.
// This function assumes there is only one slice per access unit.
// This is a very simplified access unit segmenter that is good
// enough for unit tests.
std::vector<Packet> GetAccessUnits(const uint8_t* stream, size_t stream_size) {
  std::vector<Packet> access_units;
  bool start_access_unit = true;

  // In a first pass, retrieve the offsets of all access units.
  size_t offset = 0;
  while (true) {
    // Find the next start code.
    uint64_t relative_offset = 0;
    uint8_t start_code_size = 0;
    bool success = NaluReader::FindStartCode(
        &stream[offset], stream_size - offset,
        &relative_offset, &start_code_size);
    if (!success)
      break;
    offset += relative_offset;

    if (start_access_unit) {
      Packet cur_access_unit;
      cur_access_unit.offset = offset;
      access_units.push_back(cur_access_unit);
      start_access_unit = false;
    }

    // Get the NALU type.
    offset += start_code_size;
    if (offset >= stream_size)
      break;
    int nal_unit_type = stream[offset] & 0x1f;

    // We assume there is only one slice per access unit.
    if (nal_unit_type == Nalu::H264_IDRSlice ||
        nal_unit_type == Nalu::H264_NonIDRSlice) {
      start_access_unit = true;
    }
  }

  ComputePacketSize(access_units, stream_size);
  return access_units;
}

// Append an AUD NALU at the beginning of each access unit
// needed for streams which do not already have AUD NALUs.
void AppendAUD(const uint8_t* stream,
               size_t stream_size,
               const std::vector<Packet>& access_units,
               std::vector<uint8_t>& stream_with_aud,
               std::vector<Packet>& access_units_with_aud) {
  uint8_t aud[] = {0x00, 0x00, 0x01, 0x09};
  stream_with_aud.resize(stream_size + access_units.size() * sizeof(aud));
  access_units_with_aud.resize(access_units.size());

  size_t offset = 0;
  for (size_t k = 0; k < access_units.size(); k++) {
    access_units_with_aud[k].offset = offset;
    access_units_with_aud[k].size = access_units[k].size + sizeof(aud);

    memcpy(&stream_with_aud[offset], aud, sizeof(aud));
    offset += sizeof(aud);

    memcpy(&stream_with_aud[offset],
           &stream[access_units[k].offset], access_units[k].size);
    offset += access_units[k].size;
  }
}

}  // namespace

class EsParserH264Test : public testing::Test {
 public:
  EsParserH264Test()
      : sample_count_(0),
        first_frame_is_key_frame_(false) {}

  void LoadStream(const char* filename);
  void ProcessPesPackets(const std::vector<Packet>& pes_packets);

  void EmitSample(std::shared_ptr<MediaSample> sample) {
    sample_count_++;
    if (sample_count_ == 1)
      first_frame_is_key_frame_ = sample->is_key_frame();
  }

  void NewVideoConfig(std::shared_ptr<StreamInfo> config) {
    DVLOG(1) << config->ToString();
    stream_map_[config->track_id()] = config;
  }

  size_t sample_count() const { return sample_count_; }
  bool first_frame_is_key_frame() { return first_frame_is_key_frame_; }

  // Stream with AUD NALUs.
  std::vector<uint8_t> stream_;

  // Access units of the stream with AUD NALUs.
  std::vector<Packet> access_units_;

 protected:
  typedef std::map<int, std::shared_ptr<StreamInfo>> StreamMap;
  StreamMap stream_map_;
  size_t sample_count_;
  bool first_frame_is_key_frame_;
};

void EsParserH264Test::LoadStream(const char* filename) {
  std::vector<uint8_t> buffer = ReadTestDataFile(filename);
  ASSERT_FALSE(buffer.empty());

  // The input file does not have AUDs.
  std::vector<Packet> access_units_without_aud =
      GetAccessUnits(buffer.data(), buffer.size());
  ASSERT_GT(access_units_without_aud.size(), 0u);
  AppendAUD(buffer.data(), buffer.size(), access_units_without_aud, stream_,
            access_units_);
}

void EsParserH264Test::ProcessPesPackets(
    const std::vector<Packet>& pes_packets) {
  // Duration of one 25fps video frame in 90KHz clock units.
  const uint32_t kMpegTicksPerFrame = 3600;

  EsParserH264 es_parser(
      0,
      std::bind(&EsParserH264Test::NewVideoConfig, this, std::placeholders::_1),
      std::bind(&EsParserH264Test::EmitSample, this, std::placeholders::_1));

  size_t au_idx = 0;
  for (size_t k = 0; k < pes_packets.size(); k++) {
    size_t cur_pes_offset = pes_packets[k].offset;
    size_t cur_pes_size = pes_packets[k].size;

    // Update the access unit the PES belongs to from a timing point of view.
    while (au_idx < access_units_.size() - 1 &&
           cur_pes_offset <= access_units_[au_idx + 1].offset &&
           cur_pes_offset + cur_pes_size > access_units_[au_idx + 1].offset) {
      au_idx++;
    }

    // Check whether the PES packet includes the start of an access unit.
    // The timings are relevant only in this case.
    int64_t pts = kNoTimestamp;
    int64_t dts = kNoTimestamp;
    if (cur_pes_offset <= access_units_[au_idx].offset &&
        cur_pes_offset + cur_pes_size > access_units_[au_idx].offset) {
      pts = au_idx * kMpegTicksPerFrame;
    }

    ASSERT_TRUE(es_parser.Parse(&stream_[cur_pes_offset],
                                static_cast<int>(cur_pes_size), pts, dts));
  }
  es_parser.Flush();
}

TEST_F(EsParserH264Test, OneAccessUnitPerPes) {
  LoadStream("bear.h264");

  // One to one equivalence between PES packets and access units.
  std::vector<Packet> pes_packets(access_units_);

  // Process each PES packet.
  ProcessPesPackets(pes_packets);
  EXPECT_EQ(sample_count(), access_units_.size());
  EXPECT_TRUE(first_frame_is_key_frame());
}

TEST_F(EsParserH264Test, NonAlignedPesPacket) {
  LoadStream("bear.h264");

  // Generate the PES packets.
  std::vector<Packet> pes_packets;
  Packet cur_pes_packet;
  cur_pes_packet.offset = 0;
  for (size_t k = 0; k < access_units_.size(); k++) {
    pes_packets.push_back(cur_pes_packet);

    // The current PES packet includes the remaining bytes of the previous
    // access unit and some bytes of the current access unit
    // (487 bytes in this unit test but no more than the current access unit
    // size).
    cur_pes_packet.offset = access_units_[k].offset +
        std::min<size_t>(487u, access_units_[k].size);
  }
  ComputePacketSize(pes_packets, stream_.size());

  // Process each PES packet.
  ProcessPesPackets(pes_packets);
  EXPECT_EQ(sample_count(), access_units_.size());
  EXPECT_TRUE(first_frame_is_key_frame());
}

TEST_F(EsParserH264Test, SeveralPesPerAccessUnit) {
  LoadStream("bear.h264");

  // Get the minimum size of an access unit.
  size_t min_access_unit_size = stream_.size();
  for (size_t k = 0; k < access_units_.size(); k++) {
    if (min_access_unit_size >= access_units_[k].size)
      min_access_unit_size = access_units_[k].size;
  }

  // Use a small PES packet size or the minimum access unit size
  // if it is even smaller.
  size_t pes_size = 512;
  if (min_access_unit_size < pes_size)
    pes_size = min_access_unit_size;

  std::vector<Packet> pes_packets;
  Packet cur_pes_packet;
  cur_pes_packet.offset = 0;
  while (cur_pes_packet.offset < stream_.size()) {
    pes_packets.push_back(cur_pes_packet);
    cur_pes_packet.offset += pes_size;
  }
  ComputePacketSize(pes_packets, stream_.size());

  // Process each PES packet.
  ProcessPesPackets(pes_packets);
  EXPECT_EQ(sample_count(), access_units_.size());
  EXPECT_TRUE(first_frame_is_key_frame());
}

TEST_F(EsParserH264Test, NonIFrameStart) {
  LoadStream("bear_no_iframe_start.h264");

  // One to one equivalence between PES packets and access units.
  std::vector<Packet> pes_packets(access_units_);

  // Process each PES packet.
  ProcessPesPackets(pes_packets);
  // Ensure samples were emitted, but fewer than number of AUDs.
  EXPECT_LT(sample_count(), access_units_.size());
  EXPECT_TRUE(first_frame_is_key_frame());
}

// Verify that the parser can get the the sar width and height.
TEST_F(EsParserH264Test, PixelWidthPixelHeight) {
  LoadStream("bear.h264");
  std::vector<Packet> pes_packets(access_units_);
  ProcessPesPackets(pes_packets);

  const int kVideoTrackId = 0;
  EXPECT_EQ(1u,
            reinterpret_cast<VideoStreamInfo*>(stream_map_[kVideoTrackId].get())
                ->pixel_width());
  EXPECT_EQ(1u,
            reinterpret_cast<VideoStreamInfo*>(stream_map_[kVideoTrackId].get())
                ->pixel_height());
}

}  // namespace mp2t
}  // namespace media
}  // namespace shaka
