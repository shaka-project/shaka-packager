// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef MEDIA_BASE_MEDIA_STREAM_H_
#define MEDIA_BASE_MEDIA_STREAM_H_

#include <deque>
#include <memory>

#include "packager/base/memory/ref_counted.h"
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
  MediaStream(scoped_refptr<StreamInfo> info, Demuxer* demuxer);
  ~MediaStream();

  /// Connect the stream to Muxer.
  /// @param muxer cannot be NULL.
  void Connect(Muxer* muxer);

  /// Start the stream for pushing or pulling.
  Status Start(MediaStreamOperation operation);

  /// Push sample to Muxer (triggered by Demuxer).
  Status PushSample(const scoped_refptr<MediaSample>& sample);

  /// Pull sample from Demuxer (triggered by Muxer).
  Status PullSample(scoped_refptr<MediaSample>* sample);

  Demuxer* demuxer() { return demuxer_; }
  Muxer* muxer() { return muxer_; }
  const scoped_refptr<StreamInfo> info() const;

  /// @return a human-readable string describing |*this|.
  std::string ToString() const;

 private:
  // State transition diagram available @ http://goo.gl/ThJQbl.
  enum State {
    kIdle,
    kConnected,
    kDisconnected,
    kPushing,
    kPulling,
  };

  scoped_refptr<StreamInfo> info_;
  Demuxer* demuxer_;
  Muxer* muxer_;
  State state_;
  // An internal buffer to store samples temporarily.
  std::deque<scoped_refptr<MediaSample> > samples_;

  DISALLOW_COPY_AND_ASSIGN(MediaStream);
};

}  // namespace media
}  // namespace shaka

#endif  // MEDIA_BASE_MEDIA_STREAM_H_
