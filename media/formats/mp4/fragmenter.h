// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef MEDIA_FORMATS_MP4_FRAGMENTER_H_
#define MEDIA_FORMATS_MP4_FRAGMENTER_H_

#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "media/base/status.h"

namespace media {

class AesCtrEncryptor;
class BufferWriter;
class MediaSample;

struct EncryptionKey;

namespace mp4 {

struct MovieFragment;
struct SegmentReference;
struct TrackFragment;

/// Fragmenter is responsible for the generation of MP4 fragments, i.e. traf
/// box and corresponding mdat box. The samples are also encrypted if encryption
/// is requested.
class Fragmenter {
 public:
  /// @param traf points to a TrackFragment box.
  /// @param normalize_presentation_timestamp defines whether PTS should be
  ///        normalized to start from zero.
  Fragmenter(TrackFragment* traf,
             bool normalize_presentation_timestamp);

  /// @param traf points to a TrackFragment box.
  /// @param normalize_presentation_timestamp defines whether PTS should be
  ///        normalized to start from zero.
  /// @param encryption_key contains the encryption parameters.
  /// @param clear_time specifies clear lead duration in units of the current
  ///        track's timescale.
  /// @param nalu_length_size NAL unit length size, in bytes, for subsample
  ///        encryption.
  Fragmenter(TrackFragment* traf,
             bool normalize_presentation_timestamp,
             scoped_ptr<EncryptionKey> encryption_key,
             int64 clear_time,
             uint8 nalu_length_size);

  virtual ~Fragmenter();

  /// Add a sample to the fragmenter.
  Status AddSample(scoped_refptr<MediaSample> sample);

  /// Initialize the fragment with default data.
  /// @return OK on success, an error status otherwise.
  Status InitializeFragment();

  /// Finalize and optimize the fragment.
  void FinalizeFragment();

  /// Fill @a reference with current fragment information.
  void GenerateSegmentReference(SegmentReference* reference);

  uint64 fragment_duration() const { return fragment_duration_; }
  uint64 first_sap_time() const { return first_sap_time_; }
  uint64 earliest_presentation_time() const {
    return earliest_presentation_time_;
  }
  bool fragment_finalized() const { return fragment_finalized_; }
  BufferWriter* data() { return data_.get(); }
  BufferWriter* aux_data() { return aux_data_.get(); }

 protected:
  /// Prepare current fragment for encryption.
  /// @return OK on success, an error status otherwise.
  virtual Status PrepareFragmentForEncryption();
  /// Finalize current fragment for encryption.
  virtual void FinalizeFragmentForEncryption();

  /// Create the encryptor for the internal encryption key. The existing
  /// encryptor will be reset if it is not NULL.
  /// @return OK on success, an error status otherwise.
  Status CreateEncryptor();

  TrackFragment* traf() { return traf_; }
  EncryptionKey* encryption_key() { return encryption_key_.get(); }
  AesCtrEncryptor* encryptor() { return encryptor_.get(); }

  void set_encryption_key(scoped_ptr<EncryptionKey> encryption_key) {
    encryption_key_ = encryption_key.Pass();
  }

 private:
  void EncryptBytes(uint8* data, uint32 size);
  Status EncryptSample(scoped_refptr<MediaSample> sample);

  // Should we enable subsample encryption?
  bool IsSubsampleEncryptionRequired() { return nalu_length_size_ != 0; }

  // Check if the current fragment starts with SAP.
  bool StartsWithSAP();

  TrackFragment* traf_;

  scoped_ptr<EncryptionKey> encryption_key_;
  scoped_ptr<AesCtrEncryptor> encryptor_;
  // If this stream contains AVC, subsample encryption specifies that the size
  // and type of NAL units remain unencrypted. This field specifies the size of
  // the size field. Can be 1, 2 or 4 bytes.
  const uint8 nalu_length_size_;
  const int64 clear_time_;

  bool fragment_finalized_;
  uint64 fragment_duration_;
  bool normalize_presentation_timestamp_;
  int64 presentation_start_time_;
  int64 earliest_presentation_time_;
  int64 first_sap_time_;
  scoped_ptr<BufferWriter> data_;
  scoped_ptr<BufferWriter> aux_data_;

  DISALLOW_COPY_AND_ASSIGN(Fragmenter);
};

}  // namespace mp4
}  // namespace media

#endif  // MEDIA_FORMATS_MP4_FRAGMENTER_H_
