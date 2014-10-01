// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef MEDIA_FORMATS_MP4_SEGMENTER_H_
#define MEDIA_FORMATS_MP4_SEGMENTER_H_

#include <map>
#include <vector>

#include "packager/base/memory/ref_counted.h"
#include "packager/base/memory/scoped_ptr.h"
#include "packager/media/base/status.h"

namespace edash_packager {
namespace media {

struct MuxerOptions;

class BufferWriter;
class KeySource;
class MediaSample;
class MediaStream;

namespace event {
class MuxerListener;
}  // namespace event

namespace mp4 {

class Fragmenter;

struct FileType;
struct Movie;
struct MovieFragment;
struct SegmentIndex;

/// This class defines the Segmenter which is responsible for organizing
/// fragments into segments/subsegments and package them into a MP4 file.
/// Inherited by MultiSegmentSegmenter and SingleSegmentSegmenter.
/// SingleSegmentSegmenter defines the Segmenter for DASH Video-On-Demand with
/// a single segment for each media presentation while MultiSegmentSegmenter
/// handles all other cases including DASH live profile.
class Segmenter {
 public:
  Segmenter(const MuxerOptions& options,
            scoped_ptr<FileType> ftyp,
            scoped_ptr<Movie> moov);
  virtual ~Segmenter();

  /// Initialize the segmenter.
  /// Calling other public methods of this class without this method returning
  /// Status::OK results in an undefined behavior.
  /// @param streams contains the vector of MediaStreams to be segmented.
  /// @param encryption_key_source points to the key source which contains
  ///        the encryption keys. It can be NULL to indicate that no encryption
  ///        is required.
  /// @param max_sd_pixels specifies the threshold to determine whether a video
  ///        track should be considered as SD or HD. If the track has more
  ///        pixels per frame than max_sd_pixels, it is HD, SD otherwise.
  /// @param clear_time specifies clear lead duration in seconds.
  /// @param crypto_period_duration specifies crypto period duration in seconds.
  /// @return OK on success, an error status otherwise.
  Status Initialize(const std::vector<MediaStream*>& streams,
                    event::MuxerListener* muxer_listener,
                    KeySource* encryption_key_source,
                    uint32_t max_sd_pixels,
                    double clear_lead_in_seconds,
                    double crypto_period_duration_in_seconds);

  /// Finalize the segmenter.
  /// @return OK on success, an error status otherwise.
  Status Finalize();

  /// Add sample to the indicated stream.
  /// @param stream points to the stream to which the sample belongs. It cannot
  ///        be NULL.
  /// @param sample points to the sample to be added.
  /// @return OK on success, an error status otherwise.
  Status AddSample(const MediaStream* stream,
                   scoped_refptr<MediaSample> sample);

  /// @return true if there is an initialization range, while setting @a offset
  ///         and @a size; or false if initialization range does not apply.
  virtual bool GetInitRange(size_t* offset, size_t* size) = 0;

  /// @return true if there is an index byte range, while setting @a offset
  ///         and @a size; or false if index byte range does not apply.
  virtual bool GetIndexRange(size_t* offset, size_t* size) = 0;

  uint32_t GetReferenceTimeScale() const;

  /// @return The total length, in seconds, of segmented media files.
  double GetDuration() const;

 protected:
  const MuxerOptions& options() const { return options_; }
  FileType* ftyp() { return ftyp_.get(); }
  Movie* moov() { return moov_.get(); }
  BufferWriter* fragment_buffer() { return fragment_buffer_.get(); }
  SegmentIndex* sidx() { return sidx_.get(); }
  event::MuxerListener* muxer_listener() { return muxer_listener_; }

 private:
  virtual Status DoInitialize() = 0;
  virtual Status DoFinalize() = 0;
  virtual Status DoFinalizeSegment() = 0;

  void InitializeSegment();
  Status FinalizeSegment();
  uint32_t GetReferenceStreamId();

  Status FinalizeFragment(Fragmenter* fragment);

  const MuxerOptions& options_;
  scoped_ptr<FileType> ftyp_;
  scoped_ptr<Movie> moov_;
  scoped_ptr<MovieFragment> moof_;
  scoped_ptr<BufferWriter> fragment_buffer_;
  scoped_ptr<SegmentIndex> sidx_;
  std::vector<Fragmenter*> fragmenters_;
  std::vector<uint64_t> segment_durations_;
  std::map<const MediaStream*, uint32_t> stream_map_;
  bool segment_initialized_;
  bool end_of_segment_;
  event::MuxerListener* muxer_listener_;

  DISALLOW_COPY_AND_ASSIGN(Segmenter);
};

}  // namespace mp4
}  // namespace media
}  // namespace edash_packager

#endif  // MEDIA_FORMATS_MP4_SEGMENTER_H_
