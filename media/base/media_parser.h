// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef MEDIA_BASE_MEDIA_PARSER_H_
#define MEDIA_BASE_MEDIA_PARSER_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "media/base/container_names.h"

namespace media {

class MediaSample;
class StreamInfo;

class MediaParser {
 public:
  MediaParser() {}
  virtual ~MediaParser() {}

  /// Called upon completion of parser initialization.
  /// @param stream_info contains the stream info of all the elementary streams
  ///        within this file.
  typedef base::Callback<
      void(const std::vector<scoped_refptr<StreamInfo> >& stream_info)> InitCB;

  /// Called when a new media sample has been parsed.
  /// @param track_id is the track id of the new sample.
  /// @param media_sample is the new media sample.
  /// @return true if the sample is accepted, false if something was wrong
  ///         with the sample and a parsing error should be signaled.
  typedef base::Callback<
      bool(uint32 track_id, const scoped_refptr<MediaSample>& media_sample)>
      NewSampleCB;

  /// Called when a new potentially encrypted stream has been parsed.
  /// @param init_data is the initialization data associated with the stream.
  /// @param init_data_size is the number of bytes of the initialization data.
  typedef base::Callback<void(MediaContainerName container_name,
                              scoped_ptr<uint8[]> init_data,
                              int init_data_size)> NeedKeyCB;

  /// Initialize the parser with necessary callbacks. Must be called before any
  /// data is passed to Parse().
  /// @param init_cb will be called once enough data has been parsed to
  ///        determine the initial stream configurations.
  virtual void Init(const InitCB& init_cb,
                    const NewSampleCB& new_sample_cb,
                    const NeedKeyCB& need_key_cb) = 0;

  /// Should be called when there is new data to parse.
  /// @return true if successful.
  virtual bool Parse(const uint8* buf, int size) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(MediaParser);
};

}  // namespace media

#endif  // MEDIA_BASE_MEDIA_PARSER_H_
