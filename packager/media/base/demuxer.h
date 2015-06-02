// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef MEDIA_BASE_DEMUXER_H_
#define MEDIA_BASE_DEMUXER_H_

#include <vector>

#include "packager/base/memory/ref_counted.h"
#include "packager/base/memory/scoped_ptr.h"
#include "packager/media/base/container_names.h"
#include "packager/media/base/status.h"

namespace edash_packager {
namespace media {

class Decryptor;
class File;
class KeySource;
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
  explicit Demuxer(const std::string& file_name);
  ~Demuxer();

  /// Set the KeySource for media decryption.
  /// @param key_source points to the source of decryption keys. The key
  ///        source must support fetching of keys for the type of media being
  ///        demuxed.
  void SetKeySource(scoped_ptr<KeySource> key_source);

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

  /// Cancel a demuxing job in progress. Will cause @a Run to exit with an error
  /// status of type CANCELLED.
  void Cancel();

  /// @return Streams in the media container being demuxed. The caller cannot
  ///         add or remove streams from the returned vector, but the caller is
  ///         allowed to change the internal state of the streams in the vector
  ///         through MediaStream APIs.
  const std::vector<MediaStream*>& streams() { return streams_; }

  /// @return Container name (type). Value is CONTAINER_UNKNOWN if the demuxer
  ///         is not initialized.
  MediaContainerName container_name() { return container_name_; }

 private:
  // Parser event handlers.
  void ParserInitEvent(const std::vector<scoped_refptr<StreamInfo> >& streams);
  bool NewSampleEvent(uint32_t track_id,
                      const scoped_refptr<MediaSample>& sample);

  std::string file_name_;
  File* media_file_;
  bool init_event_received_;
  Status init_parsing_status_;
  scoped_ptr<MediaParser> parser_;
  std::vector<MediaStream*> streams_;
  MediaContainerName container_name_;
  scoped_ptr<uint8_t[]> buffer_;
  scoped_ptr<KeySource> key_source_;
  bool cancelled_;

  DISALLOW_COPY_AND_ASSIGN(Demuxer);
};

}  // namespace media
}  // namespace edash_packager

#endif  // MEDIA_BASE_DEMUXER_H_
