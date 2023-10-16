// Copyright 2020 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/media/formats/ttml/ttml_to_mp4_handler.h>

#include <absl/log/check.h>

#include <packager/macros/status.h>

namespace shaka {
namespace media {
namespace ttml {

namespace {

size_t kTrackId = 0;

std::shared_ptr<MediaSample> CreateMediaSample(const std::string& data,
                                               int64_t start_time,
                                               int64_t duration) {
  DCHECK_GE(start_time, 0);
  DCHECK_GT(duration, 0);

  const bool kIsKeyFrame = true;

  std::shared_ptr<MediaSample> sample = MediaSample::CopyFrom(
      reinterpret_cast<const uint8_t*>(data.data()), data.size(), kIsKeyFrame);
  sample->set_pts(start_time);
  sample->set_dts(start_time);
  sample->set_duration(duration);

  return sample;
}

}  // namespace

Status TtmlToMp4Handler::InitializeInternal() {
  return Status::OK;
}

Status TtmlToMp4Handler::Process(std::unique_ptr<StreamData> stream_data) {
  switch (stream_data->stream_data_type) {
    case StreamDataType::kStreamInfo:
      return OnStreamInfo(std::move(stream_data));
    case StreamDataType::kCueEvent:
      return OnCueEvent(std::move(stream_data));
    case StreamDataType::kSegmentInfo:
      return OnSegmentInfo(std::move(stream_data));
    case StreamDataType::kTextSample:
      return OnTextSample(std::move(stream_data));
    default:
      return Status(error::INTERNAL_ERROR,
                    "Invalid stream data type (" +
                        StreamDataTypeToString(stream_data->stream_data_type) +
                        ") for this TtmlToMp4 handler");
  }
}

Status TtmlToMp4Handler::OnStreamInfo(std::unique_ptr<StreamData> stream_data) {
  DCHECK(stream_data);
  DCHECK(stream_data->stream_info);

  auto clone = stream_data->stream_info->Clone();
  clone->set_codec(kCodecTtml);
  clone->set_codec_string("ttml");

  if (clone->stream_type() != kStreamText)
    return Status(error::MUXER_FAILURE, "Incorrect stream type");
  auto* text_stream = static_cast<const TextStreamInfo*>(clone.get());
  generator_.Initialize(text_stream->regions(), text_stream->language(),
                        text_stream->time_scale());

  return Dispatch(
      StreamData::FromStreamInfo(stream_data->stream_index, std::move(clone)));
}

Status TtmlToMp4Handler::OnCueEvent(std::unique_ptr<StreamData> stream_data) {
  DCHECK(stream_data);
  DCHECK(stream_data->cue_event);
  return Dispatch(std::move(stream_data));
}

Status TtmlToMp4Handler::OnSegmentInfo(
    std::unique_ptr<StreamData> stream_data) {
  DCHECK(stream_data);
  DCHECK(stream_data->segment_info);

  const auto& segment = stream_data->segment_info;

  std::string data;
  if (!generator_.Dump(&data))
    return Status(error::INTERNAL_ERROR, "Error generating XML");
  generator_.Reset();

  RETURN_IF_ERROR(DispatchMediaSample(
      kTrackId,
      CreateMediaSample(data, segment->start_timestamp, segment->duration)));

  return Dispatch(std::move(stream_data));
}

Status TtmlToMp4Handler::OnTextSample(std::unique_ptr<StreamData> stream_data) {
  DCHECK(stream_data);
  DCHECK(stream_data->text_sample);

  auto& sample = stream_data->text_sample;

  // Ignore empty samples. This will create gaps, but we will handle that
  // later.
  if (sample->body().is_empty()) {
    return Status::OK;
  }

  // Add the new text sample to the cache of samples that belong in the
  // current segment.
  generator_.AddSample(*sample);
  return Status::OK;
}

}  // namespace ttml
}  // namespace media
}  // namespace shaka
