// Copyright 2014 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_BASE_MEDIA_PARSER_H_
#define PACKAGER_MEDIA_BASE_MEDIA_PARSER_H_

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <packager/macros/classes.h>
#include <packager/media/base/container_names.h>

namespace shaka {
namespace media {

class KeySource;
class MediaSample;
class StreamInfo;
class TextSample;

class MediaParser {
 public:
  MediaParser() {}
  virtual ~MediaParser() {}

  /// Called upon completion of parser initialization.
  /// @param stream_info contains the stream info of all the elementary streams
  ///        within this file.
  typedef std::function<void(
      const std::vector<std::shared_ptr<StreamInfo> >& stream_info)>
      InitCB;

  /// Called when a new media sample has been parsed.
  /// @param track_id is the track id of the new sample.
  /// @param media_sample is the new media sample.
  /// @return true if the sample is accepted, false if something was wrong
  ///         with the sample and a parsing error should be signaled.
  typedef std::function<bool(uint32_t track_id,
                             std::shared_ptr<MediaSample> media_sample)>
      NewMediaSampleCB;

  /// Called when a new text sample has been parsed.
  /// @param track_id is the track id of the new sample.
  /// @param text_sample is the new text sample.
  /// @return true if the sample is accepted, false if something was wrong
  ///         with the sample and a parsing error should be signaled.
  typedef std::function<bool(uint32_t track_id,
                             std::shared_ptr<TextSample> text_sample)>
      NewTextSampleCB;

  /// Initialize the parser with necessary callbacks. Must be called before any
  /// data is passed to Parse().
  /// @param init_cb will be called once enough data has been parsed to
  ///        determine the initial stream configurations.
  /// @param new_media_sample_cb will be called each time a new media sample is
  ///        available from the parser.
  /// @param new_text_sample_cb will be called each time a new text sample is
  ///        available from the parser.
  /// @param decryption_key_source the key source to decrypt the frames.  May be
  ///        NULL, and caller retains ownership.
  virtual void Init(const InitCB& init_cb,
                    const NewMediaSampleCB& new_media_sample_cb,
                    const NewTextSampleCB& new_text_sample_cb,
                    KeySource* decryption_key_source) = 0;

  /// Flush data currently in the parser and put the parser in a state where it
  /// can receive data for a new seek point.
  /// @return true if successful, false otherwise.
  [[nodiscard]] virtual bool Flush() = 0;

  /// Should be called when there is new data to parse.
  /// @return true if successful.
  [[nodiscard]] virtual bool Parse(const uint8_t* buf, int size) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(MediaParser);
};

}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_BASE_MEDIA_PARSER_H_
