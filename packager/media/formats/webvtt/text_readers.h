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

namespace shaka {
namespace media {

class CharReader {
 public:
  virtual bool Next(char* out) = 0;
};

class PeekingCharReader : public CharReader {
 public:
  explicit PeekingCharReader(std::unique_ptr<CharReader> source);

  bool Next(char* out) override;
  bool Peek(char* out);

 private:
  PeekingCharReader(const PeekingCharReader&) = delete;
  PeekingCharReader operator=(const PeekingCharReader&) = delete;

  std::unique_ptr<CharReader> source_;
  char cached_next_ = 0;
  bool has_cached_next_ = false;
};

class LineReader {
 public:
  explicit LineReader(std::unique_ptr<CharReader> source);

  bool Next(std::string* out);

 private:
  LineReader(const LineReader&) = delete;
  LineReader operator=(const LineReader&) = delete;

  PeekingCharReader source_;
};

class BlockReader {
 public:
  explicit BlockReader(std::unique_ptr<CharReader> source);

  bool Next(std::vector<std::string>* out);

 private:
  BlockReader(const BlockReader&) = delete;
  BlockReader operator=(const BlockReader&) = delete;

  LineReader source_;
};

class StringCharReader : public CharReader {
 public:
  explicit StringCharReader(const std::string& str);

  bool Next(char* out) override;

 private:
  StringCharReader(const StringCharReader&) = delete;
  StringCharReader& operator=(const StringCharReader&) = delete;

  const std::string source_;
  size_t pos_ = 0;
};

}  // namespace media
}  // namespace shaka

#endif  // MEDIA_FORMATS_WEBVTT_TEXT_READERS_H_
