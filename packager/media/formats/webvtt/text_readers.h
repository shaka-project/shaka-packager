// Copyright 2017 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_FORMATS_WEBVTT_TEXT_READERS_H_
#define PACKAGER_MEDIA_FORMATS_WEBVTT_TEXT_READERS_H_

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <packager/media/base/byte_queue.h>
#include <packager/status.h>

namespace shaka {
class File;

namespace media {

class LineReader {
 public:
  LineReader();

  /// Pushes data onto the end of the buffer.
  void PushData(const uint8_t* data, size_t data_size);
  /// Reads the next line from the buffer.
  /// @return True if a line is read, false if there's no line in the buffer.
  bool Next(std::string* out);
  /// Indicates that no more data is coming and that calls to Next should
  /// return even possibly-incomplete data.
  void Flush();

 private:
  LineReader(const LineReader&) = delete;
  LineReader operator=(const LineReader&) = delete;

  ByteQueue buffer_;
  bool should_flush_;
};

class BlockReader {
 public:
  BlockReader();

  /// Pushes data onto the end of the buffer.
  void PushData(const uint8_t* data, size_t data_size);
  /// Reads the next block from the buffer.
  /// @return True if a block is read, false if there is no block in the buffer.
  bool Next(std::vector<std::string>* out);
  /// Indicates that no more data is coming and that calls to Next should
  /// return even possibly-incomplete data.
  void Flush();

 private:
  BlockReader(const BlockReader&) = delete;
  BlockReader operator=(const BlockReader&) = delete;

  LineReader source_;
  std::vector<std::string> temp_;
  bool should_flush_;
};

}  // namespace media
}  // namespace shaka

#endif  // MEDIA_FORMATS_WEBVTT_TEXT_READERS_H_
