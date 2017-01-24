// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/base/media_stream.h"

#include "packager/base/logging.h"
#include "packager/base/strings/stringprintf.h"
#include "packager/media/base/demuxer.h"
#include "packager/media/base/media_sample.h"
#include "packager/media/base/muxer.h"
#include "packager/media/base/stream_info.h"

namespace shaka {
namespace media {

MediaStream::MediaStream(std::shared_ptr<StreamInfo> info, Demuxer* demuxer)
    : info_(info), demuxer_(demuxer), muxer_(NULL), state_(kIdle) {}

MediaStream::~MediaStream() {}

Status MediaStream::PullSample(std::shared_ptr<MediaSample>* sample) {
  DCHECK(state_ == kPulling || state_ == kIdle);

  // Trigger a new parse in demuxer if no more samples.
  while (samples_.empty()) {
    Status status = demuxer_->Parse();
    if (!status.ok())
      return status;
  }

  *sample = samples_.front();
  samples_.pop_front();
  return Status::OK;
}

Status MediaStream::PushSample(const std::shared_ptr<MediaSample>& sample) {
  switch (state_) {
    case kIdle:
    case kPulling:
      samples_.push_back(sample);
      return Status::OK;
    case kDisconnected:
      return Status::OK;
    case kPushing:
      return muxer_->AddSample(this, sample);
    default:
      NOTREACHED() << "Unexpected State " << state_;
      return Status::UNKNOWN;
  }
}

void MediaStream::Connect(Muxer* muxer) {
  DCHECK(muxer);
  DCHECK(!muxer_);
  state_ = kConnected;
  muxer_ = muxer;
}

Status MediaStream::Start(MediaStreamOperation operation) {
  DCHECK(demuxer_);
  DCHECK(operation == kPush || operation == kPull);

  switch (state_) {
    case kIdle:
      // Disconnect the stream if it is not connected to a muxer.
      state_ = kDisconnected;
      samples_.clear();
      return Status::OK;
    case kConnected:
      state_ = (operation == kPush) ? kPushing : kPulling;
      if (operation == kPush) {
        // Push samples in the queue to muxer if there is any.
        while (!samples_.empty()) {
          Status status = muxer_->AddSample(this, samples_.front());
          if (!status.ok())
            return status;
          samples_.pop_front();
        }
      } else {
        // We need to disconnect all its peer streams which are not connected
        // to a muxer.
        for (size_t i = 0; i < demuxer_->streams().size(); ++i) {
          Status status = demuxer_->streams()[i]->Start(operation);
          if (!status.ok())
            return status;
        }
      }
      return Status::OK;
    case kPulling:
      DCHECK(operation == kPull);
      return Status::OK;
    default:
      NOTREACHED() << "Unexpected State " << state_;
      return Status::UNKNOWN;
  }
}

const std::shared_ptr<StreamInfo> MediaStream::info() const {
  return info_;
}

std::string MediaStream::ToString() const {
  return base::StringPrintf("state: %d\n samples in the queue: %zu\n %s",
                            state_, samples_.size(), info_->ToString().c_str());
}

}  // namespace media
}  // namespace shaka
