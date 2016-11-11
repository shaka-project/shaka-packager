// Copyright 2016 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "packager/media/base/audio_stream_info.h"
#include "packager/media/base/fixed_key_source.h"
#include "packager/media/base/test/status_test_util.h"
#include "packager/media/base/video_stream_info.h"
#include "packager/media/event/mock_muxer_listener.h"
#include "packager/media/formats/mp2t/ts_segmenter.h"

namespace shaka {
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
const Codec kH264Codec = Codec::kCodecH264;
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

const uint8_t kAnyData[] = {
    0x01, 0x0F, 0x3C,
};

class MockPesPacketGenerator : public PesPacketGenerator {
 public:
  MOCK_METHOD1(Initialize, bool(const StreamInfo& info));
  MOCK_METHOD1(PushSample, bool(scoped_refptr<MediaSample> sample));
  MOCK_METHOD1(SetEncryptionKeyMock, bool(EncryptionKey* encryption_key));
  bool SetEncryptionKey(
      std::unique_ptr<EncryptionKey> encryption_key) override {
    return SetEncryptionKeyMock(encryption_key.get());
  }

  MOCK_METHOD0(NumberOfReadyPesPackets, size_t());

  // Hack found at the URL below for mocking methods that return
  // std::unique_ptr.
  // https://groups.google.com/a/chromium.org/forum/#!topic/chromium-dev/01sDxsJ1OYw
  MOCK_METHOD0(GetNextPesPacketMock, PesPacket*());
  std::unique_ptr<PesPacket> GetNextPesPacket() override {
    return std::unique_ptr<PesPacket>(GetNextPesPacketMock());
  }

  MOCK_METHOD0(Flush, bool());
};

class MockTsWriter : public TsWriter {
 public:
  MOCK_METHOD1(Initialize, bool(const StreamInfo& stream_info));
  MOCK_METHOD1(NewSegment, bool(const std::string& file_name));
  MOCK_METHOD0(SignalEncrypted, void());
  MOCK_METHOD0(FinalizeSegment, bool());

  // Similar to the hack above but takes a std::unique_ptr.
  MOCK_METHOD1(AddPesPacketMock, bool(PesPacket* pes_packet));
  bool AddPesPacket(std::unique_ptr<PesPacket> pes_packet) override {
    // No need to keep the pes packet around for the current tests.
    return AddPesPacketMock(pes_packet.get());
  }
};

// TODO(rkuroiwa): Add mock_key_source.{h,cc} in media/base.
class MockKeySource : public FixedKeySource {
 public:
  MOCK_METHOD2(GetKey, Status(TrackType track_type, EncryptionKey* key));
};

}  // namespace

class TsSegmenterTest : public ::testing::Test {
 protected:
  void SetUp() override {
    mock_ts_writer_.reset(new MockTsWriter());
    mock_pes_packet_generator_.reset(new MockPesPacketGenerator());
  }

  std::unique_ptr<MockTsWriter> mock_ts_writer_;
  std::unique_ptr<MockPesPacketGenerator> mock_pes_packet_generator_;
};

TEST_F(TsSegmenterTest, Initialize) {
  scoped_refptr<VideoStreamInfo> stream_info(new VideoStreamInfo(
      kTrackId, kTimeScale, kDuration, kH264Codec, kCodecString, kExtraData,
      arraysize(kExtraData), kWidth, kHeight, kPixelWidth, kPixelHeight,
      kTrickPlayRate, kNaluLengthSize, kLanguage, kIsEncrypted));
  MuxerOptions options;
  options.segment_template = "file$Number$.ts";
  TsSegmenter segmenter(options, nullptr);

  EXPECT_CALL(*mock_ts_writer_, Initialize(_)).WillOnce(Return(true));
  EXPECT_CALL(*mock_pes_packet_generator_, Initialize(_))
      .WillOnce(Return(true));

  segmenter.InjectTsWriterForTesting(std::move(mock_ts_writer_));
  segmenter.InjectPesPacketGeneratorForTesting(
      std::move(mock_pes_packet_generator_));

  EXPECT_OK(segmenter.Initialize(*stream_info, nullptr, 0, 0, 0, 0));
}

