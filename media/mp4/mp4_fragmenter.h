// Copyright (c) 2013 Google Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// MP4Fragmenter is responsible for the generation of MP4 fragments, i.e. traf
// and the corresponding mdat. The samples are also encrypted if encryption is
// requested.

#ifndef MEDIA_MP4_MP4_FRAGMENTER_H_
#define MEDIA_MP4_MP4_FRAGMENTER_H_

#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "media/base/status.h"

namespace media {

class AesCtrEncryptor;
class BufferWriter;
class MediaSample;

namespace mp4 {

class SegmentReference;
class TrackFragment;

class MP4Fragmenter {
 public:
  // Caller retains the ownership of |traf| and transfers ownership of
  // |encryptor|. |clear_time| specifies clear time in the current track
  // timescale. |nalu_length_size| specifies NAL unit length size, for
  // subsample encryption.
  MP4Fragmenter(TrackFragment* traf,
                scoped_ptr<AesCtrEncryptor> encryptor,
                int64 clear_time,
                uint8 nalu_length_size);
  ~MP4Fragmenter();

  virtual Status AddSample(scoped_refptr<MediaSample> sample);

  // Initialize the fragment with default data.
  void InitializeFragment();

  // Finalize and optimize the fragment.
  void FinalizeFragment();

  // Fill in |reference| with current fragment information.
  void GenerateSegmentReference(SegmentReference* reference);

  uint64 fragment_duration() const { return fragment_duration_; }
  uint64 first_sap_time() const { return first_sap_time_; }
  uint64 earliest_presentation_time() const {
    return earliest_presentation_time_;
  }
  bool fragment_finalized() const { return fragment_finalized_; }
  BufferWriter* data() { return data_.get(); }
  BufferWriter* aux_data() { return aux_data_.get(); }

 private:
  void EncryptBytes(uint8* data, uint32 size);
  Status EncryptSample(scoped_refptr<MediaSample> sample);

  // Should we enable encrytion for the current fragment?
  bool ShouldEncryptFragment() {
    return (encryptor_ != NULL && clear_time_ <= 0);
  }

  // Should we enable subsample encryption?
  bool IsSubsampleEncryptionRequired() { return nalu_length_size_ != 0; }

  // Check if the current fragment starts with SAP.
  bool StartsWithSAP();

  scoped_ptr<AesCtrEncryptor> encryptor_;
  // If this stream contains AVC, subsample encryption specifies that the size
  // and type of NAL units remain unencrypted. This field specifies the size of
  // the size field. Can be 1, 2 or 4 bytes.
  uint8 nalu_length_size_;
  TrackFragment* traf_;
  bool fragment_finalized_;
  uint64 fragment_duration_;
  uint64 earliest_presentation_time_;
  uint64 first_sap_time_;
  int64 clear_time_;
  scoped_ptr<BufferWriter> data_;
  scoped_ptr<BufferWriter> aux_data_;

  DISALLOW_COPY_AND_ASSIGN(MP4Fragmenter);
};

}  // namespace mp4
}  // namespace media

#endif  // MEDIA_MP4_MP4_FRAGMENTER_H_
