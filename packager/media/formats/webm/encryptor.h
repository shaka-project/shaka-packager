// Copyright 2015 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef MEDIA_FORMATS_WEBM_ENCRYPTOR_H_
#define MEDIA_FORMATS_WEBM_ENCRYPTOR_H_

#include "packager/base/memory/ref_counted.h"
#include "packager/base/memory/scoped_ptr.h"
#include "packager/media/base/key_source.h"
#include "packager/media/base/status.h"
#include "packager/media/event/muxer_listener.h"
#include "packager/third_party/libwebm/src/mkvmuxer.hpp"

namespace edash_packager {
namespace media {

class AesCtrEncryptor;
class MediaSample;

namespace webm {

/// A helper class used to encrypt WebM frames before being written to the
/// Cluster.  This can also handle unencrypted frames.
class Encryptor {
 public:
  Encryptor();
  ~Encryptor();

  /// Initializes the encryptor with the given key source.
  /// @return OK on success, an error status otherwise.
  Status Initialize(MuxerListener* muxer_listener,
                    KeySource::TrackType track_type,
                    KeySource* key_source);

  /// Adds the encryption info to the given track.  Initialize must be called
  /// first.
  /// @return OK on success, an error status otherwise.
  Status AddTrackInfo(mkvmuxer::Track* track);

  /// Encrypt the data.  This needs to be told whether the current frame will
  /// be encrypted (e.g. for a clear lead).
  /// @return OK on success, an error status otherwise.
  Status EncryptFrame(scoped_refptr<MediaSample> sample,
                      bool encrypt_frame);

 private:
  // Create the encryptor for the internal encryption key.
  Status CreateEncryptor(MuxerListener* muxer_listener,
                         KeySource::TrackType track_type,
                         KeySource* key_source);

 private:
  scoped_ptr<EncryptionKey> key_;
  scoped_ptr<AesCtrEncryptor> encryptor_;
};

}  // namespace webm
}  // namespace media
}  // namespace edash_packager

#endif  // MEDIA_FORMATS_WEBM_ENCRYPTOR_H_