TEST_F(TsSegmenterTest, AddSample) {
  scoped_refptr<VideoStreamInfo> stream_info(new VideoStreamInfo(
      kTrackId, kTimeScale, kDuration, kH264Codec, kCodecString, kExtraData,
      arraysize(kExtraData), kWidth, kHeight, kPixelWidth, kPixelHeight,
      kTrickPlayRate, kNaluLengthSize, kLanguage, kIsEncrypted));
  MuxerOptions options;
  options.segment_duration = 10.0;
  options.segment_template = "file$Number$.ts";
  TsSegmenter segmenter(options, nullptr);

  EXPECT_CALL(*mock_ts_writer_, Initialize(_)).WillOnce(Return(true));
  EXPECT_CALL(*mock_pes_packet_generator_, Initialize(_))
      .WillOnce(Return(true));

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
  EXPECT_CALL(*mock_pes_packet_generator_, GetNextPesPacketMock())
      .WillOnce(Return(new PesPacket()));

  segmenter.InjectTsWriterForTesting(std::move(mock_ts_writer_));
  segmenter.InjectPesPacketGeneratorForTesting(
      std::move(mock_pes_packet_generator_));

  EXPECT_OK(segmenter.Initialize(*stream_info, nullptr, 0, 0, 0, 0));
  EXPECT_OK(segmenter.AddSample(sample));
}

// Verify the case where the segment is long enough and the current segment
// should be closed.
// This will add 2 samples and verify that the first segment is closed when the
// second sample is added.
TEST_F(TsSegmenterTest, PassedSegmentDuration) {
  // Use something significantly smaller than 90000 to check that the scaling is
  // done correctly in the segmenter.
  const uint32_t kInputTimescale = 1001;
  scoped_refptr<VideoStreamInfo> stream_info(new VideoStreamInfo(
      kTrackId, kInputTimescale, kDuration, kH264Codec, kCodecString,
      kExtraData, arraysize(kExtraData), kWidth, kHeight, kPixelWidth,
      kPixelHeight, kTrickPlayRate, kNaluLengthSize, kLanguage, kIsEncrypted));
  MuxerOptions options;
  options.segment_duration = 10.0;
  options.segment_template = "file$Number$.ts";

  MockMuxerListener mock_listener;
  TsSegmenter segmenter(options, &mock_listener);

  const uint32_t kFirstPts = 1000;

  EXPECT_CALL(*mock_ts_writer_, Initialize(_)).WillOnce(Return(true));
  EXPECT_CALL(*mock_pes_packet_generator_, Initialize(_))
      .WillOnce(Return(true));

  scoped_refptr<MediaSample> sample1 =
      MediaSample::CopyFrom(kAnyData, arraysize(kAnyData), kIsKeyFrame);
  scoped_refptr<MediaSample> sample2 =
      MediaSample::CopyFrom(kAnyData, arraysize(kAnyData), kIsKeyFrame);

  // 11 seconds > 10 seconds (segment duration).
  // Expect the segment to be finalized.
  sample1->set_duration(kInputTimescale * 11);

  // (Finalize is not called at the end of this test so) Expect one segment
  // event. The length should be the same as the above sample that exceeds the
  // duration.
  EXPECT_CALL(mock_listener,
              OnNewSegment("file1.ts", kFirstPts, kTimeScale * 11, _));

  // Doesn't really matter how long this is.
  sample2->set_duration(kInputTimescale * 7);

  Sequence writer_sequence;
  EXPECT_CALL(*mock_ts_writer_, NewSegment(StrEq("file1.ts")))
      .InSequence(writer_sequence)
      .WillOnce(Return(true));

  EXPECT_CALL(*mock_pes_packet_generator_, PushSample(_))
      .Times(2)
      .WillRepeatedly(Return(true));

  Sequence ready_pes_sequence;
  // First AddSample().
  EXPECT_CALL(*mock_pes_packet_generator_, NumberOfReadyPesPackets())
      .InSequence(ready_pes_sequence)
      .WillOnce(Return(1u));
  EXPECT_CALL(*mock_pes_packet_generator_, NumberOfReadyPesPackets())
      .InSequence(ready_pes_sequence)
      .WillOnce(Return(0u));
  // When Flush() is called, inside second AddSample().
  EXPECT_CALL(*mock_pes_packet_generator_, NumberOfReadyPesPackets())
      .InSequence(ready_pes_sequence)
      .WillOnce(Return(0u));
  // Still inside AddSample() but after Flush().
  EXPECT_CALL(*mock_pes_packet_generator_, NumberOfReadyPesPackets())
      .InSequence(ready_pes_sequence)
      .WillOnce(Return(1u));
  EXPECT_CALL(*mock_pes_packet_generator_, NumberOfReadyPesPackets())
      .InSequence(ready_pes_sequence)
      .WillOnce(Return(0u));

  EXPECT_CALL(*mock_pes_packet_generator_, Flush())
      .WillOnce(Return(true));

  EXPECT_CALL(*mock_ts_writer_, FinalizeSegment())
      .InSequence(writer_sequence)
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_ts_writer_, NewSegment(StrEq("file2.ts")))
      .InSequence(writer_sequence)
      .WillOnce(Return(true));

  EXPECT_CALL(*mock_ts_writer_, AddPesPacketMock(_))
      .Times(2)
      .WillRepeatedly(Return(true));

  // The pointers are released inside the segmenter.
  Sequence pes_packet_sequence;
  PesPacket* first_pes = new PesPacket();
  first_pes->set_pts(kFirstPts);
  EXPECT_CALL(*mock_pes_packet_generator_, GetNextPesPacketMock())
      .InSequence(pes_packet_sequence)
      .WillOnce(Return(first_pes));
  EXPECT_CALL(*mock_pes_packet_generator_, GetNextPesPacketMock())
      .InSequence(pes_packet_sequence)
      .WillOnce(Return(new PesPacket()));

  segmenter.InjectTsWriterForTesting(std::move(mock_ts_writer_));
  segmenter.InjectPesPacketGeneratorForTesting(
      std::move(mock_pes_packet_generator_));
  EXPECT_OK(segmenter.Initialize(*stream_info, nullptr, 0, 0, 0, 0));
  EXPECT_OK(segmenter.AddSample(sample1));
  EXPECT_OK(segmenter.AddSample(sample2));
}

