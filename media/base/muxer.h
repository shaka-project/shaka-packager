// Copyright (c) 2013 Google Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Defines the muxer interface.

#ifndef MEDIA_BASE_MUXER_H_
#define MEDIA_BASE_MUXER_H_

#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "media/base/muxer_options.h"
#include "media/base/status.h"

namespace media {

class EncryptorSource;
class MediaSample;
class MediaStream;

class Muxer {
 public:
  Muxer(const MuxerOptions& options, EncryptorSource* encryptor_source);
  virtual ~Muxer();

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

  const std::vector<MediaStream*>& streams() const { return streams_; }

 protected:
  const MuxerOptions& options() const { return options_; }
  EncryptorSource* encryptor_source() { return encryptor_source_; }

 private:
  MuxerOptions options_;
  std::vector<MediaStream*> streams_;
  EncryptorSource* const encryptor_source_;

  DISALLOW_COPY_AND_ASSIGN(Muxer);
};

}  // namespace media

#endif  // MEDIA_BASE_MUXER_H_
