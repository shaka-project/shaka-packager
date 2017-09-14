// Copyright 2017 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "packager/base/strings/string_number_conversions.h"
#include "packager/media/base/media_handler.h"

namespace shaka {
namespace media {

MATCHER_P3(IsStreamInfo, stream_index, time_scale, encrypted, "") {
  *result_listener << "which is (" << stream_index << "," << time_scale << ","
                   << (encrypted ? "encrypted" : "not encrypted") << ")";
  return arg->stream_index == stream_index &&
         arg->stream_data_type == StreamDataType::kStreamInfo &&
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
  *result_listener << "which is (" << stream_index << "," << start_timestamp
                   << "," << duration << ","
                   << (subsegment ? "subsegment" : "not subsegment") << ","
                   << (encrypted ? "encrypted" : "not encrypted") << ")";
  return arg->stream_index == stream_index &&
         arg->stream_data_type == StreamDataType::kSegmentInfo &&
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
  *result_listener << "which is (" << FourCCToString(protection_scheme) << ","
                   << static_cast<int>(crypt_byte_block) << ","
                   << static_cast<int>(skip_byte_block) << ","
                   << static_cast<int>(per_sample_iv_size) << ","
                   << base::HexEncode(constant_iv.data(), constant_iv.size())
                   << "," << base::HexEncode(key_id.data(), key_id.size())
                   << ")";
  return arg.protection_scheme == protection_scheme &&
         arg.crypt_byte_block == crypt_byte_block &&
         arg.skip_byte_block == skip_byte_block &&
         arg.per_sample_iv_size == per_sample_iv_size &&
         arg.constant_iv == constant_iv && arg.key_id == key_id;
}

MATCHER_P4(IsMediaSample, stream_index, timestamp, duration, encrypted, "") {
  *result_listener << "which is (" << stream_index << "," << timestamp << ","
                   << duration << ","
                   << (encrypted ? "encrypted" : "not encrypted") << ")";
  return arg->stream_index == stream_index &&
         arg->stream_data_type == StreamDataType::kMediaSample &&
         arg->media_sample->dts() == timestamp &&
         arg->media_sample->duration() == duration &&
         arg->media_sample->is_encrypted() == encrypted;
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

// A fake media handler definition used for testing.
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
  MediaHandlerTestBase();

  bool IsVideoCodec(Codec codec) const;

  std::unique_ptr<StreamInfo> GetVideoStreamInfo(
      uint32_t time_scale) const;

  std::unique_ptr<StreamInfo> GetVideoStreamInfo(
      uint32_t time_scale, uint32_t width, uint64_t height) const;

  std::unique_ptr<StreamInfo> GetVideoStreamInfo(
      uint32_t time_scale, Codec codec) const;

  std::unique_ptr<StreamInfo> GetVideoStreamInfo(
      uint32_t time_scale,
      Codec codec,
      uint32_t width,
      uint64_t height) const;

  std::unique_ptr<StreamInfo> GetAudioStreamInfo(
      uint32_t time_scale) const;

  std::unique_ptr<StreamInfo> GetAudioStreamInfo(
      uint32_t time_scale,
      Codec codec) const;

  std::unique_ptr<MediaSample> GetMediaSample(
      int64_t timestamp,
      int64_t duration,
      bool is_keyframe) const;

  std::unique_ptr<MediaSample> GetMediaSample(
      int64_t timestamp,
      int64_t duration,
      bool is_keyframe,
      const uint8_t* data,
      size_t data_length) const;

  std::unique_ptr<SegmentInfo> GetSegmentInfo(
      int64_t start_timestamp,
      int64_t duration,
      bool is_subsegment) const;

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
  MediaHandlerTestBase(const MediaHandlerTestBase&) = delete;
  MediaHandlerTestBase& operator=(const MediaHandlerTestBase&) = delete;

  // Downstream handler used in testing graph.
  std::shared_ptr<FakeMediaHandler> next_handler_;
  // Some random handler which can be used for testing.
  std::shared_ptr<MediaHandler> some_handler_;
};

}  // namespace media
}  // namespace shaka