// Finalize right after Initialize(). The writer will not be initialized.
TEST_F(TsSegmenterTest, InitializeThenFinalize) {
  scoped_refptr<VideoStreamInfo> stream_info(new VideoStreamInfo(
      kTrackId, kTimeScale, kDuration, kH264Codec, kCodecString, kExtraData,
      arraysize(kExtraData), kWidth, kHeight, kPixelWidth, kPixelHeight,
      kTrickPlayRate, kNaluLengthSize, kLanguage, kIsEncrypted));
  MuxerOptions options;
  options.segment_duration = 10.0;
  options.segment_template = "file$Number$.ts";
  TsSegmenter segmenter(options, nullptr);

  EXPECT_CALL(*mock_ts_writer_, Initialize(_)).WillOnce(Return(true));
  EXPECT_CALL(*mock_pes_packet_generator_, Initialize(_))
      .WillOnce(Return(true));

  EXPECT_CALL(*mock_pes_packet_generator_, Flush()).WillOnce(Return(true));
  ON_CALL(*mock_pes_packet_generator_, NumberOfReadyPesPackets())
      .WillByDefault(Return(0));

  segmenter.InjectTsWriterForTesting(std::move(mock_ts_writer_));
  segmenter.InjectPesPacketGeneratorForTesting(
      std::move(mock_pes_packet_generator_));
  EXPECT_OK(segmenter.Initialize(*stream_info, nullptr, 0, 0, 0, 0));
  EXPECT_OK(segmenter.Finalize());
}

