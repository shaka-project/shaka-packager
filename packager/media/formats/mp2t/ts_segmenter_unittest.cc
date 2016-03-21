// Copyright 2016 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "packager/media/base/audio_stream_info.h"
#include "packager/media/base/test/status_test_util.h"
#include "packager/media/base/video_stream_info.h"
#include "packager/media/formats/mp2t/ts_segmenter.h"

namespace edash_packager {
namespace media {
namespace mp2t {

using ::testing::InSequence;
using ::testing::Return;
using ::testing::Sequence;
using ::testing::StrEq;
using ::testing::_;

namespace {

// All data here is bogus. They are used to create VideoStreamInfo but the
// actual values don't matter at all.
const bool kIsKeyFrame = true;
const VideoCodec kH264VideoCodec = VideoCodec::kCodecH264;
const uint8_t kExtraData[] = {
    0x00,
};
const int kTrackId = 0;
const uint32_t kTimeScale = 90000;
const uint64_t kDuration = 180000;
const char kCodecString[] = "avc1";
const char kLanguage[] = "eng";
const uint32_t kWidth = 1280;
const uint32_t kHeight = 720;
const uint32_t kPixelWidth = 1;
const uint32_t kPixelHeight = 1;
const uint16_t kTrickPlayRate = 1;
const uint8_t kNaluLengthSize = 1;
const bool kIsEncrypted = false;

class MockPesPacketGenerator : public PesPacketGenerator {
 public:
  MOCK_METHOD1(Initialize, bool(const StreamInfo& info));
  MOCK_METHOD1(PushSample, bool(scoped_refptr<MediaSample> sample));
  MOCK_METHOD0(NumberOfReadyPesPackets, size_t());

  // Hack found at the URL below for mocking methods that return scoped_ptr.
  // https://groups.google.com/a/chromium.org/forum/#!topic/chromium-dev/01sDxsJ1OYw
  MOCK_METHOD0(GetNextPesPacketMock, PesPacket*());
  scoped_ptr<PesPacket> GetNextPesPacket() override {
    return scoped_ptr<PesPacket>(GetNextPesPacketMock());
  }

  MOCK_METHOD0(Flush, bool());
};

class MockTsWriter : public TsWriter {
 public:
  MOCK_METHOD1(Initialize, bool(const StreamInfo& stream_info));
  MOCK_METHOD1(NewSegment, bool(const std::string& file_name));
  MOCK_METHOD0(FinalizeSegment, bool());

  // Similar to the hack above but takes a scoped_ptr.
  MOCK_METHOD1(AddPesPacketMock, bool(PesPacket* pes_packet));
  bool AddPesPacket(scoped_ptr<PesPacket> pes_packet) override {
    // No need to keep the pes packet around for the current tests.
    return AddPesPacketMock(pes_packet.get());
  }

  MOCK_METHOD0(NotifyReinjectPsi, bool());
  MOCK_CONST_METHOD0(TimeScale, uint32_t());
};

}  // namespace

class TsSegmenterTest : public ::testing::Test {
 protected:
  void SetUp() override {
    mock_ts_writer_.reset(new MockTsWriter());
    mock_pes_packet_generator_.reset(new MockPesPacketGenerator());
  }

