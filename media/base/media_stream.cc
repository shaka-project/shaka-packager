// Copyright (c) 2013 Google Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/media_stream.h"

#include "base/logging.h"
#include "media/base/demuxer.h"
#include "media/base/media_sample.h"
#include "media/base/muxer.h"
#include "media/base/stream_info.h"

namespace media {

MediaStream::MediaStream(scoped_refptr<StreamInfo> info, Demuxer* demuxer)
    : info_(info), demuxer_(demuxer), muxer_(NULL), state_(kIdle) {}

MediaStream::~MediaStream() {}

Status MediaStream::PullSample(scoped_refptr<MediaSample>* sample) {
  DCHECK_EQ(state_, kPulling);

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

Status MediaStream::PushSample(const scoped_refptr<MediaSample>& sample) {
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
  DCHECK(muxer != NULL);
  DCHECK(muxer_ == NULL);
  state_ = kConnected;
  muxer_ = muxer;
}

Status MediaStream::Start(MediaStreamOperation operation) {
  DCHECK(demuxer_ != NULL);
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
        for (int i = 0; i < demuxer_->num_streams(); ++i) {
          Status status = demuxer_->streams(i)->Start(operation);
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

scoped_refptr<StreamInfo> MediaStream::info() {
  return info_;
}

std::string MediaStream::ToString() const {
  std::ostringstream s;
  s << "state: " << state_
    << " samples in the queue: " << samples_.size()
    << " " << info_->ToString();
  return s.str();
}

}  // namespace media
