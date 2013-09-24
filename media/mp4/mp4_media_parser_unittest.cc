// Copyright (c) 2013 Google Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mp4/mp4_media_parser.h"

#include <algorithm>
#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "media/base/media_sample.h"
#include "media/base/stream_info.h"
#include "media/base/test_data_util.h"
#include "media/mp4/es_descriptor.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace mp4 {

class MP4MediaParserTest : public testing::Test {
 public:
  MP4MediaParserTest() : configs_received_(false) {
    parser_.reset(new MP4MediaParser());
  }

 protected:
  scoped_ptr<MP4MediaParser> parser_;
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

  void InitF(bool init_ok, std::vector<scoped_refptr<StreamInfo> >& streams) {
    DVLOG(1) << "InitF: ok=" << init_ok;
    if (init_ok && streams.size() > 0)
      configs_received_ = true;
  }

  bool NewSampleF(uint32 track_id, const scoped_refptr<MediaSample>& sample) {
    DVLOG(2) << "Track Id: " << track_id << " "
             << sample->ToString();
    return true;
  }

  void KeyNeededF(MediaContainerName container,
                  scoped_ptr<uint8[]> init_data,
                  int init_data_size) {
    DVLOG(1) << "KeyNeededF: " << init_data_size;
    EXPECT_TRUE(init_data.get());
    EXPECT_GT(init_data_size, 0);
  }

  void InitializeParser() {
    parser_->Init(
        base::Bind(&MP4MediaParserTest::InitF, base::Unretained(this)),
        base::Bind(&MP4MediaParserTest::NewSampleF, base::Unretained(this)),
        base::Bind(&MP4MediaParserTest::KeyNeededF, base::Unretained(this)));
  }

  bool ParseMP4File(const std::string& filename, int append_bytes) {
    InitializeParser();

    std::vector<uint8> buffer = ReadTestDataFile(filename);
    EXPECT_TRUE(AppendDataInPieces(buffer.data(),
                                   buffer.size(),
                                   append_bytes));
    return true;
  }
};

TEST_F(MP4MediaParserTest, UnalignedAppend) {
  // Test small, non-segment-aligned appends (small enough to exercise
  // incremental append system)
  ParseMP4File("bear-1280x720-av_frag.mp4", 512);
}

TEST_F(MP4MediaParserTest, BytewiseAppend) {
  // Ensure no incremental errors occur when parsing
  ParseMP4File("bear-1280x720-av_frag.mp4", 1);
}

TEST_F(MP4MediaParserTest, MultiFragmentAppend) {
  // Large size ensures multiple fragments are appended in one call (size is
  // larger than this particular test file)
  ParseMP4File("bear-1280x720-av_frag.mp4", 768432);
}

TEST_F(MP4MediaParserTest, Reinitialization) {
  InitializeParser();

  std::vector<uint8> buffer =
      ReadTestDataFile("bear-1280x720-av_frag.mp4");
  EXPECT_TRUE(AppendDataInPieces(buffer.data(),
                                 buffer.size(),
                                 512));
  EXPECT_TRUE(AppendDataInPieces(buffer.data(),
                                 buffer.size(),
                                 512));
}

TEST_F(MP4MediaParserTest, MPEG2_AAC_LC) {
  parser_.reset(new MP4MediaParser());
  ParseMP4File("bear-mpeg2-aac-only_frag.mp4", 512);
}

// TODO(strobe): Create and test media which uses CENC auxiliary info stored
// inside a private box

}  // namespace mp4
}  // namespace media