  scoped_ptr<MockTsWriter> mock_ts_writer_;
  scoped_ptr<MockPesPacketGenerator> mock_pes_packet_generator_;
};

TEST_F(TsSegmenterTest, Initialize) {
  scoped_refptr<VideoStreamInfo> stream_info(new VideoStreamInfo(
      kTrackId, kTimeScale, kDuration, kH264VideoCodec, kCodecString, kLanguage,
      kWidth, kHeight, kPixelWidth, kPixelHeight, kTrickPlayRate,
      kNaluLengthSize, kExtraData, arraysize(kExtraData), kIsEncrypted));
  MuxerOptions options;
  options.segment_template = "file$Number$.ts";
  TsSegmenter segmenter(options);

  EXPECT_CALL(*mock_ts_writer_, Initialize(_)).WillOnce(Return(true));
  EXPECT_CALL(*mock_pes_packet_generator_, Initialize(_))
      .WillOnce(Return(true));

  segmenter.InjectTsWriterForTesting(mock_ts_writer_.Pass());
  segmenter.InjectPesPacketGeneratorForTesting(
      mock_pes_packet_generator_.Pass());

  EXPECT_OK(segmenter.Initialize(*stream_info));
}

TEST_F(TsSegmenterTest, AddSample) {
  scoped_refptr<VideoStreamInfo> stream_info(new VideoStreamInfo(
      kTrackId, kTimeScale, kDuration, kH264VideoCodec, kCodecString, kLanguage,
      kWidth, kHeight, kPixelWidth, kPixelHeight, kTrickPlayRate,
      kNaluLengthSize, kExtraData, arraysize(kExtraData), kIsEncrypted));
  MuxerOptions options;
  options.segment_duration = 10.0;
  options.segment_template = "file$Number$.ts";
  TsSegmenter segmenter(options);

  ON_CALL(*mock_ts_writer_, TimeScale())
      .WillByDefault(Return(kTimeScale));

  EXPECT_CALL(*mock_ts_writer_, Initialize(_)).WillOnce(Return(true));
  EXPECT_CALL(*mock_pes_packet_generator_, Initialize(_))
      .WillOnce(Return(true));

  const uint8_t kAnyData[] = {
      0x01, 0x0F, 0x3C,
  };
  scoped_refptr<MediaSample> sample =
      MediaSample::CopyFrom(kAnyData, arraysize(kAnyData), kIsKeyFrame);

  Sequence writer_sequence;
  EXPECT_CALL(*mock_ts_writer_, NewSegment(StrEq("file1.ts")))
      .InSequence(writer_sequence)
      .WillOnce(Return(true));

  EXPECT_CALL(*mock_pes_packet_generator_, PushSample(_))
      .WillOnce(Return(true));

  Sequence ready_pes_sequence;
  EXPECT_CALL(*mock_pes_packet_generator_, NumberOfReadyPesPackets())
      .InSequence(ready_pes_sequence)
      .WillOnce(Return(1u));
  EXPECT_CALL(*mock_pes_packet_generator_, NumberOfReadyPesPackets())
      .InSequence(ready_pes_sequence)
      .WillOnce(Return(0u));

  EXPECT_CALL(*mock_ts_writer_, AddPesPacketMock(_))
      .WillOnce(Return(true));

  // The pointer is released inside the segmenter.
  PesPacket* pes = new PesPacket();
  pes->set_duration(kTimeScale);
  EXPECT_CALL(*mock_pes_packet_generator_, GetNextPesPacketMock())
      .WillOnce(Return(pes));

  segmenter.InjectTsWriterForTesting(mock_ts_writer_.Pass());
  segmenter.InjectPesPacketGeneratorForTesting(
      mock_pes_packet_generator_.Pass());

  EXPECT_OK(segmenter.Initialize(*stream_info));
  EXPECT_OK(segmenter.AddSample(sample));
}

// Verify the case where the segment is long enough and the current segment
// should be closed.
TEST_F(TsSegmenterTest, PastSegmentDuration) {
  scoped_refptr<VideoStreamInfo> stream_info(new VideoStreamInfo(
      kTrackId, kTimeScale, kDuration, kH264VideoCodec, kCodecString, kLanguage,
      kWidth, kHeight, kPixelWidth, kPixelHeight, kTrickPlayRate,
      kNaluLengthSize, kExtraData, arraysize(kExtraData), kIsEncrypted));
  MuxerOptions options;
  options.segment_duration = 10.0;
  options.segment_template = "file$Number$.ts";
  TsSegmenter segmenter(options);

  ON_CALL(*mock_ts_writer_, TimeScale())
      .WillByDefault(Return(kTimeScale));

  EXPECT_CALL(*mock_ts_writer_, Initialize(_)).WillOnce(Return(true));
  EXPECT_CALL(*mock_pes_packet_generator_, Initialize(_))
      .WillOnce(Return(true));

  const uint8_t kAnyData[] = {
      0x01, 0x0F, 0x3C,
  };
  scoped_refptr<MediaSample> sample =
      MediaSample::CopyFrom(kAnyData, arraysize(kAnyData), kIsKeyFrame);

  Sequence writer_sequence;
  EXPECT_CALL(*mock_ts_writer_, NewSegment(StrEq("file1.ts")))
      .InSequence(writer_sequence)
      .WillOnce(Return(true));

  EXPECT_CALL(*mock_pes_packet_generator_, PushSample(_)).WillOnce(Return(true));

  Sequence ready_pes_sequence;
  EXPECT_CALL(*mock_pes_packet_generator_, NumberOfReadyPesPackets())
      .InSequence(ready_pes_sequence)
      .WillOnce(Return(1u));
  EXPECT_CALL(*mock_pes_packet_generator_, NumberOfReadyPesPackets())
      .InSequence(ready_pes_sequence)
      .WillOnce(Return(0u));

  EXPECT_CALL(*mock_ts_writer_, FinalizeSegment())
      .InSequence(writer_sequence)
      .WillOnce(Return(true));

  EXPECT_CALL(*mock_ts_writer_, AddPesPacketMock(_))
      .WillOnce(Return(true));

  // The pointer is released inside the segmenter.
  PesPacket* pes = new PesPacket();
  // 11 seconds > 10 seconds (segment duration).
  // Expect the segment to be finalized.
  pes->set_duration(11 * kTimeScale);
  EXPECT_CALL(*mock_pes_packet_generator_, GetNextPesPacketMock())
      .WillOnce(Return(pes));

  segmenter.InjectTsWriterForTesting(mock_ts_writer_.Pass());
  segmenter.InjectPesPacketGeneratorForTesting(
      mock_pes_packet_generator_.Pass());
  EXPECT_OK(segmenter.Initialize(*stream_info));
  EXPECT_OK(segmenter.AddSample(sample));
}

// Finalize right after Initialize(). The writer will not be initialized.
TEST_F(TsSegmenterTest, InitializeThenFinalize) {
  scoped_refptr<VideoStreamInfo> stream_info(new VideoStreamInfo(
      kTrackId, kTimeScale, kDuration, kH264VideoCodec, kCodecString, kLanguage,
      kWidth, kHeight, kPixelWidth, kPixelHeight, kTrickPlayRate,
      kNaluLengthSize, kExtraData, arraysize(kExtraData), kIsEncrypted));
  MuxerOptions options;
  options.segment_duration = 10.0;
  options.segment_template = "file$Number$.ts";
  TsSegmenter segmenter(options);

  EXPECT_CALL(*mock_ts_writer_, Initialize(_)).WillOnce(Return(true));
  EXPECT_CALL(*mock_pes_packet_generator_, Initialize(_))
      .WillOnce(Return(true));

  EXPECT_CALL(*mock_pes_packet_generator_, Flush()).WillOnce(Return(true));

  segmenter.InjectTsWriterForTesting(mock_ts_writer_.Pass());
  segmenter.InjectPesPacketGeneratorForTesting(
      mock_pes_packet_generator_.Pass());
  EXPECT_OK(segmenter.Initialize(*stream_info));
  EXPECT_OK(segmenter.Finalize());
}

// Verify the "normal" case where samples have been added and the writer has
// been initialized.
// The test does not really add any samples but instead simulates an initialized
// writer with a mock.
TEST_F(TsSegmenterTest, Finalize) {
  scoped_refptr<VideoStreamInfo> stream_info(new VideoStreamInfo(
      kTrackId, kTimeScale, kDuration, kH264VideoCodec, kCodecString, kLanguage,
      kWidth, kHeight, kPixelWidth, kPixelHeight, kTrickPlayRate,
      kNaluLengthSize, kExtraData, arraysize(kExtraData), kIsEncrypted));
  MuxerOptions options;
  options.segment_duration = 10.0;
  options.segment_template = "file$Number$.ts";
  TsSegmenter segmenter(options);

  EXPECT_CALL(*mock_ts_writer_, Initialize(_)).WillOnce(Return(true));
  EXPECT_CALL(*mock_pes_packet_generator_, Initialize(_))
      .WillOnce(Return(true));

  InSequence s;
  EXPECT_CALL(*mock_pes_packet_generator_, Flush()).WillOnce(Return(true));
  EXPECT_CALL(*mock_pes_packet_generator_, NumberOfReadyPesPackets())
      .WillOnce(Return(0u));
  EXPECT_CALL(*mock_ts_writer_, FinalizeSegment()).WillOnce(Return(true));

  segmenter.InjectTsWriterForTesting(mock_ts_writer_.Pass());
  segmenter.InjectPesPacketGeneratorForTesting(
      mock_pes_packet_generator_.Pass());
  EXPECT_OK(segmenter.Initialize(*stream_info));
  segmenter.SetTsWriterFileOpenedForTesting(true);
  EXPECT_OK(segmenter.Finalize());
}

// Verify that it can generate multiple segments.
TEST_F(TsSegmenterTest, MultipleSegments) {
  scoped_refptr<VideoStreamInfo> stream_info(new VideoStreamInfo(
      kTrackId, kTimeScale, kDuration, kH264VideoCodec, kCodecString, kLanguage,
      kWidth, kHeight, kPixelWidth, kPixelHeight, kTrickPlayRate,
      kNaluLengthSize, kExtraData, arraysize(kExtraData), kIsEncrypted));
  MuxerOptions options;
  options.segment_duration = 10.0;
  options.segment_template = "file$Number$.ts";
  TsSegmenter segmenter(options);

  ON_CALL(*mock_ts_writer_, TimeScale())
      .WillByDefault(Return(kTimeScale));

  EXPECT_CALL(*mock_ts_writer_, Initialize(_)).WillOnce(Return(true));
  EXPECT_CALL(*mock_pes_packet_generator_, Initialize(_))
      .WillOnce(Return(true));

  const uint8_t kAnyData[] = {
      0x01, 0x0F, 0x3C,
  };
  scoped_refptr<MediaSample> sample =
      MediaSample::CopyFrom(kAnyData, arraysize(kAnyData), kIsKeyFrame);

  EXPECT_CALL(*mock_pes_packet_generator_, PushSample(_))
      .Times(2)
      .WillRepeatedly(Return(true));

  Sequence writer_sequence;
  EXPECT_CALL(*mock_ts_writer_, NewSegment(StrEq("file1.ts")))
      .InSequence(writer_sequence)
      .WillOnce(Return(true));

  Sequence ready_pes_sequence;
  EXPECT_CALL(*mock_pes_packet_generator_, NumberOfReadyPesPackets())
      .InSequence(ready_pes_sequence)
      .WillOnce(Return(1u));

  // The pointer is released inside the segmenter.
  PesPacket* pes = new PesPacket();
  // 11 seconds > 10 seconds (segment duration).
  // Expect the segment to be finalized.
  pes->set_duration(11 * kTimeScale);
  EXPECT_CALL(*mock_pes_packet_generator_, GetNextPesPacketMock())
      .InSequence(ready_pes_sequence)
      .WillOnce(Return(pes));

  EXPECT_CALL(*mock_pes_packet_generator_, NumberOfReadyPesPackets())
      .InSequence(ready_pes_sequence)
      .WillOnce(Return(0u));

  EXPECT_CALL(*mock_ts_writer_, AddPesPacketMock(_))
      .InSequence(writer_sequence)
      .WillOnce(Return(true));

  EXPECT_CALL(*mock_ts_writer_, FinalizeSegment())
      .InSequence(writer_sequence)
      .WillOnce(Return(true));

  // Expectations for second AddSample() for the second segment.
  EXPECT_CALL(*mock_ts_writer_, NewSegment(StrEq("file2.ts")))
      .InSequence(writer_sequence)
      .WillOnce(Return(true));

  EXPECT_CALL(*mock_pes_packet_generator_, NumberOfReadyPesPackets())
      .InSequence(ready_pes_sequence)
      .WillOnce(Return(1u));

  // The pointer is released inside the segmenter.
  pes = new PesPacket();
  // 7 < 10 seconds, If FinalizeSegment() is called AddSample will fail (due to
  // mock returning false by default).
  pes->set_duration(7 * kTimeScale);
  EXPECT_CALL(*mock_pes_packet_generator_, GetNextPesPacketMock())
      .InSequence(ready_pes_sequence)
      .WillOnce(Return(pes));

  EXPECT_CALL(*mock_pes_packet_generator_, NumberOfReadyPesPackets())
      .InSequence(ready_pes_sequence)
      .WillOnce(Return(0u));

  EXPECT_CALL(*mock_ts_writer_, AddPesPacketMock(_))
      .InSequence(writer_sequence)
      .WillOnce(Return(true));

  segmenter.InjectTsWriterForTesting(mock_ts_writer_.Pass());
  segmenter.InjectPesPacketGeneratorForTesting(
      mock_pes_packet_generator_.Pass());
  EXPECT_OK(segmenter.Initialize(*stream_info));
  EXPECT_OK(segmenter.AddSample(sample));
  EXPECT_OK(segmenter.AddSample(sample));
}

}  // namespace mp2t
}  // namespace media
}  // namespace edash_packager