// Verify the "normal" case where samples have been added and the writer has
// been initialized.
// The test does not really add any samples but instead simulates an initialized
// writer with a mock.
TEST_F(TsSegmenterTest, Finalize) {
  scoped_refptr<VideoStreamInfo> stream_info(new VideoStreamInfo(
      kTrackId, kTimeScale, kDuration, kH264Codec, kCodecString, kExtraData,
      arraysize(kExtraData), kWidth, kHeight, kPixelWidth, kPixelHeight,
      kTrickPlayRate, kNaluLengthSize, kLanguage, kIsEncrypted));
  MuxerOptions options;
  options.segment_duration = 10.0;
  options.segment_template = "file$Number$.ts";
  TsSegmenter segmenter(options, nullptr);

  EXPECT_CALL(*mock_ts_writer_, Initialize(_)).WillOnce(Return(true));
  EXPECT_CALL(*mock_pes_packet_generator_, Initialize(_))
      .WillOnce(Return(true));

  InSequence s;
  EXPECT_CALL(*mock_pes_packet_generator_, Flush()).WillOnce(Return(true));
  EXPECT_CALL(*mock_pes_packet_generator_, NumberOfReadyPesPackets())
      .WillOnce(Return(0u));
  EXPECT_CALL(*mock_ts_writer_, FinalizeSegment()).WillOnce(Return(true));

  segmenter.InjectTsWriterForTesting(std::move(mock_ts_writer_));
  segmenter.InjectPesPacketGeneratorForTesting(
      std::move(mock_pes_packet_generator_));
  EXPECT_OK(segmenter.Initialize(*stream_info, nullptr, 0, 0, 0, 0));
  segmenter.SetTsWriterFileOpenedForTesting(true);
  EXPECT_OK(segmenter.Finalize());
}

// Verify that it won't finish a segment if the sample is not a key frame.
TEST_F(TsSegmenterTest, SegmentOnlyBeforeKeyFrame) {
  scoped_refptr<VideoStreamInfo> stream_info(new VideoStreamInfo(
      kTrackId, kTimeScale, kDuration, kH264Codec, kCodecString, kExtraData,
      arraysize(kExtraData), kWidth, kHeight, kPixelWidth, kPixelHeight,
      kTrickPlayRate, kNaluLengthSize, kLanguage, kIsEncrypted));
  MuxerOptions options;
  options.segment_duration = 10.0;
  options.segment_template = "file$Number$.ts";
  TsSegmenter segmenter(options, nullptr);

  EXPECT_CALL(*mock_ts_writer_, Initialize(_)).WillOnce(Return(true));
  EXPECT_CALL(*mock_pes_packet_generator_, Initialize(_))
      .WillOnce(Return(true));

  const uint8_t kAnyData[] = {
      0x01, 0x0F, 0x3C,
  };
  scoped_refptr<MediaSample> key_frame_sample1 =
      MediaSample::CopyFrom(kAnyData, arraysize(kAnyData), kIsKeyFrame);
  scoped_refptr<MediaSample> non_key_frame_sample =
      MediaSample::CopyFrom(kAnyData, arraysize(kAnyData), !kIsKeyFrame);
  scoped_refptr<MediaSample> key_frame_sample2 =
      MediaSample::CopyFrom(kAnyData, arraysize(kAnyData), kIsKeyFrame);

  // 11 seconds > 10 seconds (segment duration).
  key_frame_sample1->set_duration(kTimeScale * 11);

  // But since the second sample is not a key frame, it shouldn't be segmented.
  non_key_frame_sample->set_duration(kTimeScale * 7);

  // Since this is a key frame, it should be segmented when this is added.
  key_frame_sample2->set_duration(kTimeScale * 3);

  EXPECT_CALL(*mock_pes_packet_generator_, PushSample(_))
      .Times(3)
      .WillRepeatedly(Return(true));

  Sequence writer_sequence;
  EXPECT_CALL(*mock_ts_writer_, NewSegment(StrEq("file1.ts")))
      .InSequence(writer_sequence)
      .WillOnce(Return(true));

  Sequence ready_pes_sequence;
  // First AddSample().
  EXPECT_CALL(*mock_pes_packet_generator_, NumberOfReadyPesPackets())
      .InSequence(ready_pes_sequence)
      .WillOnce(Return(1u));
  EXPECT_CALL(*mock_pes_packet_generator_, NumberOfReadyPesPackets())
      .InSequence(ready_pes_sequence)
      .WillOnce(Return(0u));
  // Second AddSample().
  EXPECT_CALL(*mock_pes_packet_generator_, NumberOfReadyPesPackets())
      .InSequence(ready_pes_sequence)
      .WillOnce(Return(1u));
  EXPECT_CALL(*mock_pes_packet_generator_, NumberOfReadyPesPackets())
      .InSequence(ready_pes_sequence)
      .WillOnce(Return(0u));
  // Third AddSample(), in Flush().
  EXPECT_CALL(*mock_pes_packet_generator_, NumberOfReadyPesPackets())
      .InSequence(ready_pes_sequence)
      .WillOnce(Return(0u));
  // Third AddSample() after Flush().
  EXPECT_CALL(*mock_pes_packet_generator_, NumberOfReadyPesPackets())
      .InSequence(ready_pes_sequence)
      .WillOnce(Return(1u));
  EXPECT_CALL(*mock_pes_packet_generator_, NumberOfReadyPesPackets())
      .InSequence(ready_pes_sequence)
      .WillOnce(Return(0u));

  EXPECT_CALL(*mock_pes_packet_generator_, Flush())
      .WillOnce(Return(true));

  EXPECT_CALL(*mock_ts_writer_, FinalizeSegment())
      .InSequence(writer_sequence)
      .WillOnce(Return(true));

  // Expectations for second AddSample() for the second segment.
  EXPECT_CALL(*mock_ts_writer_, NewSegment(StrEq("file2.ts")))
      .InSequence(writer_sequence)
      .WillOnce(Return(true));

  EXPECT_CALL(*mock_ts_writer_, AddPesPacketMock(_))
      .Times(3)
      .WillRepeatedly(Return(true));

  // The pointers are released inside the segmenter.
  Sequence pes_packet_sequence;
  EXPECT_CALL(*mock_pes_packet_generator_, GetNextPesPacketMock())
      .InSequence(pes_packet_sequence)
      .WillOnce(Return(new PesPacket()));
  EXPECT_CALL(*mock_pes_packet_generator_, GetNextPesPacketMock())
      .InSequence(pes_packet_sequence)
      .WillOnce(Return(new PesPacket()));
  EXPECT_CALL(*mock_pes_packet_generator_, GetNextPesPacketMock())
      .InSequence(pes_packet_sequence)
      .WillOnce(Return(new PesPacket()));

  segmenter.InjectTsWriterForTesting(std::move(mock_ts_writer_));
  segmenter.InjectPesPacketGeneratorForTesting(
      std::move(mock_pes_packet_generator_));
  EXPECT_OK(segmenter.Initialize(*stream_info, nullptr, 0, 0, 0, 0));
  EXPECT_OK(segmenter.AddSample(key_frame_sample1));
  EXPECT_OK(segmenter.AddSample(non_key_frame_sample));
  EXPECT_OK(segmenter.AddSample(key_frame_sample2));
}

