// Copyright 2017 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_BASE_MEDIA_HANDLER_TEST_BASE_H_
#define PACKAGER_MEDIA_BASE_MEDIA_HANDLER_TEST_BASE_H_

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "packager/base/strings/string_number_conversions.h"
#include "packager/media/base/media_handler.h"

namespace shaka {
namespace media {

std::string StreamDataTypeToString(StreamDataType stream_data_type);
std::string BoolToString(bool value);

MATCHER_P(IsStreamInfo, stream_index, "") {
  return arg->stream_index == stream_index &&
         arg->stream_data_type == StreamDataType::kStreamInfo;
}

MATCHER_P3(IsStreamInfo, stream_index, time_scale, encrypted, "") {
  if (arg->stream_data_type != StreamDataType::kStreamInfo) {
    *result_listener << "which is "
                     << StreamDataTypeToString(arg->stream_data_type);
    return false;
  }

  *result_listener << "which is (" << arg->stream_index << ","
                   << arg->stream_info->time_scale() << ","
                   << BoolToString(arg->stream_info->is_encrypted()) << ")";
  return arg->stream_index == stream_index &&
         arg->stream_info->time_scale() == time_scale &&
         arg->stream_info->is_encrypted() == encrypted;
}

MATCHER_P5(IsSegmentInfo,
           stream_index,
           start_timestamp,
           duration,
           subsegment,
           encrypted,
           "") {
  if (arg->stream_data_type != StreamDataType::kSegmentInfo) {
    *result_listener << "which is "
                     << StreamDataTypeToString(arg->stream_data_type);
    return false;
  }

  *result_listener << "which is (" << arg->stream_index << ","
                   << arg->segment_info->start_timestamp << ","
                   << arg->segment_info->duration << ","
                   << BoolToString(arg->segment_info->is_subsegment) << ","
                   << BoolToString(arg->segment_info->is_encrypted) << ")";
  return arg->stream_index == stream_index &&
         arg->segment_info->start_timestamp == start_timestamp &&
         arg->segment_info->duration == duration &&
         arg->segment_info->is_subsegment == subsegment &&
         arg->segment_info->is_encrypted == encrypted;
}

MATCHER_P6(MatchEncryptionConfig,
           protection_scheme,
           crypt_byte_block,
           skip_byte_block,
           per_sample_iv_size,
           constant_iv,
           key_id,
           "") {
  *result_listener << "which is (" << FourCCToString(arg.protection_scheme)
                   << "," << static_cast<int>(arg.crypt_byte_block) << ","
                   << static_cast<int>(arg.skip_byte_block) << ","
                   << static_cast<int>(arg.per_sample_iv_size) << ","
                   << base::HexEncode(arg.constant_iv.data(),
                                      arg.constant_iv.size())
                   << ","
                   << base::HexEncode(arg.key_id.data(), arg.key_id.size())
                   << ")";
  return arg.protection_scheme == protection_scheme &&
         arg.crypt_byte_block == crypt_byte_block &&
         arg.skip_byte_block == skip_byte_block &&
         arg.per_sample_iv_size == per_sample_iv_size &&
         arg.constant_iv == constant_iv && arg.key_id == key_id;
}

MATCHER_P4(IsMediaSample, stream_index, timestamp, duration, encrypted, "") {
  if (arg->stream_data_type != StreamDataType::kMediaSample) {
    *result_listener << "which is "
                     << StreamDataTypeToString(arg->stream_data_type);
    return false;
  }
  *result_listener << "which is (" << arg->stream_index << ","
                   << arg->media_sample->dts() << ","
                   << arg->media_sample->duration() << ","
                   << BoolToString(arg->media_sample->is_encrypted()) << ")";
  return arg->stream_index == stream_index &&
         arg->media_sample->dts() == timestamp &&
         arg->media_sample->duration() == duration &&
         arg->media_sample->is_encrypted() == encrypted;
}

MATCHER_P5(IsTextSample, id, start_time, end_time, settings, payload, "") {
  if (arg->stream_data_type != StreamDataType::kTextSample) {
    *result_listener << "which is "
                     << StreamDataTypeToString(arg->stream_data_type);
    return false;
  }
  *result_listener << "which is (" << arg->text_sample->id() << ","
                   << arg->text_sample->start_time() << ","
                   << arg->text_sample->EndTime() << ","
                   << arg->text_sample->settings() << ","
                   << arg->text_sample->payload() << ")";
  return arg->text_sample->id() == id &&
         arg->text_sample->start_time() == start_time &&
         arg->text_sample->EndTime() == end_time &&
         arg->text_sample->settings() == settings &&
         arg->text_sample->payload() == payload;
}

MATCHER_P2(IsCueEvent, stream_index, timestamp, "") {
  if (arg->stream_data_type != StreamDataType::kCueEvent) {
    *result_listener << "which is "
                     << StreamDataTypeToString(arg->stream_data_type);
    return false;
  }
  *result_listener << "which is (" << arg->stream_index << ","
                   << arg->cue_event->timestamp << ")";
  return arg->stream_index == stream_index &&
         arg->cue_event->timestamp == timestamp;
}

class FakeInputMediaHandler : public MediaHandler {
 public:
  using MediaHandler::Dispatch;
  using MediaHandler::FlushAllDownstreams;
  using MediaHandler::FlushDownstream;

