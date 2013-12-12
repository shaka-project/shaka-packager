// Copyright (c) 2013 Google Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This class defines the MP4 Segmenter which is responsible for organizing
// MP4 fragments into segments/subsegments and package into a MP4 file.
// Inherited by MP4GeneralSegmenter and MP4VODSegmenter. MP4VODSegmenter defines
// the segmenter for DASH Video-On-Demand with a single segment for each media
// presentation while MP4GeneralSegmenter handles all other cases including
// DASH live profile.

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

class MP4Segmenter {
 public:
  // Caller transfers the ownership of |ftyp| and |moov| to this class.
  MP4Segmenter(const MuxerOptions& options,
               scoped_ptr<FileType> ftyp,
               scoped_ptr<Movie> moov);
  virtual ~MP4Segmenter();

  // Initialize the segmenter. Caller retains the ownership of
  // |encryptor_source|. |encryptor_source| can be NULL.
  // Calling other public methods of this class without this method returning
  // Status::OK, results in an undefined behavior.
  virtual Status Initialize(EncryptorSource* encryptor_source,
                            double clear_lead_in_seconds,
                            const std::vector<MediaStream*>& streams);

  virtual Status Finalize();

  virtual Status AddSample(const MediaStream* stream,
                           scoped_refptr<MediaSample> sample);

  // Returns false if it does not apply.
  // If it has an initialization byte range this returns true and set |offset|
  // and |size|, otherwise returns false.
  virtual bool GetInitRange(size_t* offset, size_t* size) = 0;

  // Returns false if it does not apply.
  // If it has an index byte range this returns true and set |offset| and
  // |size|, otherwise returns false.
  virtual bool GetIndexRange(size_t* offset, size_t* size) = 0;

  uint32 GetReferenceTimeScale() const;

  // Returns the total length, in seconds, of segmented media files.
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
