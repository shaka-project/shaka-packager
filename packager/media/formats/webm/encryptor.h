// Copyright 2015 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef MEDIA_FORMATS_WEBM_ENCRYPTOR_H_
#define MEDIA_FORMATS_WEBM_ENCRYPTOR_H_

#include <memory>
#include "packager/base/macros.h"
#include "packager/base/memory/ref_counted.h"
#include "packager/media/base/key_source.h"
#include "packager/media/base/status.h"
#include "packager/media/base/stream_info.h"
#include "packager/media/codecs/vpx_parser.h"
#include "packager/media/event/muxer_listener.h"
#include "packager/third_party/libwebm/src/mkvmuxer.hpp"

namespace shaka {
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
                    Codec codec,
                    KeySource* key_source,
                    bool webm_subsample_encryption);

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
                         Codec codec,
                         KeySource* key_source,
                         bool webm_subsample_encryption);

 private:
  std::unique_ptr<EncryptionKey> key_;
  std::unique_ptr<AesCtrEncryptor> encryptor_;
  std::unique_ptr<VPxParser> vpx_parser_;

  DISALLOW_COPY_AND_ASSIGN(Encryptor);
};

}  // namespace webm
}  // namespace media
}  // namespace shaka

#endif  // MEDIA_FORMATS_WEBM_ENCRYPTOR_H_
