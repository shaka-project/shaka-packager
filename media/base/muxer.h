// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd
//
// Defines the muxer interface.

#ifndef MEDIA_BASE_MUXER_H_
#define MEDIA_BASE_MUXER_H_

#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "media/base/muxer_options.h"
#include "media/base/status.h"

namespace base {
class Clock;
}

namespace media {

class EncryptorSource;
class MediaSample;
class MediaStream;

namespace event {
class MuxerListener;
}

class Muxer {
 public:
  explicit Muxer(const MuxerOptions& options);
  virtual ~Muxer();

  // Set encryptor source. Caller retains ownership of |encryptor_source|.
  // Should be called before calling Initialize().
  void SetEncryptorSource(EncryptorSource* encryptor_source,
                          double clear_lead_in_seconds);

  // Initialize the muxer. Must be called after connecting all the streams.
  virtual Status Initialize() = 0;

  // Final clean up.
  virtual Status Finalize() = 0;

  // Adds video/audio stream.
  virtual Status AddStream(MediaStream* stream);

  // Adds new media sample.
  virtual Status AddSample(const MediaStream* stream,
                           scoped_refptr<MediaSample> sample) = 0;

  // Drives the remuxing from muxer side (pull).
  virtual Status Run();

  // Set a MuxerListener event handler for this object. Ownership does not
  // transfer.
  void SetMuxerListener(event::MuxerListener* muxer_listener);

  const std::vector<MediaStream*>& streams() const { return streams_; }

  // Inject clock, mainly used for testing.
  // The injected clock will be used to generate the creation time-stamp and
  // modification time-stamp of the muxer output.
  // If no clock is injected, the code uses base::Time::Now() to generate the
  // time-stamps.
  void set_clock(base::Clock* clock) {
    clock_ = clock;
  }

 protected:
  const MuxerOptions& options() const { return options_; }
  EncryptorSource* encryptor_source() { return encryptor_source_; }
  double clear_lead_in_seconds() const { return clear_lead_in_seconds_; }
  event::MuxerListener* muxer_listener() { return muxer_listener_; }
  base::Clock* clock() { return clock_; }

 private:
  MuxerOptions options_;
  std::vector<MediaStream*> streams_;
  EncryptorSource* encryptor_source_;
  double clear_lead_in_seconds_;

  event::MuxerListener* muxer_listener_;
  // An external injected clock, can be NULL.
  base::Clock* clock_;

  DISALLOW_COPY_AND_ASSIGN(Muxer);
};

}  // namespace media

#endif  // MEDIA_BASE_MUXER_H_
