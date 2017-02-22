// Copyright 2017 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/base/media_handler.h"

namespace shaka {
namespace media {

Status MediaHandler::SetHandler(int output_stream_index,
                                std::shared_ptr<MediaHandler> handler) {
  if (!ValidateOutputStreamIndex(output_stream_index))
    return Status(error::INVALID_ARGUMENT, "Invalid output stream index");
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
    status = pair.second.first->Initialize();
    if (!status.ok())
      return status;
  }
  initialized_ = true;
  return Status::OK;
}

Status MediaHandler::OnFlushRequest(int input_stream_index) {
  // The default implementation treats the output stream index to be identical
  // to the input stream index, which is true for most handlers.
  const int output_stream_index = input_stream_index;
  return FlushDownstream(output_stream_index);
}

bool MediaHandler::ValidateOutputStreamIndex(int stream_index) const {
  return stream_index >= 0 && stream_index < num_input_streams_;
}

Status MediaHandler::Dispatch(std::unique_ptr<StreamData> stream_data) {
  int output_stream_index = stream_data->stream_index;
  auto handler_it = output_handlers_.find(output_stream_index);
  if (handler_it == output_handlers_.end()) {
    return Status(error::NOT_FOUND,
                  "No output handler exist at the specified index.");
  }
  stream_data->stream_index = handler_it->second.second;
  return handler_it->second.first->Process(std::move(stream_data));
}

Status MediaHandler::FlushDownstream(int output_stream_index) {
  auto handler_it = output_handlers_.find(output_stream_index);
  if (handler_it == output_handlers_.end()) {
    return Status(error::NOT_FOUND,
                  "No output handler exist at the specified index.");
  }
  return handler_it->second.first->OnFlushRequest(handler_it->second.second);
}

}  // namespace media
}  // namespace shaka
