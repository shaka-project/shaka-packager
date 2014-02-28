// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef MEDIA_MP4_MP4_MUXER_H_
#define MEDIA_MP4_MP4_MUXER_H_

#include <vector>

#include "media/base/muxer.h"
#include "media/mp4/fourccs.h"

namespace media {

class AudioStreamInfo;
class StreamInfo;
class VideoStreamInfo;

namespace mp4 {

class MP4Segmenter;

struct ProtectionSchemeInfo;
struct ProtectionSystemSpecificHeader;
struct Track;

/// Implements MP4 Muxer for ISO-BMFF. Please refer to ISO/IEC 14496-12: ISO
/// base media file format for details.
class MP4Muxer : public Muxer {
 public:
  /// Create a MP4Muxer object from MuxerOptions.
  explicit MP4Muxer(const MuxerOptions& options);
  virtual ~MP4Muxer();

  /// @name Muxer implementation overrides.
  /// @{
  virtual Status Initialize() OVERRIDE;
  virtual Status Finalize() OVERRIDE;
  virtual Status AddSample(const MediaStream* stream,
                           scoped_refptr<MediaSample> sample) OVERRIDE;
  /// @}

 private:
  // Generate Audio/Video Track atom.
  void InitializeTrak(const StreamInfo* info, Track* trak);
  void GenerateAudioTrak(const AudioStreamInfo* audio_info,
                         Track* trak,
                         uint32 track_id);
  void GenerateVideoTrak(const VideoStreamInfo* video_info,
                         Track* trak,
                         uint32 track_id);

  // Generate Pssh atom.
  void GeneratePssh(ProtectionSystemSpecificHeader* pssh);

  // Generates a sinf atom with CENC encryption parameters.
  void GenerateSinf(ProtectionSchemeInfo* sinf, FourCC old_type);

  // Should we enable encrytion?
  bool IsEncryptionRequired() { return (encryptor_source() != NULL); }

  // Helper functions for events.
  void GetStreamInfo(std::vector<StreamInfo*>* stream_infos);

  // Gets |start| and |end| initialization range. Returns true if there is an
  // init range and sets start-end byte-range-spec specified in RFC2616.
  bool GetInitRangeStartAndEnd(uint32* start, uint32* end);

  // Gets |start| and |end| index range. Returns true if there is an index range
  // and sets start-end byte-range-spec specified in RFC2616.
  bool GetIndexRangeStartAndEnd(uint32* start, uint32* end);

  // Fire events if there are no errors and Muxer::muxer_listener() is not NULL.
  void FireOnMediaStartEvent();
  void FireOnMediaEndEvent();

  // Get time in seconds since midnight, Jan. 1, 1904, in UTC Time.
  uint64 IsoTimeNow();

  scoped_ptr<MP4Segmenter> segmenter_;

  DISALLOW_COPY_AND_ASSIGN(MP4Muxer);
};

}  // namespace mp4
}  // namespace media

#endif  // MEDIA_MP4_MP4_MUXER_H_
