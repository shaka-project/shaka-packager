// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/decoder_buffer.h"
#include "media/base/stream_parser_buffer.h"
#include "media/base/test_data_util.h"
#include "media/base/video_decoder_config.h"
#include "media/mp4/es_descriptor.h"
#include "media/mp4/mp4_stream_parser.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::TimeDelta;

namespace media {
namespace mp4 {

// TODO(xhwang): Figure out the init data type appropriately once it's spec'ed.
static const char kMp4InitDataType[] = "video/mp4";

class MP4StreamParserTest : public testing::Test {
 public:
  MP4StreamParserTest()
      : configs_received_(false) {
    std::set<int> audio_object_types;
    audio_object_types.insert(kISO_14496_3);
    parser_.reset(new MP4StreamParser(audio_object_types, false));
  }

 protected:
  scoped_ptr<MP4StreamParser> parser_;
  bool configs_received_;

  bool AppendData(const uint8* data, size_t length) {
    return parser_->Parse(data, length);
  }

  bool AppendDataInPieces(const uint8* data, size_t length, size_t piece_size) {
    const uint8* start = data;
    const uint8* end = data + length;
    while (start < end) {
      size_t append_size = std::min(piece_size,
                                    static_cast<size_t>(end - start));
      if (!AppendData(start, append_size))
        return false;
      start += append_size;
    }
    return true;
  }

  void InitF(bool init_ok, base::TimeDelta duration) {
    DVLOG(1) << "InitF: ok=" << init_ok
             << ", dur=" << duration.InMilliseconds();
  }

  bool NewConfigF(const AudioDecoderConfig& ac, const VideoDecoderConfig& vc) {
    DVLOG(1) << "NewConfigF: audio=" << ac.IsValidConfig()
             << ", video=" << vc.IsValidConfig();
    configs_received_ = true;
    return true;
  }


  void DumpBuffers(const std::string& label,
                   const StreamParser::BufferQueue& buffers) {
    DVLOG(2) << "DumpBuffers: " << label << " size " << buffers.size();
    for (StreamParser::BufferQueue::const_iterator buf = buffers.begin();
         buf != buffers.end(); buf++) {
      DVLOG(3) << "  n=" << buf - buffers.begin()
               << ", size=" << (*buf)->data_size()
               << ", dur=" << (*buf)->duration().InMilliseconds();
    }
  }

  bool NewBuffersF(const StreamParser::BufferQueue& audio_buffers,
                   const StreamParser::BufferQueue& video_buffers) {
    DumpBuffers("audio_buffers", audio_buffers);
    DumpBuffers("video_buffers", video_buffers);
    return true;
  }

  bool NewTextBuffersF(TextTrack* text_track,
                       const StreamParser::BufferQueue& buffers) {
    return true;
  }

  void KeyNeededF(const std::string& type,
                  scoped_ptr<uint8[]> init_data, int init_data_size) {
    DVLOG(1) << "KeyNeededF: " << init_data_size;
    EXPECT_EQ(kMp4InitDataType, type);
    EXPECT_TRUE(init_data.get());
    EXPECT_GT(init_data_size, 0);
  }

  scoped_ptr<TextTrack> AddTextTrackF(
      TextKind kind,
      const std::string& label,
      const std::string& language) {
    return scoped_ptr<TextTrack>();
  }

  void NewSegmentF() {
    DVLOG(1) << "NewSegmentF";
  }

  void EndOfSegmentF() {
    DVLOG(1) << "EndOfSegmentF()";
  }

  void InitializeParser() {
    parser_->Init(
        base::Bind(&MP4StreamParserTest::InitF, base::Unretained(this)),
        base::Bind(&MP4StreamParserTest::NewConfigF, base::Unretained(this)),
        base::Bind(&MP4StreamParserTest::NewBuffersF, base::Unretained(this)),
        base::Bind(&MP4StreamParserTest::NewTextBuffersF,
                   base::Unretained(this)),
        base::Bind(&MP4StreamParserTest::KeyNeededF, base::Unretained(this)),
        base::Bind(&MP4StreamParserTest::AddTextTrackF, base::Unretained(this)),
        base::Bind(&MP4StreamParserTest::NewSegmentF, base::Unretained(this)),
        base::Bind(&MP4StreamParserTest::EndOfSegmentF,
                   base::Unretained(this)),
        LogCB());
  }

  bool ParseMP4File(const std::string& filename, int append_bytes) {
    InitializeParser();

    scoped_refptr<DecoderBuffer> buffer = ReadTestDataFile(filename);
    EXPECT_TRUE(AppendDataInPieces(buffer->data(),
                                   buffer->data_size(),
                                   append_bytes));
    return true;
  }
};

TEST_F(MP4StreamParserTest, UnalignedAppend) {
  // Test small, non-segment-aligned appends (small enough to exercise
  // incremental append system)
  ParseMP4File("bear-1280x720-av_frag.mp4", 512);
}

TEST_F(MP4StreamParserTest, BytewiseAppend) {
  // Ensure no incremental errors occur when parsing
  ParseMP4File("bear-1280x720-av_frag.mp4", 1);
}

TEST_F(MP4StreamParserTest, MultiFragmentAppend) {
  // Large size ensures multiple fragments are appended in one call (size is
  // larger than this particular test file)
  ParseMP4File("bear-1280x720-av_frag.mp4", 768432);
}

TEST_F(MP4StreamParserTest, Flush) {
  // Flush while reading sample data, then start a new stream.
  InitializeParser();

  scoped_refptr<DecoderBuffer> buffer =
      ReadTestDataFile("bear-1280x720-av_frag.mp4");
  EXPECT_TRUE(AppendDataInPieces(buffer->data(), 65536, 512));
  parser_->Flush();
  EXPECT_TRUE(AppendDataInPieces(buffer->data(),
                                 buffer->data_size(),
                                 512));
}

TEST_F(MP4StreamParserTest, Reinitialization) {
  InitializeParser();

  scoped_refptr<DecoderBuffer> buffer =
      ReadTestDataFile("bear-1280x720-av_frag.mp4");
  EXPECT_TRUE(AppendDataInPieces(buffer->data(),
                                 buffer->data_size(),
                                 512));
  EXPECT_TRUE(AppendDataInPieces(buffer->data(),
                                 buffer->data_size(),
                                 512));
}

TEST_F(MP4StreamParserTest, MPEG2_AAC_LC) {
  std::set<int> audio_object_types;
  audio_object_types.insert(kISO_13818_7_AAC_LC);
  parser_.reset(new MP4StreamParser(audio_object_types, false));
  ParseMP4File("bear-mpeg2-aac-only_frag.mp4", 512);
}

// Test that a moov box is not always required after Flush() is called.
TEST_F(MP4StreamParserTest, NoMoovAfterFlush) {
  InitializeParser();

  scoped_refptr<DecoderBuffer> buffer =
      ReadTestDataFile("bear-1280x720-av_frag.mp4");
  EXPECT_TRUE(AppendDataInPieces(buffer->data(),
                                 buffer->data_size(),
                                 512));
  parser_->Flush();

  const int kFirstMoofOffset = 1307;
  EXPECT_TRUE(AppendDataInPieces(buffer->data() + kFirstMoofOffset,
                                 buffer->data_size() - kFirstMoofOffset,
                                 512));
}

// TODO(strobe): Create and test media which uses CENC auxiliary info stored
// inside a private box

}  // namespace mp4
}  // namespace media
