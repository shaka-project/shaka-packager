// Copyright (c) 2013 Google Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Implements MP4 Muxer.

#ifndef MEDIA_MP4_MP4_MUXER_H_
#define MEDIA_MP4_MP4_MUXER_H_

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

class MP4Muxer : public Muxer {
 public:
  MP4Muxer(const MuxerOptions& options, EncryptorSource* encryptor_source);
  virtual ~MP4Muxer();

  // Muxer implementations.
  virtual Status Initialize() OVERRIDE;
  virtual Status Finalize() OVERRIDE;
  virtual Status AddSample(const MediaStream* stream,
                           scoped_refptr<MediaSample> sample) OVERRIDE;

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

  scoped_ptr<MP4Segmenter> segmenter_;

  DISALLOW_COPY_AND_ASSIGN(MP4Muxer);
};

}  // namespace mp4
}  // namespace media

#endif  // MEDIA_MP4_MP4_MUXER_H_
