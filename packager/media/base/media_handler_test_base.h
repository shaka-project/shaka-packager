// Copyright 2017 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "packager/media/base/media_handler.h"

namespace shaka {
namespace media {

class FakeMediaHandler;

MATCHER_P3(IsStreamInfo, stream_index, time_scale, encrypted, "") {
  return arg->stream_index == stream_index &&
         arg->stream_data_type == StreamDataType::kStreamInfo &&
         arg->stream_info->time_scale() == time_scale &&
         arg->stream_info->is_encrypted() == encrypted;
}

MATCHER_P4(IsSegmentInfo, stream_index, timestamp, duration, subsegment, "") {
  return arg->stream_index == stream_index &&
         arg->stream_data_type == StreamDataType::kSegmentInfo &&
         arg->segment_info->start_timestamp == timestamp &&
         arg->segment_info->duration == duration &&
         arg->segment_info->is_subsegment == subsegment;
}

MATCHER_P3(IsMediaSample, stream_index, timestamp, duration, "") {
  return arg->stream_index == stream_index &&
         arg->stream_data_type == StreamDataType::kMediaSample &&
         arg->media_sample->dts() == timestamp &&
         arg->media_sample->duration() == duration;
}

class MediaHandlerTestBase : public ::testing::Test {
 public:
  MediaHandlerTestBase();

  /// Return a stream data with mock stream info.
  std::unique_ptr<StreamData> GetStreamInfoStreamData(int stream_index,
                                                      Codec codec,
                                                      uint32_t time_scale);

  /// Return a stream data with mock video stream info.
  std::unique_ptr<StreamData> GetVideoStreamInfoStreamData(
      int stream_index,
      uint32_t time_scale) {
    return GetStreamInfoStreamData(stream_index, kCodecVP9, time_scale);
  }

  /// Return a stream data with mock audio stream info.
  std::unique_ptr<StreamData> GetAudioStreamInfoStreamData(
      int stream_index,
      uint32_t time_scale) {
    return GetStreamInfoStreamData(stream_index, kCodecAAC, time_scale);
  }

  /// Return a stream data with mock media sample.
  std::unique_ptr<StreamData> GetMediaSampleStreamData(int stream_index,
                                                       int64_t timestamp,
                                                       int64_t duration,
                                                       bool is_keyframe);

  /// Setup a graph using |handler| with |num_inputs| and |num_outputs|.
  void SetUpGraph(int num_inputs,
                  int num_outputs,
                  std::shared_ptr<MediaHandler> handler);

  /// Return the output stream data vector from handler.
  const std::vector<std::unique_ptr<StreamData>>& GetOutputStreamDataVector()
      const;

  /// Clear the output stream data vector.
  void ClearOutputStreamDataVector();

  /// @return some random handler that can be used for testing.
  std::shared_ptr<MediaHandler> some_handler() { return some_handler_; }

 private:
  MediaHandlerTestBase(const MediaHandlerTestBase&) = delete;
  MediaHandlerTestBase& operator=(const MediaHandlerTestBase&) = delete;

  // Get a mock stream info for testing.
  std::shared_ptr<StreamInfo> GetMockStreamInfo(Codec codec,
                                                uint32_t time_scale);

  // Downstream handler used in testing graph.
  std::shared_ptr<FakeMediaHandler> next_handler_;
  // Some random handler which can be used for testing.
  std::shared_ptr<MediaHandler> some_handler_;
};

}  // namespace media
}  // namespace shaka
