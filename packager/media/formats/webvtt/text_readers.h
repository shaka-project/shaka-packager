// Copyright 2017 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_FORMATS_WEBVTT_TEXT_READERS_H_
#define PACKAGER_MEDIA_FORMATS_WEBVTT_TEXT_READERS_H_

#include <memory>
#include <string>
#include <vector>

#include "packager/file/file_closer.h"
#include "packager/status.h"

namespace shaka {
class File;

namespace media {

/// Class to read character-by-character from a file.
class FileReader {
 public:
  /// Create a new file reader by opening a file. If the file fails to open (in
  /// readonly mode) a non-ok status will be returned. If the file successfully
  /// opens, |out| will be set to a new FileReader and an ok status will be
  /// returned.
  static Status Open(const std::string& filename,
                     std::unique_ptr<FileReader>* out);

  /// Read the next character from the file. If there is a next character,
  /// |out| will be set and true will be returned. If there is no next
  /// character false will be returned.
  bool Next(char* out);

 private:
  explicit FileReader(std::unique_ptr<File, FileCloser> file);

  FileReader(const FileReader& reader) = delete;
  FileReader operator=(const FileReader& reader) = delete;

  std::unique_ptr<File, FileCloser> file_;
};

class PeekingReader {
 public:
  explicit PeekingReader(std::unique_ptr<FileReader> source);

  bool Peek(char* out);
  bool Next(char* out);

 private:
  PeekingReader(const PeekingReader&) = delete;
  PeekingReader operator=(const PeekingReader&) = delete;

  std::unique_ptr<FileReader> source_;
  char cached_next_ = 0;
  bool has_cached_next_ = false;
};

class LineReader {
 public:
  explicit LineReader(std::unique_ptr<FileReader> source);

  bool Next(std::string* out);

 private:
  LineReader(const LineReader&) = delete;
  LineReader operator=(const LineReader&) = delete;

  PeekingReader source_;
};

class BlockReader {
 public:
  explicit BlockReader(std::unique_ptr<FileReader> source);

  bool Next(std::vector<std::string>* out);

 private:
  BlockReader(const BlockReader&) = delete;
  BlockReader operator=(const BlockReader&) = delete;

  LineReader source_;
};

}  // namespace media
}  // namespace shaka

#endif  // MEDIA_FORMATS_WEBVTT_TEXT_READERS_H_
