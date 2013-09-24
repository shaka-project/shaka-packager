// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_LOCATION_H_
#define TOOLS_GN_LOCATION_H_

#include <algorithm>

#include "base/logging.h"

class InputFile;

// Represents a place in a source file. Used for error reporting.
class Location {
 public:
  Location()
      : file_(NULL),
        line_number_(-1),
        char_offset_(-1) {
  }
  Location(const InputFile* file, int line_number, int char_offset)
      : file_(file),
        line_number_(line_number),
        char_offset_(char_offset) {
  }

  const InputFile* file() const { return file_; }
  int line_number() const { return line_number_; }
  int char_offset() const { return char_offset_; }

  bool operator==(const Location& other) const {
    return other.file_ == file_ &&
           other.line_number_ == line_number_ &&
           other.char_offset_ == char_offset_;
  }

  bool operator<(const Location& other) const {
    DCHECK(file_ == other.file_);
    if (line_number_ != other.line_number_)
      return line_number_ < other.line_number_;
    return char_offset_ < other.char_offset_;
  }

 private:
  const InputFile* file_;  // Null when unset.
  int line_number_;  // -1 when unset.
  int char_offset_;  // -1 when unset.
};

// Represents a range in a source file. Used for error reporting.
// The end is exclusive i.e. [begin, end)
class LocationRange {
 public:
  LocationRange() {}
  LocationRange(const Location& begin, const Location& end)
      : begin_(begin),
        end_(end) {
    DCHECK(begin_.file() == end_.file());
  }

  const Location& begin() const { return begin_; }
  const Location& end() const { return end_; }

  LocationRange Union(const LocationRange& other) const {
    DCHECK(begin_.file() == other.begin_.file());
    return LocationRange(
        begin_ < other.begin_ ? begin_ : other.begin_,
        end_ < other.end_ ? other.end_ : end_);
  }

 private:
  Location begin_;
  Location end_;
};

#endif  // TOOLS_GN_LOCATION_H_
