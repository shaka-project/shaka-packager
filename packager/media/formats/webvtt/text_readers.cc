// Copyright 2017 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/formats/webvtt/text_readers.h"

#include "packager/base/logging.h"

namespace shaka {
namespace media {

PeekingCharReader::PeekingCharReader(std::unique_ptr<CharReader> source)
    : source_(std::move(source)) {}

bool PeekingCharReader::Next(char* out) {
  DCHECK(out);
  if (Peek(out)) {
    has_cached_next_ = false;
    return true;
  }
  return false;
}

bool PeekingCharReader::Peek(char* out) {
  DCHECK(out);
  if (!has_cached_next_ && source_->Next(&cached_next_)) {
    has_cached_next_ = true;
  }
  if (has_cached_next_) {
    *out = cached_next_;
    return true;
  }
  return false;
}

LineReader::LineReader(std::unique_ptr<CharReader> source)
    : source_(std::move(source)) {}

// Split lines based on https://w3c.github.io/webvtt/#webvtt-line-terminator
bool LineReader::Next(std::string* out) {
  DCHECK(out);
  out->clear();
  bool read_something = false;
  char now;
  while (source_.Next(&now)) {
    read_something = true;
    // handle \n
    if (now == '\n') {
      break;
    }
    // handle \r and \r\n
    if (now == '\r') {
      char next;
      if (source_.Peek(&next) && next == '\n') {
        source_.Next(&next);  // Read in the '\n' that was just seen via |Peek|
      }
      break;
    }
    out->push_back(now);
  }
  return read_something;
}

BlockReader::BlockReader(std::unique_ptr<CharReader> source)
    : source_(std::move(source)) {}

bool BlockReader::Next(std::vector<std::string>* out) {
  DCHECK(out);

  out->clear();

  bool in_block = false;

  // Read through lines until a non-empty line is found. With a non-empty
  // line is found, start adding the lines to the output and once an empty
  // line if found again, stop adding lines and exit.
  std::string line;
  while (source_.Next(&line)) {
    if (in_block && line.empty()) {
      break;
    }
    if (in_block || !line.empty()) {
      out->push_back(line);
      in_block = true;
    }
  }

  return in_block;
}

StringCharReader::StringCharReader(const std::string& str) : source_(str) {}

bool StringCharReader::Next(char* out) {
  if (pos_ < source_.length()) {
    *out = source_[pos_++];
    return true;
  }
  return false;
}
}  // namespace media
}  // namespace shaka
