// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef MEDIA_FORMATS_MP4_MP4_MEDIA_PARSER_H_
#define MEDIA_FORMATS_MP4_MP4_MEDIA_PARSER_H_

#include <stdint.h>

#include <map>
#include <vector>

#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "media/base/media_parser.h"
#include "media/base/offset_byte_queue.h"

namespace edash_packager {
namespace media {

class AesCtrEncryptor;
class DecryptConfig;

namespace mp4 {

class BoxReader;
class TrackRunIterator;
struct Movie;
struct ProtectionSystemSpecificHeader;

class MP4MediaParser : public MediaParser {
 public:
  MP4MediaParser();
  virtual ~MP4MediaParser();

  /// @name MediaParser implementation overrides.
  /// @{
  virtual void Init(const InitCB& init_cb,
                    const NewSampleCB& new_sample_cb,
                    KeySource* decryption_key_source) OVERRIDE;
  virtual void Flush() OVERRIDE;
  virtual bool Parse(const uint8_t* buf, int size) OVERRIDE;
  /// @}

 private:
  enum State {
    kWaitingForInit,
    kParsingBoxes,
    kEmittingSamples,
    kError
  };

  bool ParseBox(bool* err);
  bool ParseMoov(mp4::BoxReader* reader);
  bool ParseMoof(mp4::BoxReader* reader);

  bool FetchKeysIfNecessary(
      const std::vector<ProtectionSystemSpecificHeader>& headers);

  bool DecryptSampleBuffer(const DecryptConfig* decrypt_config,
                           uint8_t* buffer,
                           size_t buffer_size);

  // To retain proper framing, each 'mdat' atom must be read; to limit memory
  // usage, the atom's data needs to be discarded incrementally as frames are
  // extracted from the stream. This function discards data from the stream up
  // to |offset|, updating the |mdat_tail_| value so that framing can be
  // retained after all 'mdat' information has been read.
  // Returns 'true' on success, 'false' if there was an error.
  bool ReadAndDiscardMDATsUntil(const int64_t offset);

  void ChangeState(State new_state);

  bool EmitConfigs();

  bool EnqueueSample(bool* err);

  void Reset();

  State state_;
  InitCB init_cb_;
  NewSampleCB new_sample_cb_;
  KeySource* decryption_key_source_;

  OffsetByteQueue queue_;

  // These two parameters are only valid in the |kEmittingSegments| state.
  //
  // |moof_head_| is the offset of the start of the most recently parsed moof
  // block. All byte offsets in sample information are relative to this offset,
  // as mandated by the Media Source spec.
  int64_t moof_head_;
  // |mdat_tail_| is the stream offset of the end of the current 'mdat' box.
  // Valid iff it is greater than the head of the queue.
  int64_t mdat_tail_;

  scoped_ptr<Movie> moov_;
  scoped_ptr<TrackRunIterator> runs_;

  typedef std::map<std::vector<uint8_t>, AesCtrEncryptor*> DecryptorMap;
  DecryptorMap decryptor_map_;

  DISALLOW_COPY_AND_ASSIGN(MP4MediaParser);
};

}  // namespace mp4
}  // namespace media
}  // namespace edash_packager

#endif  // MEDIA_FORMATS_MP4_MP4_MEDIA_PARSER_H_
