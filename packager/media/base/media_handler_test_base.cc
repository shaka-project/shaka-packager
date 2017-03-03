// Copyright 2017 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/base/media_handler_test_base.h"

#include "packager/media/base/audio_stream_info.h"
#include "packager/media/base/test/status_test_util.h"
#include "packager/media/base/video_stream_info.h"

namespace {

const int kTrackId = 1;
const uint64_t kDuration = 10000;
const char kCodecString[] = "codec string";
const uint8_t kSampleBits = 1;
const uint8_t kNumChannels = 2;
const uint32_t kSamplingFrequency = 48000;
const uint64_t kSeekPrerollNs = 12345;
const uint64_t kCodecDelayNs = 56789;
const uint32_t kMaxBitrate = 13579;
const uint32_t kAvgBitrate = 13000;
const char kLanguage[] = "eng";
const uint16_t kWidth = 10u;
const uint16_t kHeight = 20u;
const uint32_t kPixelWidth = 2u;
const uint32_t kPixelHeight = 3u;
const int16_t kTrickPlayRate = 4;
const uint8_t kNaluLengthSize = 1u;
const bool kEncrypted = true;

// Use H264 code config.
const uint8_t kCodecConfig[]{
    // Header
    0x01, 0x64, 0x00, 0x1e, 0xff,
    // SPS count (ignore top three bits)
    0xe1,
    // SPS
    0x00, 0x19,  // Size
    0x67, 0x64, 0x00, 0x1e, 0xac, 0xd9, 0x40, 0xa0, 0x2f, 0xf9, 0x70, 0x11,
    0x00, 0x00, 0x03, 0x03, 0xe9, 0x00, 0x00, 0xea, 0x60, 0x0f, 0x16, 0x2d,
    0x96,
    // PPS count
    0x01,
    // PPS
    0x00, 0x06,  // Size
    0x68, 0xeb, 0xe3, 0xcb, 0x22, 0xc0,
};

// Mock data, we don't really care about what is inside.
const uint8_t kData[]{
    0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
};

}  // namespace

namespace shaka {
namespace media {

// A fake media handler definition used for testing.
class FakeMediaHandler : public MediaHandler {
 public:
  const std::vector<std::unique_ptr<StreamData>>& stream_data_vector() const {
    return stream_data_vector_;
  }
  void clear_stream_data_vector() { stream_data_vector_.clear(); }

 protected:
  Status InitializeInternal() override { return Status::OK; }
  Status Process(std::unique_ptr<StreamData> stream_data) override {
    stream_data_vector_.push_back(std::move(stream_data));
    return Status::OK;
  }
  Status OnFlushRequest(size_t input_stream_index) override { return Status::OK; }
  bool ValidateOutputStreamIndex(size_t stream_index) const override {
    return true;
  }

  std::vector<std::unique_ptr<StreamData>> stream_data_vector_;
};

MediaHandlerTestBase::MediaHandlerTestBase()
    : next_handler_(new FakeMediaHandler),
      some_handler_(new FakeMediaHandler) {}

std::unique_ptr<StreamData> MediaHandlerTestBase::GetStreamInfoStreamData(
    int stream_index,
    Codec codec,
    uint32_t time_scale) {
  std::unique_ptr<StreamData> stream_data(new StreamData);
  stream_data->stream_index = stream_index;
  stream_data->stream_data_type = StreamDataType::kStreamInfo;
  stream_data->stream_info = GetMockStreamInfo(codec, time_scale);
  return stream_data;
}

std::unique_ptr<StreamData> MediaHandlerTestBase::GetMediaSampleStreamData(
    int stream_index,
    int64_t timestamp,
    int64_t duration,
    bool is_keyframe) {
  std::unique_ptr<StreamData> stream_data(new StreamData);
  stream_data->stream_index = stream_index;
  stream_data->stream_data_type = StreamDataType::kMediaSample;
  stream_data->media_sample.reset(
      new MediaSample(kData, sizeof(kData), nullptr, 0, is_keyframe));
  stream_data->media_sample->set_dts(timestamp);
  stream_data->media_sample->set_duration(duration);
  return stream_data;
}

void MediaHandlerTestBase::SetUpGraph(int num_inputs,
                                      int num_outputs,
                                      std::shared_ptr<MediaHandler> handler) {
  // Input handler is not really used anywhere but just to satisfy one input
  // one output restriction for the encryption handler.
  auto input_handler = std::make_shared<FakeMediaHandler>();
  for (int i = 0; i < num_inputs; ++i)
    ASSERT_OK(input_handler->SetHandler(i, handler));
  // All outputs are routed to |next_handler_|.
  for (int i = 0; i < num_outputs; ++i)
    ASSERT_OK(handler->SetHandler(i, next_handler_));
}

const std::vector<std::unique_ptr<StreamData>>&
MediaHandlerTestBase::GetOutputStreamDataVector() const {
  return next_handler_->stream_data_vector();
}

void MediaHandlerTestBase::ClearOutputStreamDataVector() {
  next_handler_->clear_stream_data_vector();
}

std::shared_ptr<StreamInfo> MediaHandlerTestBase::GetMockStreamInfo(
    Codec codec,
    uint32_t time_scale) {
  if (codec >= kCodecAudio && codec < kCodecAudioMaxPlusOne) {
    return std::shared_ptr<StreamInfo>(new AudioStreamInfo(
        kTrackId, time_scale, kDuration, codec, kCodecString, kCodecConfig,
        sizeof(kCodecConfig), kSampleBits, kNumChannels, kSamplingFrequency,
        kSeekPrerollNs, kCodecDelayNs, kMaxBitrate, kAvgBitrate, kLanguage,
        !kEncrypted));
  } else if (codec >= kCodecVideo && codec < kCodecVideoMaxPlusOne) {
    return std::shared_ptr<StreamInfo>(new VideoStreamInfo(
        kTrackId, time_scale, kDuration, codec, kCodecString, kCodecConfig,
        sizeof(kCodecConfig), kWidth, kHeight, kPixelWidth, kPixelHeight,
        kTrickPlayRate, kNaluLengthSize, kLanguage, !kEncrypted));
  }
  return nullptr;
}

}  // namespace media
}  // namespace shaka
