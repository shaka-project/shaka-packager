// Copyright 2015 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef MEDIA_FORMATS_WEBM_SEGMENTER_H_
#define MEDIA_FORMATS_WEBM_SEGMENTER_H_

#include <memory>
#include "packager/media/base/status.h"
#include "packager/media/formats/webm/encryptor.h"
#include "packager/media/formats/webm/mkv_writer.h"
#include "packager/media/formats/webm/seek_head.h"
#include "packager/third_party/libwebm/src/mkvmuxer.hpp"

namespace shaka {
namespace media {

struct MuxerOptions;

class AudioStreamInfo;
class KeySource;
class MediaSample;
class StreamInfo;
class MuxerListener;
class ProgressListener;
class StreamInfo;
class VideoStreamInfo;

namespace webm {

class Segmenter {
 public:
  explicit Segmenter(const MuxerOptions& options);
  virtual ~Segmenter();

  /// Initialize the segmenter.
  /// Calling other public methods of this class without this method returning
  /// Status::OK results in an undefined behavior.
  /// @param writer contains the output file (or init file in multi-segment).
  /// @param info The stream info for the stream being segmented.
  /// @param muxer_listener receives muxer events. Can be NULL.
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
  /// @return OK on success, an error status otherwise.
  Status Initialize(std::unique_ptr<MkvWriter> writer,
                    StreamInfo* info,
                    ProgressListener* progress_listener,
                    MuxerListener* muxer_listener,
                    KeySource* encryption_key_source,
                    uint32_t max_sd_pixels,
                    uint32_t max_hd_pixels,
                    uint32_t max_uhd1_pixels,
                    double clear_lead_in_seconds);

  /// Finalize the segmenter.
  /// @return OK on success, an error status otherwise.
  Status Finalize();

  /// Add sample to the indicated stream.
  /// @param sample points to the sample to be added.
  /// @return OK on success, an error status otherwise.
  Status AddSample(std::shared_ptr<MediaSample> sample);

  /// Finalize the (sub)segment.
  virtual Status FinalizeSegment(uint64_t start_timescale,
                                 uint64_t duration_timescale,
                                 bool is_subsegment) = 0;

  /// @return true if there is an initialization range, while setting @a start
  ///         and @a end; or false if initialization range does not apply.
  virtual bool GetInitRangeStartAndEnd(uint64_t* start, uint64_t* end) = 0;

  /// @return true if there is an index byte range, while setting @a start
  ///         and @a end; or false if index byte range does not apply.
  virtual bool GetIndexRangeStartAndEnd(uint64_t* start, uint64_t* end) = 0;

  /// @return The total length, in seconds, of segmented media files.
  float GetDuration() const;

 protected:
  /// Converts the given time in ISO BMFF timescale to the current WebM
  /// timecode.
  uint64_t FromBMFFTimescale(uint64_t time_timescale);
  /// Converts the given time in WebM timecode to ISO BMFF timescale.
  uint64_t FromWebMTimecode(uint64_t time_webm_timecode);
  /// Writes the Segment header to @a writer.
  Status WriteSegmentHeader(uint64_t file_size, MkvWriter* writer);
  /// Creates a Cluster object with the given parameters.
  Status SetCluster(uint64_t start_webm_timecode,
                    uint64_t position,
                    MkvWriter* writer);

  /// Update segmentation progress using ProgressListener.
  void UpdateProgress(uint64_t progress);
  void set_progress_target(uint64_t target) { progress_target_ = target; }

  const MuxerOptions& options() const { return options_; }
  mkvmuxer::Cluster* cluster() { return cluster_.get(); }
  mkvmuxer::Cues* cues() { return &cues_; }
  MuxerListener* muxer_listener() { return muxer_listener_; }
  StreamInfo* info() { return info_; }
  SeekHead* seek_head() { return &seek_head_; }

  int track_id() const { return track_id_; }
  uint64_t segment_payload_pos() const { return segment_payload_pos_; }

  virtual Status DoInitialize(std::unique_ptr<MkvWriter> writer) = 0;
  virtual Status DoFinalize() = 0;

 private:
  Status CreateVideoTrack(VideoStreamInfo* info);
  Status CreateAudioTrack(AudioStreamInfo* info);
  Status InitializeEncryptor(KeySource* key_source, uint32_t max_sd_pixels,
                             uint32_t max_hd_pixels, uint32_t max_uhd1_pixels);

  // Writes the previous frame to the file.
  Status WriteFrame(bool write_duration);

  // This is called when there needs to be a new (sub)segment.
  // In single-segment mode, a Cluster is a segment and there is no subsegment.
  // In multi-segment mode, a new file is a segment and the clusters in the file
  // are subsegments.
  virtual Status NewSegment(uint64_t start_timescale, bool is_subsegment) = 0;

  // Store the previous sample so we know which one is the last frame.
  std::shared_ptr<MediaSample> prev_sample_;
  // The reference frame timestamp; used to populate the ReferenceBlock element
  // when writing non-keyframe BlockGroups.
  uint64_t reference_frame_timestamp_ = 0;

  const MuxerOptions& options_;
  std::unique_ptr<Encryptor> encryptor_;
  double clear_lead_ = 0;
  // Encryption is enabled only after clear_lead_.
  bool enable_encryption_ = false;

  std::unique_ptr<mkvmuxer::Cluster> cluster_;
  mkvmuxer::Cues cues_;
  SeekHead seek_head_;
  mkvmuxer::SegmentInfo segment_info_;
  mkvmuxer::Tracks tracks_;

  StreamInfo* info_ = nullptr;
  MuxerListener* muxer_listener_ = nullptr;
  ProgressListener* progress_listener_ = nullptr;
  uint64_t progress_target_ = 0;
  uint64_t accumulated_progress_ = 0;
  uint64_t first_timestamp_ = 0;
  int64_t sample_duration_ = 0;
  // The position (in bytes) of the start of the Segment payload in the init
  // file.  This is also the size of the header before the SeekHead.
  uint64_t segment_payload_pos_ = 0;

  // Indicate whether a new segment needed to be created, which is always true
  // in the beginning.
  bool new_segment_ = true;
  // Indicate whether a new subsegment needed to be created.
  bool new_subsegment_ = false;
  int track_id_ = 0;

  DISALLOW_COPY_AND_ASSIGN(Segmenter);
};

}  // namespace webm
}  // namespace media
}  // namespace shaka

#endif  // MEDIA_FORMATS_WEBM_SEGMENTER_H_
