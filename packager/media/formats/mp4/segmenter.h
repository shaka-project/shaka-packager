// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef MEDIA_FORMATS_MP4_SEGMENTER_H_
#define MEDIA_FORMATS_MP4_SEGMENTER_H_

#include <map>
#include <memory>
#include <vector>

#include "packager/base/memory/ref_counted.h"
#include "packager/media/base/fourccs.h"
#include "packager/media/base/status.h"
#include "packager/media/formats/mp4/box_definitions.h"

namespace shaka {
namespace media {

struct MuxerOptions;

class BufferWriter;
class KeySource;
class MediaSample;
class MediaStream;
class MuxerListener;
class ProgressListener;

namespace mp4 {

class Fragmenter;

/// This class defines the Segmenter which is responsible for organizing
/// fragments into segments/subsegments and package them into a MP4 file.
/// Inherited by MultiSegmentSegmenter and SingleSegmentSegmenter.
/// SingleSegmentSegmenter defines the Segmenter for DASH Video-On-Demand with
/// a single segment for each media presentation while MultiSegmentSegmenter
/// handles all other cases including DASH live profile.
class Segmenter {
 public:
  Segmenter(const MuxerOptions& options,
            std::unique_ptr<FileType> ftyp,
            std::unique_ptr<Movie> moov);
  virtual ~Segmenter();

  /// Initialize the segmenter.
  /// Calling other public methods of this class without this method returning
  /// Status::OK results in an undefined behavior.
  /// @param streams contains the vector of MediaStreams to be segmented.
  /// @param muxer_listener receives muxer events. Can be NULL.
  /// @param progress_listener receives progress updates. Can be NULL.
  /// @param encryption_key_source points to the key source which contains
  ///        the encryption keys. It can be NULL to indicate that no encryption
  ///        is required.
  /// @param max_sd_pixels specifies the threshold to determine whether a video
  ///        track should be considered as SD. If the max pixels per frame is
  ///        no higher than max_sd_pixels, it is SD.
  /// @param max_hd_pixels specifies the threshold to determine whether a video
  ///        track should be considered as HD. If the max pixels per frame is
  ///        higher than max_sd_pixels, but no higher than max_hd_pixels,
  ///        it is HD.
  /// @param max_uhd1_pixels specifies the threshold to determine whether a video
  ///        track should be considered as UHD1. If the max pixels per frame is
  ///        higher than max_hd_pixels, but no higher than max_uhd1_pixels,
  ///        it is UHD1. Otherwise it is UHD2.
  /// @param clear_time specifies clear lead duration in seconds.
  /// @param crypto_period_duration specifies crypto period duration in seconds.
  /// @param protection_scheme specifies the protection scheme: 'cenc', 'cens',
  ///        'cbc1', 'cbcs'.
  /// @return OK on success, an error status otherwise.
  Status Initialize(const std::vector<MediaStream*>& streams,
                    MuxerListener* muxer_listener,
                    ProgressListener* progress_listener,
                    KeySource* encryption_key_source,
                    uint32_t max_sd_pixels,
                    uint32_t max_hd_pixels,
                    uint32_t max_uhd1_pixels,
                    double clear_lead_in_seconds,
                    double crypto_period_duration_in_seconds,
                    FourCC protection_scheme);

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

  /// @return The sample duration in the timescale of the media.
  ///         Returns 0 if no samples are added yet.
  uint32_t sample_duration() const { return sample_duration_; }

 protected:
  /// Update segmentation progress using ProgressListener.
  void UpdateProgress(uint64_t progress);
  /// Set progress to 100%.
  void SetComplete();

  const MuxerOptions& options() const { return options_; }
  FileType* ftyp() { return ftyp_.get(); }
  Movie* moov() { return moov_.get(); }
  BufferWriter* fragment_buffer() { return fragment_buffer_.get(); }
  SegmentIndex* sidx() { return sidx_.get(); }
  MuxerListener* muxer_listener() { return muxer_listener_; }
  uint64_t progress_target() { return progress_target_; }

  void set_progress_target(uint64_t progress_target) {
    progress_target_ = progress_target;
  }

 private:
  virtual Status DoInitialize() = 0;
  virtual Status DoFinalize() = 0;
  virtual Status DoFinalizeSegment() = 0;

  Status FinalizeSegment();
  uint32_t GetReferenceStreamId();

  Status FinalizeFragment(bool finalize_segment, Fragmenter* fragment);

  const MuxerOptions& options_;
  std::unique_ptr<FileType> ftyp_;
  std::unique_ptr<Movie> moov_;
  std::unique_ptr<MovieFragment> moof_;
  std::unique_ptr<BufferWriter> fragment_buffer_;
  std::unique_ptr<SegmentIndex> sidx_;
  std::vector<std::unique_ptr<Fragmenter>> fragmenters_;
  std::vector<uint64_t> segment_durations_;
  std::map<const MediaStream*, uint32_t> stream_map_;
  MuxerListener* muxer_listener_;
  ProgressListener* progress_listener_;
  uint64_t progress_target_;
  uint64_t accumulated_progress_;
  uint32_t sample_duration_;

  DISALLOW_COPY_AND_ASSIGN(Segmenter);
};

}  // namespace mp4
}  // namespace media
}  // namespace shaka

#endif  // MEDIA_FORMATS_MP4_SEGMENTER_H_
