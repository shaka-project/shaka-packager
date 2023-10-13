// Copyright 2014 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_FORMATS_MP4_MP4_MEDIA_PARSER_H_
#define PACKAGER_MEDIA_FORMATS_MP4_MP4_MEDIA_PARSER_H_

#include <cstdint>
#include <map>
#include <memory>
#include <vector>

#include <packager/macros/classes.h>
#include <packager/media/base/decryptor_source.h>
#include <packager/media/base/media_parser.h>
#include <packager/media/base/offset_byte_queue.h>

namespace shaka {
namespace media {
namespace mp4 {

class BoxReader;
class TrackRunIterator;
struct Movie;
struct ProtectionSystemSpecificHeader;

class MP4MediaParser : public MediaParser {
 public:
  MP4MediaParser();
  ~MP4MediaParser() override;

  /// @name MediaParser implementation overrides.
  /// @{
  void Init(const InitCB& init_cb,
            const NewMediaSampleCB& new_media_sample_cb,
            const NewTextSampleCB& new_text_sample_cb,
            KeySource* decryption_key_source) override;
  [[nodiscard]] bool Flush() override;
  [[nodiscard]] bool Parse(const uint8_t* buf, int size) override;
  /// @}

  /// Handles ISO-BMFF containers which have the 'moov' box trailing the
  /// movie data ('mdat'). It does this by doing a sparse parse of the file
  /// to locate the 'moov' box, and parsing its contents if it is found to be
  /// located after the 'mdat' box(es).
  /// @param file_path is the path to the media file to be parsed.
  /// @return true if successful, false otherwise.
  bool LoadMoov(const std::string& file_path);

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

  // To retain proper framing, each 'mdat' box must be read; to limit memory
  // usage, the box's data needs to be discarded incrementally as frames are
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
  NewMediaSampleCB new_sample_cb_;
  KeySource* decryption_key_source_;
  std::unique_ptr<DecryptorSource> decryptor_source_;

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

  std::unique_ptr<Movie> moov_;
  std::unique_ptr<TrackRunIterator> runs_;

  DISALLOW_COPY_AND_ASSIGN(MP4MediaParser);
};

}  // namespace mp4
}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_FORMATS_MP4_MP4_MEDIA_PARSER_H_