TEST_F(TsSegmenterTest, WithEncryptionNoClearLead) {
  scoped_refptr<VideoStreamInfo> stream_info(new VideoStreamInfo(
      kTrackId, kTimeScale, kDuration, kH264Codec, kCodecString, kExtraData,
      arraysize(kExtraData), kWidth, kHeight, kPixelWidth, kPixelHeight,
      kTrickPlayRate, kNaluLengthSize, kLanguage, kIsEncrypted));
  MuxerOptions options;
  options.segment_duration = 10.0;
  options.segment_template = "file$Number$.ts";

  MockMuxerListener mock_listener;
  EXPECT_CALL(mock_listener, OnEncryptionInfoReady(_, _, _, _, _));
  TsSegmenter segmenter(options, &mock_listener);

  EXPECT_CALL(*mock_ts_writer_, Initialize(_)).WillOnce(Return(true));
  EXPECT_CALL(*mock_ts_writer_, SignalEncrypted());
  EXPECT_CALL(*mock_pes_packet_generator_, Initialize(_))
      .WillOnce(Return(true));

  EXPECT_CALL(*mock_pes_packet_generator_, SetEncryptionKeyMock(_))
      .WillOnce(Return(true));

  segmenter.InjectTsWriterForTesting(std::move(mock_ts_writer_));
  segmenter.InjectPesPacketGeneratorForTesting(
      std::move(mock_pes_packet_generator_));

  MockKeySource mock_key_source;
  EXPECT_CALL(mock_key_source, GetKey(KeySource::TRACK_TYPE_HD, _))
      .WillOnce(Return(Status::OK));

  const uint32_t k480pPixels = 640 * 480;
  const uint32_t k1080pPixels = 1920 * 1080;
  const uint32_t k2160pPixels = 4096 * 2160;
  // Set this to 0 so that Finalize will call
  // PesPacketGenerator::SetEncryptionKey().
  // Even tho no samples have been added.
  const double kClearLeadSeconds = 0;
  EXPECT_OK(segmenter.Initialize(*stream_info, &mock_key_source, k480pPixels,
                                 k1080pPixels, k2160pPixels,
                                 kClearLeadSeconds));
}

