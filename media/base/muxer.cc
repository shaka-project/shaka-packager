// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "media/base/muxer.h"

#include "media/base/encryptor_source.h"
#include "media/base/media_sample.h"
#include "media/base/media_stream.h"

namespace media {

Muxer::Muxer(const MuxerOptions& options)
    : options_(options),
      encryptor_source_(NULL),
      clear_lead_in_seconds_(0),
      muxer_listener_(NULL) {}

Muxer::~Muxer() {}

void Muxer::SetEncryptorSource(EncryptorSource* encryptor_source,
                               double clear_lead_in_seconds) {
  encryptor_source_ = encryptor_source;
  clear_lead_in_seconds_ = clear_lead_in_seconds;
}

Status Muxer::AddStream(MediaStream* stream) {
  stream->Connect(this);
  streams_.push_back(stream);
  return Status::OK;
}

Status Muxer::Run() {
  DCHECK(!streams_.empty());

  Status status;
  // Start the streams.
  for (std::vector<MediaStream*>::iterator it = streams_.begin();
       it != streams_.end();
       ++it) {
    status = (*it)->Start(MediaStream::kPull);
    if (!status.ok())
      return status;
  }

  uint32 current_stream_id = 0;
  while (status.ok()) {
    scoped_refptr<MediaSample> sample;
    status = streams_[current_stream_id]->PullSample(&sample);
    if (!status.ok())
      break;
    status = AddSample(streams_[current_stream_id], sample);

    // Switch to next stream if the current stream is ready for fragmentation.
    if (status.Matches(Status(error::FRAGMENT_FINALIZED, ""))) {
      current_stream_id = (current_stream_id + 1) % streams_.size();
      status.Clear();
    }
  }
  return status.Matches(Status(error::END_OF_STREAM, "")) ? Status::OK : status;
}

void Muxer::SetMuxerListener(media::event::MuxerListener* muxer_listener) {
  muxer_listener_ = muxer_listener;
}

}  // namespace media
