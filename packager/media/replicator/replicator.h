// Copyright 2017 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_REPLICATOR_HANDLER_H_
#define PACKAGER_MEDIA_REPLICATOR_HANDLER_H_

#include "packager/media/base/media_handler.h"

namespace shaka {
namespace media {

/// The replicator takes a single input and send the messages to multiple
/// downstream handlers. The messages that are sent downstream are not copies,
/// they are the original message. It is the responsibility of downstream
/// handlers to make a copy before modifying the message.
class Replicator : public MediaHandler {
 private:
  Status InitializeInternal() override;
  Status Process(std::unique_ptr<StreamData> stream_data) override;
  bool ValidateOutputStreamIndex(size_t stream_index) const override;
  Status OnFlushRequest(size_t input_stream_index) override;
};

}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_REPLICATOR_HANDLER_H_
