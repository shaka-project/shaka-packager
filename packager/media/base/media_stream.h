// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef MEDIA_BASE_MEDIA_STREAM_H_
#define MEDIA_BASE_MEDIA_STREAM_H_

#include <deque>
#include <memory>

#include "packager/media/base/status.h"

namespace shaka {
namespace media {

class Demuxer;
class Muxer;
class MediaSample;
class StreamInfo;

/// MediaStream connects Demuxer to Muxer. It is an abstraction for a media
/// elementary stream.
class MediaStream {
 public:
  enum MediaStreamOperation {
    kPush,
    kPull,
  };
  /// Create MediaStream from StreamInfo and Demuxer.
  /// @param demuxer cannot be NULL.
  MediaStream(std::shared_ptr<StreamInfo> info, Demuxer* demuxer);
  ~MediaStream();

  /// Connect the stream to Muxer.
  /// @param muxer cannot be NULL.
  void Connect(Muxer* muxer);

  /// Start the stream for pushing or pulling.
  Status Start(MediaStreamOperation operation);

  /// Push sample to Muxer (triggered by Demuxer).
  Status PushSample(const std::shared_ptr<MediaSample>& sample);

  /// Pull sample from Demuxer (triggered by Muxer).
  Status PullSample(std::shared_ptr<MediaSample>* sample);

  Demuxer* demuxer() { return demuxer_; }
  Muxer* muxer() { return muxer_; }
  const std::shared_ptr<StreamInfo> info() const;

  /// @return a human-readable string describing |*this|.
  std::string ToString() const;

 private:
  MediaStream(const MediaStream&) = delete;
  MediaStream& operator=(const MediaStream&) = delete;

  // State transition diagram available @ http://goo.gl/ThJQbl.
  enum State {
    kIdle,
    kConnected,
    kDisconnected,
    kPushing,
    kPulling,
  };

  std::shared_ptr<StreamInfo> info_;
  Demuxer* demuxer_;
  Muxer* muxer_;
  State state_;
  // An internal buffer to store samples temporarily.
  std::deque<std::shared_ptr<MediaSample>> samples_;
};

}  // namespace media
}  // namespace shaka

#endif  // MEDIA_BASE_MEDIA_STREAM_H_
