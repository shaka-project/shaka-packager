// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_BASE_MEDIA_PARSER_H_
#define PACKAGER_MEDIA_BASE_MEDIA_PARSER_H_

#include <memory>
#include <string>
#include <vector>
#include "packager/base/callback.h"
#include "packager/base/compiler_specific.h"
#include "packager/media/base/container_names.h"

namespace shaka {
namespace media {

class KeySource;
class MediaSample;
class StreamInfo;

class MediaParser {
 public:
  MediaParser() {}
  virtual ~MediaParser() {}

  /// Called upon completion of parser initialization.
  /// @param stream_info contains the stream info of all the elementary streams
  ///        within this file.
  typedef base::Callback<void(
      const std::vector<std::shared_ptr<StreamInfo> >& stream_info)>
      InitCB;

  /// Called when a new media sample has been parsed.
  /// @param track_id is the track id of the new sample.
  /// @param media_sample is the new media sample.
  /// @return true if the sample is accepted, false if something was wrong
  ///         with the sample and a parsing error should be signaled.
  typedef base::Callback<bool(uint32_t track_id,
                              const std::shared_ptr<MediaSample>& media_sample)>
      NewSampleCB;

  /// Initialize the parser with necessary callbacks. Must be called before any
  /// data is passed to Parse().
  /// @param init_cb will be called once enough data has been parsed to
  ///        determine the initial stream configurations.
  /// @param new_sample_cb will be called each time a new media sample is
  ///        available from the parser. May be NULL, and caller retains
  ///        ownership.
  virtual void Init(const InitCB& init_cb,
                    const NewSampleCB& new_sample_cb,
                    KeySource* decryption_key_source) = 0;

  /// Flush data currently in the parser and put the parser in a state where it
  /// can receive data for a new seek point.
  /// @return true if successful, false otherwise.
  virtual bool Flush() WARN_UNUSED_RESULT = 0;

  /// Should be called when there is new data to parse.
  /// @return true if successful.
  virtual bool Parse(const uint8_t* buf, int size) WARN_UNUSED_RESULT = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(MediaParser);
};

}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_BASE_MEDIA_PARSER_H_
