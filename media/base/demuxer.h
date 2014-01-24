// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

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

/// Demuxer is responsible for extracting elementary stream samples from a
/// media file, e.g. an ISO BMFF file.
class Demuxer {
 public:
  /// @param file_name specifies the input source. It uses prefix matching to
  ///        create a proper File object. The user can extend File to support
  ///        a custom File object with its own prefix.
  /// @param decryptor_source generates decryptor(s) from decryption
  ///        initialization data. It can be NULL if the media is not encrypted.
  Demuxer(const std::string& file_name, DecryptorSource* decryptor_source);
  ~Demuxer();

  /// Initialize the Demuxer. Calling other public methods of this class
  /// without this method returning OK, results in an undefined behavior.
  /// This method primes the demuxer by parsing portions of the media file to
  /// extract stream information.
  /// @return OK on success.
  Status Initialize();

  /// Drive the remuxing from demuxer side (push). Read the file and push
  /// the Data to Muxer until Eof.
  Status Run();

  /// Read from the source and send it to the parser.
  Status Parse();

  /// @return Streams in the media container being demuxed. The caller cannot
  ///         add or remove streams from the returned vector, but the caller is
  ///         allowed to change the internal state of the streams in the vector
  ///         through MediaStream APIs.
  const std::vector<MediaStream*>& streams() { return streams_; }

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

}  // namespace media

#endif  // MEDIA_BASE_DEMUXER_H_
