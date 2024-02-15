// Copyright 2019 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_EVENT_MULTI_CODEC_MUXER_LISTENER_H_
#define PACKAGER_MEDIA_EVENT_MULTI_CODEC_MUXER_LISTENER_H_

#include <packager/media/event/combined_muxer_listener.h>

namespace shaka {
namespace media {

/// MultiCodecMuxerListener is a variant of CombinedMuxerListener. It is
/// designed to handle the case that a stream can be signalled in multiple
/// different codecs. Like a normal CombinedMuxerListener, it contains multiple
/// child MuxerListeners, with one child per codec. If there are more child
/// MuxerListeners than the number of codecs, the extra child MuxerListeners are
/// removed; on the other hand, if there are more codecs than the number of
/// child MuxerListeners, the extra codecs are not handled.
class MultiCodecMuxerListener : public CombinedMuxerListener {
 public:
  MultiCodecMuxerListener() = default;

  /// @name CombinedMuxerListener implementation overrides.
  /// @{
  void OnMediaStart(const MuxerOptions& muxer_options,
                    const StreamInfo& stream_info,
                    int32_t time_scale,
                    ContainerType container_type) override;
  /// @}

 private:
  MultiCodecMuxerListener(const MultiCodecMuxerListener&) = delete;
  MultiCodecMuxerListener& operator=(const MultiCodecMuxerListener&) = delete;
};

}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_EVENT_MULTI_CODEC_MUXER_LISTENER_H_
