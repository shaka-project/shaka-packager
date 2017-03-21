// Copyright 2017 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_BASE_TRICK_PLAY_HANDLER_H_
#define PACKAGER_MEDIA_BASE_TRICK_PLAY_HANDLER_H_

#include "packager/media/base/media_handler.h"

namespace shaka {
namespace media {

/// TrickPlayHandler is a single-input-multiple-output media handler. It creates
/// trick play streams from the input.
// The stream data in trick play stream is not a simple duplicate. Some
// information need to be updated, including trick_play_rate in
// VideoStreamInfo, the duration in MediaSample (which makes sure there is no
// gap between the media sample dts). Since the duration information can be
// determined after getting the next media sample, a queue is used to cache the
// input stream data before the next key frame.
class TrickPlayHandler : public MediaHandler {
 public:
  TrickPlayHandler();
  ~TrickPlayHandler() override;

  void SetHandlerForMainStream(std::shared_ptr<MediaHandler> handler);
  void SetHandlerForTrickPlay(uint32_t trick_play_rate,
                              std::shared_ptr<MediaHandler> handler);

 protected:
  /// @name MediaHandler implementation overrides.
  /// @{
  Status InitializeInternal() override;
  Status Process(std::unique_ptr<StreamData> stream_data) override;
  bool ValidateOutputStreamIndex(size_t stream_index) const override;
  Status OnFlushRequest(size_t input_stream_index) override;
  /// @}

 private:
  friend class TrickPlayHandlerTest;

  // Returns true if the trick play handler has main stream output handler
  // connected, otherwise returns false.
  bool HasMainStream();

  // Process the cached stream data for one trick play stream.
  // The cached data is dispatched to the |output_stream_index|.
  Status ProcessCachedStreamData(
      size_t output_stream_index,
      std::deque<std::shared_ptr<StreamData>>* cached_stream_data);

  // Process a single stream data. Depending on the stream data type, some
  // information needs to be updated.
  // Decoding timestamp for current key media sample. It is used for calculating
  // the duration of previous key media sample, to make sure there is no gap
  // between two key media samples.
  Status ProcessOneStreamData(size_t output_stream_index,
                              const std::shared_ptr<StreamData>& stream_data);

  // Trick play rates. Note that there can be multiple trick play rates,
  // e.g., 2, 4 and 8. That means, one input video stream will generate 3
  // output trick play streams and original stream. Three trick play streams
  // are:
  // [key_frame_0, key_frame_2, key_frame_4, ...]
  // [key_frame_0, key_frame_4, key_frame_8,...]
  // [key_frame_0, key_frame_8, key_frame_16, ...].
  std::vector<uint32_t> trick_play_rates_;

  TrickPlayHandler(const TrickPlayHandler&) = delete;
  TrickPlayHandler& operator=(const TrickPlayHandler&) = delete;

  /// Num of key frames received.
  uint32_t total_key_frames_ = 0;

  // Num of frames received.
  uint32_t total_frames_ = 0;

  // End timestamp of the previous processed media_sample, which is |dts| +
  // |duration|. The duration of key frame in trick play stream is updated based
  // on this timestamp.
  int64_t prev_sample_end_timestamp_ = 0;

  // Record playback_rate for each trick play stream.
  std::vector<uint32_t> playback_rates_;

  // The data in output streams should be in the same order as in the input
  // stream. Cache the stream data before next key frame so that we can
  // determine the duration for the current key frame. Since one key frame may
  // be dispatched to different trick play stream, each trick play stream need
  // its own queue to handle the synchronization.
  // TODO(hmchen): Use one queue and multiple iterators, instead of multiple
  // queues.
  std::vector<std::deque<std::shared_ptr<StreamData>>> cached_stream_data_;
};

}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_BASE_TRICK_PLAY_HANDLER_H_
