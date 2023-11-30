// Copyright 2017 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/media/base/media_handler.h>

#include <packager/macros/status.h>

namespace shaka {
namespace media {

std::string StreamDataTypeToString(StreamDataType type) {
  switch (type) {
    case StreamDataType::kStreamInfo:
      return "stream info";
    case StreamDataType::kMediaSample:
      return "media sample";
    case StreamDataType::kTextSample:
      return "text sample";
    case StreamDataType::kSegmentInfo:
      return "segment info";
    case StreamDataType::kScte35Event:
      return "scte35 event";
    case StreamDataType::kCueEvent:
      return "cue event";
    case StreamDataType::kUnknown:
      return "unknown";
  }
  return "unknown";
}

Status MediaHandler::SetHandler(size_t output_stream_index,
                                std::shared_ptr<MediaHandler> handler) {
  if (output_handlers_.find(output_stream_index) != output_handlers_.end()) {
    return Status(error::ALREADY_EXISTS,
                  "The handler at the specified index already exists.");
  }
  output_handlers_[output_stream_index] =
      std::make_pair(handler, handler->num_input_streams_++);
  next_output_stream_index_ = output_stream_index + 1;
  return Status::OK;
}

Status MediaHandler::Initialize() {
  if (initialized_)
    return Status::OK;
  Status status = InitializeInternal();
  if (!status.ok())
    return status;
  for (auto& pair : output_handlers_) {
    if (!ValidateOutputStreamIndex(pair.first))
      return Status(error::INVALID_ARGUMENT, "Invalid output stream index");
    status = pair.second.first->Initialize();
    if (!status.ok())
      return status;
  }
  initialized_ = true;
  return Status::OK;
}

Status MediaHandler::Chain(
    const std::vector<std::shared_ptr<MediaHandler>>& list) {
  std::shared_ptr<MediaHandler> previous;

  for (const auto& next : list) {
    // Skip null entries.
    if (!next) {
      continue;
    }

    if (previous) {
      RETURN_IF_ERROR(previous->AddHandler(next));
    }

    previous = std::move(next);
  }

  return Status::OK;
}

Status MediaHandler::OnFlushRequest(size_t input_stream_index) {
  // The default implementation treats the output stream index to be identical
  // to the input stream index, which is true for most handlers.
  const size_t output_stream_index = input_stream_index;
  return FlushDownstream(output_stream_index);
}

bool MediaHandler::ValidateOutputStreamIndex(size_t stream_index) const {
  return stream_index < num_input_streams_;
}

Status MediaHandler::Dispatch(std::unique_ptr<StreamData> stream_data) const {
  size_t output_stream_index = stream_data->stream_index;
  auto handler_it = output_handlers_.find(output_stream_index);
  if (handler_it == output_handlers_.end()) {
    return Status(error::NOT_FOUND,
                  "No output handler exist at the specified index.");
  }
  stream_data->stream_index = handler_it->second.second;
  return handler_it->second.first->Process(std::move(stream_data));
}

Status MediaHandler::FlushDownstream(size_t output_stream_index) {
  auto handler_it = output_handlers_.find(output_stream_index);
  if (handler_it == output_handlers_.end()) {
    return Status(error::NOT_FOUND,
                  "No output handler exist at the specified index.");
  }
  return handler_it->second.first->OnFlushRequest(handler_it->second.second);
}

Status MediaHandler::FlushAllDownstreams() {
  for (const auto& pair : output_handlers_) {
    Status status = pair.second.first->OnFlushRequest(pair.second.second);
    if (!status.ok()) {
      return status;
    }
  }
  return Status::OK;
}
}  // namespace media
}  // namespace shaka
