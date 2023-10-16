// Copyright 2017 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/media/replicator/replicator.h>

#include <absl/log/check.h>
#include <absl/log/log.h>

namespace shaka {
namespace media {

Status Replicator::InitializeInternal() {
  return Status::OK;
}

Status Replicator::Process(std::unique_ptr<StreamData> stream_data) {
  Status status;

  for (auto& out : output_handlers()) {
    std::unique_ptr<StreamData> copy(new StreamData(*stream_data));
    copy->stream_index = out.first;

    status.Update(Dispatch(std::move(copy)));
  }

  return status;
}

bool Replicator::ValidateOutputStreamIndex(size_t /* ignored */) const {
  return true;
}

Status Replicator::OnFlushRequest(size_t input_stream_index) {
  DCHECK_EQ(input_stream_index, 0u);
  return FlushAllDownstreams();
}

}  // namespace media
}  // namespace shaka
