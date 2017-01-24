// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef MEDIA_FORMATS_MP4_ENCRYPTING_FRAGMENTER_H_
#define MEDIA_FORMATS_MP4_ENCRYPTING_FRAGMENTER_H_

#include <memory>
#include "packager/media/base/fourccs.h"
#include "packager/media/codecs/video_slice_header_parser.h"
#include "packager/media/codecs/vpx_parser.h"
#include "packager/media/event/muxer_listener.h"
#include "packager/media/formats/mp4/fragmenter.h"

namespace shaka {
namespace media {

class AesCryptor;
class StreamInfo;
struct EncryptionKey;

namespace mp4 {

/// EncryptingFragmenter generates MP4 fragments with sample encrypted.
class EncryptingFragmenter : public Fragmenter {
 public:
  /// @param info contains stream information.
  /// @param traf points to a TrackFragment box.
  /// @param encryption_key contains the encryption parameters.
  /// @param clear_time specifies clear lead duration in units of the current
  ///        track's timescale.
  /// @param protection_scheme specifies the protection scheme: 'cenc', 'cens',
  ///        'cbc1', 'cbcs'.
  /// @param crypt_byte_block indicates number of encrypted blocks (16-byte) in
  ///        pattern based encryption.
  /// @param skip_byte_block indicates number of unencrypted blocks (16-byte)
  ///        in pattern based encryption.
  EncryptingFragmenter(std::shared_ptr<StreamInfo> info,
                       TrackFragment* traf,
                       std::unique_ptr<EncryptionKey> encryption_key,
                       int64_t clear_time,
                       FourCC protection_scheme,
                       uint8_t crypt_byte_block,
                       uint8_t skip_byte_block,
                       MuxerListener* listener);

  ~EncryptingFragmenter() override;

  /// @name Fragmenter implementation overrides.
  /// @{
  Status AddSample(std::shared_ptr<MediaSample> sample) override;
  Status InitializeFragment(int64_t first_sample_dts) override;
  void FinalizeFragment() override;
  /// @}

 protected:
  /// Prepare current fragment for encryption.
  /// @return OK on success, an error status otherwise.
  virtual Status PrepareFragmentForEncryption(bool enable_encryption);
  /// Finalize current fragment for encryption.
  virtual void FinalizeFragmentForEncryption();

  /// Create the encryptor for the internal encryption key. The existing
  /// encryptor will be reset if it is not NULL.
  /// @return OK on success, an error status otherwise.
  Status CreateEncryptor();

  const EncryptionKey* encryption_key() const { return encryption_key_.get(); }
  AesCryptor* encryptor() { return encryptor_.get(); }
  FourCC protection_scheme() const { return protection_scheme_; }
  uint8_t crypt_byte_block() const { return crypt_byte_block_; }
  uint8_t skip_byte_block() const { return skip_byte_block_; }

  void set_encryption_key(std::unique_ptr<EncryptionKey> encryption_key) {
    encryption_key_ = std::move(encryption_key);
  }

 private:
  void EncryptBytes(uint8_t* data, size_t size);
  Status EncryptSample(std::shared_ptr<MediaSample> sample);

  // Should we enable subsample encryption?
  bool IsSubsampleEncryptionRequired();

  std::shared_ptr<StreamInfo> info_;
  std::unique_ptr<EncryptionKey> encryption_key_;
  std::unique_ptr<AesCryptor> encryptor_;
  // If this stream contains AVC, subsample encryption specifies that the size
  // and type of NAL units remain unencrypted. This function returns the size of
  // the size field in bytes. Can be 1, 2 or 4 bytes.
  const uint8_t nalu_length_size_;
  const Codec video_codec_;
  int64_t clear_time_;
  const FourCC protection_scheme_;
  const uint8_t crypt_byte_block_;
  const uint8_t skip_byte_block_;
  MuxerListener* listener_;

  std::unique_ptr<VPxParser> vpx_parser_;
  std::unique_ptr<VideoSliceHeaderParser> header_parser_;

  DISALLOW_COPY_AND_ASSIGN(EncryptingFragmenter);
};

}  // namespace mp4
}  // namespace media
}  // namespace shaka

#endif  // MEDIA_FORMATS_MP4_ENCRYPTING_FRAGMENTER_H_
