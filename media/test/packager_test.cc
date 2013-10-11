// Copyright (c) 2013 Google Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/demuxer.h"
#include "media/base/media_sample.h"
#include "media/base/media_stream.h"
#include "media/base/muxer.h"
#include "media/base/status_test_util.h"
#include "media/base/stream_info.h"
#include "media/base/test_data_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
const char* kMediaFiles[] = {"bear-1280x720.mp4", "bear-1280x720-av_frag.mp4"};
}

namespace media {

class TestingMuxer : public Muxer {
 public:
  TestingMuxer(const Options& options, EncryptorSource* encryptor_source)
      : Muxer(options, encryptor_source) {}

  virtual Status AddSample(const MediaStream* stream,
                           scoped_refptr<MediaSample> sample) {
    DVLOG(1) << "Add Sample: " << sample->ToString();
    DVLOG(2) << "To Stream: " << stream->ToString();
    return Status::OK;
  }

  virtual Status Finalize() {
    DVLOG(1) << "Finalize is called.";
    return Status::OK;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestingMuxer);
};

class PackagerTest : public ::testing::TestWithParam<const char*> {};

TEST_P(PackagerTest, Remux) {
  Demuxer demuxer(
      GetTestDataFilePath(GetParam()).value(), NULL);
  ASSERT_OK(demuxer.Initialize());

  LOG(INFO) << "Num Streams: " << demuxer.num_streams();
  for (int i = 0; i < demuxer.num_streams(); ++i) {
    LOG(INFO) << "Streams " << i << " " << demuxer.streams(i)->ToString();
  }

  Muxer::Options options;
  TestingMuxer muxer(options, NULL);

  ASSERT_OK(muxer.AddStream(demuxer.streams(0)));

  // Starts remuxing process.
  ASSERT_OK(demuxer.Run());

  ASSERT_OK(muxer.Finalize());
}

INSTANTIATE_TEST_CASE_P(PackagerE2ETest,
                        PackagerTest,
                        ::testing::ValuesIn(kMediaFiles));

}  // namespace media
