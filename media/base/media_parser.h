// Copyright (c) 2013 Google Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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

  // Indicates completion of parser initialization.
  // First parameter - Indicates initialization success. Set to true if
  //                   initialization was successful. False if an error
  //                   occurred.
  // Second parameter - A vector of all the elementary streams within this file.
  typedef base::Callback<void(bool, std::vector<scoped_refptr<StreamInfo> >&)>
      InitCB;

  // New stream sample have been parsed.
  // First parameter - The track id of the new sample.
  // Second parameter - The new media sample;
  // Return value - True indicates that the sample is accepted.
  //                False if something was wrong with the sample and a parsing
  //                error should be signaled.
  typedef base::Callback<
      bool(uint32 track_id, const scoped_refptr<MediaSample>&)>
      NewSampleCB;

  // A new potentially encrypted stream has been parsed.
  // First Parameter - Container name.
  // Second parameter - The initialization data associated with the stream.
  // Third parameter - Number of bytes of the initialization data.
  typedef base::Callback<void(MediaContainerName, scoped_ptr<uint8[]>, int)>
      NeedKeyCB;

  // Initialize the parser with necessary callbacks. Must be called before any
  // data is passed to Parse(). |init_cb| will be called once enough data has
  // been parsed to determine the initial stream configurations.
  virtual void Init(const InitCB& init_cb,
                    const NewSampleCB& new_sample_cb,
                    const NeedKeyCB& need_key_cb) = 0;

  // Called when there is new data to parse.
  //
  // Returns true if the parse succeeds.
  // TODO(kqyang): change to return Status.
  virtual bool Parse(const uint8* buf, int size) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(MediaParser);
};

}  // namespace media

#endif  // MEDIA_BASE_MEDIA_PARSER_H_
