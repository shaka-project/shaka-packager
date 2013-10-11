// Copyright (c) 2013 Google Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/muxer.h"

#include "media/base/encryptor_source.h"
#include "media/base/media_sample.h"
#include "media/base/media_stream.h"

namespace media {

Muxer::Muxer(const Options& options, EncryptorSource* encrytor_source) :
  encryptor_source_(encrytor_source), encryptor_(NULL) {}

Muxer::~Muxer() {}

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

  while (status.ok()) {
    // TODO(kqyang): Need to do some proper synchronization between streams.
    scoped_refptr<MediaSample> sample;
    status = streams_[0]->PullSample(&sample);
    if (!status.ok())
      break;
    status = AddSample(streams_[0], sample);
  }
  return status.Matches(Status(error::EOF, "")) ? Status::OK : status;
}

}  // namespace media
