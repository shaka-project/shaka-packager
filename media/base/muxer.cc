// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "media/base/muxer.h"

#include "media/base/media_sample.h"
#include "media/base/media_stream.h"

namespace media {

Muxer::Muxer(const MuxerOptions& options)
    : options_(options),
      initialized_(false),
      encryption_key_source_(NULL),
      max_sd_pixels_(0),
      clear_lead_in_seconds_(0),
      crypto_period_duration_in_seconds_(0),
      muxer_listener_(NULL),
      clock_(NULL) {}

Muxer::~Muxer() {}

void Muxer::SetEncryptionKeySource(EncryptionKeySource* encryption_key_source,
                                   uint32 max_sd_pixels,
                                   double clear_lead_in_seconds,
                                   double crypto_period_duration_in_seconds) {
  DCHECK(encryption_key_source);
  encryption_key_source_ = encryption_key_source;
  max_sd_pixels_ = max_sd_pixels;
  clear_lead_in_seconds_ = clear_lead_in_seconds;
  crypto_period_duration_in_seconds_ = crypto_period_duration_in_seconds;
}

void Muxer::AddStream(MediaStream* stream) {
  DCHECK(stream);
  stream->Connect(this);
  streams_.push_back(stream);
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
    if (status.error_code() == error::FRAGMENT_FINALIZED) {
      current_stream_id = (current_stream_id + 1) % streams_.size();
      status.Clear();
    }
  }
  // Finalize the muxer after reaching end of stream.
  return status.error_code() == error::END_OF_STREAM ? Finalize() : status;
}

void Muxer::SetMuxerListener(media::event::MuxerListener* muxer_listener) {
  muxer_listener_ = muxer_listener;
}

Status Muxer::AddSample(const MediaStream* stream,
                        scoped_refptr<MediaSample> sample) {
  DCHECK(std::find(streams_.begin(), streams_.end(), stream) != streams_.end());

  if (!initialized_) {
    Status status = Initialize();
    if (!status.ok())
      return status;
    initialized_ = true;
  }
  if (sample->end_of_stream()) {
    // EOS sample should be sent only when the sample was pushed from Demuxer
    // to Muxer. In this case, there should be only one stream in Muxer.
    DCHECK_EQ(1u, streams_.size());
    return Finalize();
  }
  return DoAddSample(stream, sample);
}

}  // namespace media