// Verify that the muxer listener pointer is not used without checking that it's
// not null.
TEST_F(TsSegmenterTest, WithEncryptionNoClearLeadNoMuxerListener) {
  scoped_refptr<VideoStreamInfo> stream_info(new VideoStreamInfo(
      kTrackId, kTimeScale, kDuration, kH264Codec, kCodecString, kExtraData,
      arraysize(kExtraData), kWidth, kHeight, kPixelWidth, kPixelHeight,
      kTrickPlayRate, kNaluLengthSize, kLanguage, kIsEncrypted));
  MuxerOptions options;
  options.segment_duration = 10.0;
  options.segment_template = "file$Number$.ts";

  TsSegmenter segmenter(options, nullptr);

  EXPECT_CALL(*mock_ts_writer_, Initialize(_)).WillOnce(Return(true));
  EXPECT_CALL(*mock_ts_writer_, SignalEncrypted());
  EXPECT_CALL(*mock_pes_packet_generator_, Initialize(_))
      .WillOnce(Return(true));

  EXPECT_CALL(*mock_pes_packet_generator_, SetEncryptionKeyMock(_))
      .WillOnce(Return(true));

  segmenter.InjectTsWriterForTesting(std::move(mock_ts_writer_));
  segmenter.InjectPesPacketGeneratorForTesting(
      std::move(mock_pes_packet_generator_));

  MockKeySource mock_key_source;
  EXPECT_CALL(mock_key_source, GetKey(KeySource::TRACK_TYPE_HD, _))
      .WillOnce(Return(Status::OK));

  const uint32_t k480pPixels = 640 * 480;
  const uint32_t k1080pPixels = 1920 * 1080;
  const uint32_t k2160pPixels = 4096 * 2160;
  // Set this to 0 so that Finalize will call
  // PesPacketGenerator::SetEncryptionKey().
  // Even tho no samples have been added.
  const double kClearLeadSeconds = 0;
  EXPECT_OK(segmenter.Initialize(*stream_info, &mock_key_source, k480pPixels,
                                 k1080pPixels, k2160pPixels,
                                 kClearLeadSeconds));
}

