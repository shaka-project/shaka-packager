// Copyright (c) 2013 Google Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_DEMUXER_H_
#define MEDIA_BASE_DEMUXER_H_

#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "media/base/container_names.h"
#include "media/base/status.h"

namespace media {

class Decryptor;
class DecryptorSource;
class File;
class MediaParser;
class MediaSample;
class MediaStream;
class StreamInfo;

class Demuxer {
 public:
  // |file_name| specifies the input source. It uses prefix matching to create
  // a proper File object. The user can extend File to support their custom
  // File objects with its own prefix.
  // decryptor_source generates decryptor(s) when init_data is available.
  // Demuxer does not take over the ownership of decryptor_source.
  Demuxer(const std::string& file_name, DecryptorSource* decryptor_source);
  ~Demuxer();

  // Initializes corresponding MediaParser, Decryptor, instantiates
  // MediaStream(s) etc.
  Status Initialize();

  // Drives the remuxing from demuxer side (push): Reads the file and push
  // the Data to Muxer until Eof.
  Status Run();

  // Reads from the source and send it to the parser.
  Status Parse();

  uint32 num_streams() const {
    return streams_.size();
  }

  // Demuxer retains the ownership of streams.
  MediaStream* streams(uint32 index) {
    if (index >= streams_.size())
      return NULL;
    return streams_[index];
  }

 private:
  // Parser event handlers.
  void ParserInitEvent(const std::vector<scoped_refptr<StreamInfo> >& streams);
  bool NewSampleEvent(uint32 track_id,
                      const scoped_refptr<MediaSample>& sample);
  void KeyNeededEvent(MediaContainerName container,
                      scoped_ptr<uint8[]> init_data,
                      int init_data_size);

  DecryptorSource* decryptor_source_;
  std::string file_name_;
  File* media_file_;
  bool init_event_received_;
  scoped_ptr<MediaParser> parser_;
  std::vector<MediaStream*> streams_;
  scoped_ptr<uint8[]> buffer_;

  DISALLOW_COPY_AND_ASSIGN(Demuxer);
};

}

#endif  // MEDIA_BASE_DEMUXER_H_
