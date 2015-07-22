// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef MEDIA_FORMATS_MP4_KEY_ROTATION_FRAGMENTER_H_
#define MEDIA_FORMATS_MP4_KEY_ROTATION_FRAGMENTER_H_

#include "packager/media/base/key_source.h"
#include "packager/media/event/muxer_listener.h"
#include "packager/media/formats/mp4/encrypting_fragmenter.h"

namespace edash_packager {
namespace media {
namespace mp4 {

struct MovieFragment;

/// KeyRotationFragmenter generates MP4 fragments with sample encrypted by
/// rotation keys.
class KeyRotationFragmenter : public EncryptingFragmenter {
 public:
  /// @param moof points to a MovieFragment box.
  /// @param traf points to a TrackFragment box.
  /// @param encryption_key_source points to the source which generates
  ///        encryption keys.
  /// @param track_type indicates whether SD key or HD key should be used to
  ///        encrypt the video content.
  /// @param crypto_period_duration specifies crypto period duration in units
  ///        of the current track's timescale.
  /// @param clear_time specifies clear lead duration in units of the current
  ///        track's timescale.
  /// @param nalu_length_size NAL unit length size, in bytes, for subsample
  ///        encryption.
  /// @param muxer_listener is a pointer to MuxerListener for notifying
  ///        muxer related events. This may be null.
  KeyRotationFragmenter(MovieFragment* moof,
                        TrackFragment* traf,
                        KeySource* encryption_key_source,
                        KeySource::TrackType track_type,
                        int64_t crypto_period_duration,
                        int64_t clear_time,
                        uint8_t nalu_length_size,
                        MuxerListener* muxer_listener);
  ~KeyRotationFragmenter() override;

 protected:
  /// @name Fragmenter implementation overrides.
  /// @{
  Status PrepareFragmentForEncryption(bool enable_encryption) override;
  void FinalizeFragmentForEncryption() override;
  /// @}

 private:
  MovieFragment* moof_;

  KeySource* encryption_key_source_;
  KeySource::TrackType track_type_;
  const int64_t crypto_period_duration_;
  size_t prev_crypto_period_index_;

  // For notifying new pssh boxes to the event handler.
  MuxerListener* const muxer_listener_;

  DISALLOW_COPY_AND_ASSIGN(KeyRotationFragmenter);
};

}  // namespace mp4
}  // namespace media
}  // namespace edash_packager

#endif  // MEDIA_FORMATS_MP4_KEY_ROTATION_FRAGMENTER_H_