// Verify that encryption notification is sent to objects after clear lead.
TEST_F(TsSegmenterTest, WithEncryptionWithClearLead) {
  scoped_refptr<VideoStreamInfo> stream_info(new VideoStreamInfo(
      kTrackId, kTimeScale, kDuration, kH264Codec, kCodecString, kExtraData,
      arraysize(kExtraData), kWidth, kHeight, kPixelWidth, kPixelHeight,
      kTrickPlayRate, kNaluLengthSize, kLanguage, kIsEncrypted));
  MuxerOptions options;

  options.segment_duration = 1.0;
  const uint32_t k1080pPixels = 1920 * 1080;
  const uint32_t k2160pPixels = 4096 * 2160;
  const double kClearLeadSeconds = 1.0;
  options.segment_template = "file$Number$.ts";

  MockMuxerListener mock_listener;
  TsSegmenter segmenter(options, &mock_listener);

  ON_CALL(*mock_ts_writer_, Initialize(_)).WillByDefault(Return(true));
  ON_CALL(*mock_ts_writer_, NewSegment(_)).WillByDefault(Return(true));
  ON_CALL(*mock_ts_writer_, FinalizeSegment()).WillByDefault(Return(true));
  ON_CALL(*mock_ts_writer_, AddPesPacketMock(_)).WillByDefault(Return(true));
  ON_CALL(*mock_pes_packet_generator_, Initialize(_))
      .WillByDefault(Return(true));
  ON_CALL(*mock_pes_packet_generator_, Flush()).WillByDefault(Return(true));

  const uint8_t kAnyData[] = {
      0x01, 0x0F, 0x3C,
  };
  scoped_refptr<MediaSample> sample1 =
      MediaSample::CopyFrom(kAnyData, arraysize(kAnyData), kIsKeyFrame);
  scoped_refptr<MediaSample> sample2 =
      MediaSample::CopyFrom(kAnyData, arraysize(kAnyData), kIsKeyFrame);

  // Something longer than 1.0 (segment duration and clear lead).
  sample1->set_duration(kTimeScale * 2);
  // The length of the second sample doesn't really matter.
  sample2->set_duration(kTimeScale * 3);

  EXPECT_CALL(*mock_pes_packet_generator_, PushSample(_))
      .Times(2)
      .WillRepeatedly(Return(true));

  Sequence ready_pes_sequence;
  // First AddSample().
  EXPECT_CALL(*mock_pes_packet_generator_, NumberOfReadyPesPackets())
      .InSequence(ready_pes_sequence)
      .WillOnce(Return(1u));
  EXPECT_CALL(*mock_pes_packet_generator_, NumberOfReadyPesPackets())
      .InSequence(ready_pes_sequence)
      .WillOnce(Return(0u));
  // When Flush() is called, inside second AddSample().
  EXPECT_CALL(*mock_pes_packet_generator_, NumberOfReadyPesPackets())
      .InSequence(ready_pes_sequence)
      .WillOnce(Return(0u));
  // Still inside AddSample() but after Flush().
  EXPECT_CALL(*mock_pes_packet_generator_, NumberOfReadyPesPackets())
      .InSequence(ready_pes_sequence)
      .WillOnce(Return(1u));
  EXPECT_CALL(*mock_pes_packet_generator_, NumberOfReadyPesPackets())
      .InSequence(ready_pes_sequence)
      .WillOnce(Return(0u));

  EXPECT_CALL(*mock_ts_writer_, AddPesPacketMock(_))
      .Times(2)
      .WillRepeatedly(Return(true));

  // The pointers are released inside the segmenter.
  Sequence pes_packet_sequence;
  EXPECT_CALL(*mock_pes_packet_generator_, GetNextPesPacketMock())
      .InSequence(pes_packet_sequence)
      .WillOnce(Return(new PesPacket()));
  EXPECT_CALL(*mock_pes_packet_generator_, GetNextPesPacketMock())
      .InSequence(pes_packet_sequence)
      .WillOnce(Return(new PesPacket()));

  MockPesPacketGenerator* mock_pes_packet_generator_raw =
      mock_pes_packet_generator_.get();

  MockTsWriter* mock_ts_writer_raw = mock_ts_writer_.get();

  segmenter.InjectTsWriterForTesting(std::move(mock_ts_writer_));
  segmenter.InjectPesPacketGeneratorForTesting(
      std::move(mock_pes_packet_generator_));

  MockKeySource mock_key_source;
  // This should be called AFTER the first AddSample().
  EXPECT_CALL(mock_key_source, GetKey(KeySource::TRACK_TYPE_HD, _))
      .WillOnce(Return(Status::OK));

  EXPECT_CALL(mock_listener, OnEncryptionInfoReady(_, _, _, _, _));
  EXPECT_OK(segmenter.Initialize(*stream_info, &mock_key_source, 0,
                                 k1080pPixels, k2160pPixels,
                                 kClearLeadSeconds));
  EXPECT_OK(segmenter.AddSample(sample1));

  // These should be called AFTER the first AddSample(), before the second
  // segment.
  EXPECT_CALL(mock_listener, OnEncryptionStart());
  EXPECT_CALL(*mock_pes_packet_generator_raw, SetEncryptionKeyMock(_))
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_ts_writer_raw, SignalEncrypted());
  EXPECT_OK(segmenter.AddSample(sample2));
}

}  // namespace mp2t
}  // namespace media
}  // namespace shaka
