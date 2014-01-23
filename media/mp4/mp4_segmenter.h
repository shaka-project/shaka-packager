// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef MEDIA_MP4_MP4_SEGMENTER_H_
#define MEDIA_MP4_MP4_SEGMENTER_H_

#include <map>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "media/base/status.h"

namespace media {

struct MuxerOptions;

class BufferWriter;
class EncryptorSource;
class MediaSample;
class MediaStream;

namespace mp4 {

class MP4Fragmenter;

struct FileType;
struct Movie;
struct MovieFragment;
struct SegmentIndex;

/// This class defines the MP4 Segmenter which is responsible for organizing
/// MP4 fragments into segments/subsegments and package them into a MP4 file.
/// Inherited by MP4GeneralSegmenter and MP4VODSegmenter. MP4VODSegmenter
/// defines the segmenter for DASH Video-On-Demand with a single segment for
/// each media presentation while MP4GeneralSegmenter handles all other cases
/// including DASH live profile.
class MP4Segmenter {
 public:
  MP4Segmenter(const MuxerOptions& options,
               scoped_ptr<FileType> ftyp,
               scoped_ptr<Movie> moov);
  virtual ~MP4Segmenter();

  /// Initialize the segmenter.
  /// Calling other public methods of this class without this method returning
  /// Status::OK, results in an undefined behavior.
  /// @param encryptor_source can be NULL.
  /// @return Status::OK on success.
  virtual Status Initialize(EncryptorSource* encryptor_source,
                            double clear_lead_in_seconds,
                            const std::vector<MediaStream*>& streams);

  virtual Status Finalize();

  virtual Status AddSample(const MediaStream* stream,
                           scoped_refptr<MediaSample> sample);

  /// @return true if there is an initialization range, while setting @a offset
  ///         and @a size; or false if initialization range does not apply.
  virtual bool GetInitRange(size_t* offset, size_t* size) = 0;

  /// @return true if there is an index byte range, while setting @a offset
  ///         and @a size; or false if index byte range does not apply.
  virtual bool GetIndexRange(size_t* offset, size_t* size) = 0;

  uint32 GetReferenceTimeScale() const;

  /// @return The total length, in seconds, of segmented media files.
  double GetDuration() const;

 protected:
  void InitializeSegment();
  virtual Status FinalizeSegment();

  uint32 GetReferenceStreamId();

  const MuxerOptions& options() const { return options_; }
  FileType* ftyp() { return ftyp_.get(); }
  Movie* moov() { return moov_.get(); }
  BufferWriter* fragment_buffer() { return fragment_buffer_.get(); }
  SegmentIndex* sidx() { return sidx_.get(); }

 private:
  void InitializeFragments();
  Status FinalizeFragment(MP4Fragmenter* fragment);

  const MuxerOptions& options_;
  scoped_ptr<FileType> ftyp_;
  scoped_ptr<Movie> moov_;
  scoped_ptr<MovieFragment> moof_;
  scoped_ptr<BufferWriter> fragment_buffer_;
  scoped_ptr<SegmentIndex> sidx_;
  std::vector<MP4Fragmenter*> fragmenters_;
  std::vector<uint64> segment_durations_;
  std::map<const MediaStream*, uint32> stream_map_;
  bool segment_initialized_;
  bool end_of_segment_;

  DISALLOW_COPY_AND_ASSIGN(MP4Segmenter);
};

}  // namespace mp4
}  // namespace media

#endif  // MEDIA_MP4_MP4_SEGMENTER_H_