 private:
  bool ValidateOutputStreamIndex(size_t index) const override;
  Status InitializeInternal() override;
  Status Process(std::unique_ptr<StreamData> stream_data) override;
};

class MockOutputMediaHandler : public MediaHandler {
 public:
  MOCK_METHOD1(OnProcess, void(const StreamData*));
  MOCK_METHOD1(OnFlush, void(size_t index));

 private:
  Status InitializeInternal() override;
  Status Process(std::unique_ptr<StreamData> stream_data) override;
  Status OnFlushRequest(size_t index) override;
};

// TODO(vaage) : Remove this test handler and convert other tests to use
//               FakeInputMediaHandler and MockOutputMediaHandler.
class FakeMediaHandler : public MediaHandler {
 public:
  const std::vector<std::unique_ptr<StreamData>>& stream_data_vector() const {
    return stream_data_vector_;
  }
  void clear_stream_data_vector() { stream_data_vector_.clear(); }

 protected:
  Status InitializeInternal() override;
  Status Process(std::unique_ptr<StreamData> stream_data) override;
  Status OnFlushRequest(size_t input_stream_index) override;
  bool ValidateOutputStreamIndex(size_t stream_index) const override;

  std::vector<std::unique_ptr<StreamData>> stream_data_vector_;
};

class MediaHandlerTestBase : public ::testing::Test {
 public:
  MediaHandlerTestBase() = default;

 protected:
  bool IsVideoCodec(Codec codec) const;

  std::unique_ptr<StreamInfo> GetVideoStreamInfo(uint32_t time_scale) const;

  std::unique_ptr<StreamInfo> GetVideoStreamInfo(uint32_t time_scale,
                                                 uint32_t width,
                                                 uint64_t height) const;

  std::unique_ptr<StreamInfo> GetVideoStreamInfo(uint32_t time_scale,
                                                 Codec codec) const;

  std::unique_ptr<StreamInfo> GetVideoStreamInfo(uint32_t time_scale,
                                                 Codec codec,
                                                 uint32_t width,
                                                 uint64_t height) const;

  std::unique_ptr<StreamInfo> GetAudioStreamInfo(uint32_t time_scale) const;

  std::unique_ptr<StreamInfo> GetAudioStreamInfo(uint32_t time_scale,
                                                 Codec codec) const;

  std::shared_ptr<MediaSample> GetMediaSample(int64_t timestamp,
                                              int64_t duration,
                                              bool is_keyframe) const;

  std::shared_ptr<MediaSample> GetMediaSample(int64_t timestamp,
                                              int64_t duration,
                                              bool is_keyframe,
                                              const uint8_t* data,
                                              size_t data_length) const;

  std::unique_ptr<SegmentInfo> GetSegmentInfo(int64_t start_timestamp,
                                              int64_t duration,
                                              bool is_subsegment) const;

  std::unique_ptr<StreamInfo> GetTextStreamInfo() const;

  std::unique_ptr<TextSample> GetTextSample(const std::string& id,
                                            uint64_t start,
                                            uint64_t end,
                                            const std::string& payload) const;

  // Connect and initialize all handlers.
  Status SetUpAndInitializeGraph(std::shared_ptr<MediaHandler> handler,
                                 size_t input_count,
                                 size_t output_count);

  // Get the input handler at |index|. The values of |index| will match the
  // call to |AddInput|.
  FakeInputMediaHandler* Input(size_t index);

  // Get the output handler at |index|. The values of |index| will match the
  // call to |AddOutput|.
  MockOutputMediaHandler* Output(size_t index);

 private:
  MediaHandlerTestBase(const MediaHandlerTestBase&) = delete;
  MediaHandlerTestBase& operator=(const MediaHandlerTestBase&) = delete;

  std::shared_ptr<MediaHandler> handler_;

  std::vector<std::shared_ptr<FakeInputMediaHandler>> inputs_;
  std::vector<std::shared_ptr<MockOutputMediaHandler>> outputs_;
};

class MediaHandlerGraphTestBase : public MediaHandlerTestBase {
 public:
  MediaHandlerGraphTestBase();

 protected:
  /// Setup a graph using |handler| with |num_inputs| and |num_outputs|.
  void SetUpGraph(size_t num_inputs,
                  size_t num_outputs,
                  std::shared_ptr<MediaHandler> handler);

  /// @return the output stream data vector from handler.
  const std::vector<std::unique_ptr<StreamData>>& GetOutputStreamDataVector()
      const;

  /// Clear the output stream data vector.
  void ClearOutputStreamDataVector();

  /// @return some random handler that can be used for testing.
  std::shared_ptr<MediaHandler> some_handler() { return some_handler_; }

  /// @return some a downstream handler that can be used for connecting.
  std::shared_ptr<FakeMediaHandler> next_handler() { return next_handler_; }

 private:
  MediaHandlerGraphTestBase(const MediaHandlerTestBase&) = delete;
  MediaHandlerGraphTestBase& operator=(const MediaHandlerTestBase&) = delete;

  // Downstream handler used in testing graph.
  std::shared_ptr<FakeMediaHandler> next_handler_;
  // Some random handler which can be used for testing.
  std::shared_ptr<MediaHandler> some_handler_;
};

}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_BASE_MEDIA_HANDLER_TEST_BASE_H_
