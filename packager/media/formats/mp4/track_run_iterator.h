// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PACKAGER_MEDIA_FORMATS_MP4_TRACK_RUN_ITERATOR_H_
#define PACKAGER_MEDIA_FORMATS_MP4_TRACK_RUN_ITERATOR_H_

#include <cstdint>
#include <map>
#include <memory>
#include <vector>

#include <packager/macros/classes.h>
#include <packager/media/formats/mp4/box_definitions.h>

namespace shaka {
namespace media {

class DecryptConfig;

namespace mp4 {

struct SampleInfo;
struct TrackRunInfo;

class TrackRunIterator {
 public:
  /// Create a new TrackRunIterator from movie box.
  /// @param moov should not be NULL.
  explicit TrackRunIterator(const Movie* moov);
  ~TrackRunIterator();

  /// For non-fragmented mp4, moov contains all the chunk information; This
  /// function sets up the iterator to access all the chunks.
  /// For fragmented mp4, chunk and sample information are generally contained
  /// in moof. This function is a no-op in this case. Init(moof) will be called
  /// later after parsing moof.
  /// @return true on success, false otherwise.
  bool Init();

  /// Set up the iterator to handle all the runs from the current fragment.
  /// @return true on success, false otherwise.
  bool Init(const MovieFragment& moof);

  /// @return true if the iterator points to a valid run, false if past the
  ///         last run.
  bool IsRunValid() const;
  /// @return true if the iterator points to a valid sample, false if past the
  ///         last sample.
  bool IsSampleValid() const;

  /// Advance iterator to the next run. Require that the iterator point to a
  /// valid run.
  void AdvanceRun();
  /// Advance iterator to the next sample. Require that the iterator point to a
  /// valid sample.
  void AdvanceSample();

  /// @return true if this track run has auxiliary information and has not yet
  ///         been cached. Only valid if IsRunValid().
  bool AuxInfoNeedsToBeCached();

  /// Caches the CENC data from the given buffer.
  /// @param buf must be a buffer starting at the offset given by cenc_offset().
  /// @param size must be at least cenc_size().
  /// @return true on success, false on error.
  bool CacheAuxInfo(const uint8_t* buf, int size);

  /// @return the maximum buffer location at which no data earlier in the
  ///         stream will be required in order to read the current or any
  ///         subsequent sample. You may clear all data up to this offset
  ///         before reading the current sample safely. Result is in the same
  ///         units as offset() (for Media Source this is in bytes past the
  ///         head of the MOOF box).
  int64_t GetMaxClearOffset();

  /// @name Properties of the current run. Only valid if IsRunValid().
  /// @{
  uint32_t track_id() const;
  int64_t aux_info_offset() const;
  int aux_info_size() const;
  bool is_encrypted() const;
  bool is_audio() const;
  bool is_video() const;
  /// @}

  /// Only valid if is_audio() is true.
  const AudioSampleEntry& audio_description() const;
  /// Only valid if is_video() is true.
  const VideoSampleEntry& video_description() const;

  /// @name Properties of the current sample. Only valid if IsSampleValid().
  /// @{
  int64_t sample_offset() const;
  int sample_size() const;
  int64_t dts() const;
  int64_t cts() const;
  int64_t duration() const;
  bool is_keyframe() const;
  /// @}

  /// Only call when is_encrypted() is true and AuxInfoNeedsToBeCached() is
  /// false. Result is owned by caller.
  std::unique_ptr<DecryptConfig> GetDecryptConfig();

 private:
  void ResetRun();
  const TrackEncryption& track_encryption() const;
  int64_t GetTimestampAdjustment(const Movie& movie,
                                 const Track& track,
                                 const TrackFragment* traf);

  const Movie* moov_;

  std::vector<TrackRunInfo> runs_;
  std::vector<TrackRunInfo>::const_iterator run_itr_;
  std::vector<SampleInfo>::const_iterator sample_itr_;

  // Track the start dts of the next segment, only useful if decode_time box is
  // absent.
  std::vector<int64_t> next_fragment_start_dts_;

  int64_t sample_dts_;
  int64_t sample_offset_;

  // TrackId => adjustment map.
  std::map<uint32_t, int64_t> timestamp_adjustment_map_;

  DISALLOW_COPY_AND_ASSIGN(TrackRunIterator);
};

}  // namespace mp4
}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_FORMATS_MP4_TRACK_RUN_ITERATOR_H_
