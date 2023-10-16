// Copyright 2017 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/media/formats/webvtt/text_readers.h>

#include <cstring>

#include <absl/log/check.h>
#include <absl/log/log.h>

namespace shaka {
namespace media {

LineReader::LineReader() : should_flush_(false) {}

void LineReader::PushData(const uint8_t* data, size_t data_size) {
  buffer_.Push(data, static_cast<int>(data_size));
  should_flush_ = false;
}

// Split lines based on https://w3c.github.io/webvtt/#webvtt-line-terminator
bool LineReader::Next(std::string* out) {
  DCHECK(out);

  int i;
  int skip = 0;
  const uint8_t* data;
  int data_size;
  buffer_.Peek(&data, &data_size);
  for (i = 0; i < data_size; i++) {
    // Handle \n
    if (data[i] == '\n') {
      skip = 1;
      break;
    }

    // Handle \r and \r\n
    if (data[i] == '\r') {
      // Only read if we can see the next character; this ensures we don't get
      // the '\n' in the next PushData.
      if (i + 1 == data_size) {
        if (!should_flush_)
          return false;
        skip = 1;
      } else {
        if (data[i + 1] == '\n')
          skip = 2;
        else
          skip = 1;
      }
      break;
    }
  }

  if (i == data_size && (!should_flush_ || i == 0)) {
    return false;
  }

  // TODO(modmaker): Handle character encodings?
  out->assign(data, data + i);
  buffer_.Pop(i + skip);
  return true;
}

void LineReader::Flush() {
  should_flush_ = true;
}

BlockReader::BlockReader() : should_flush_(false) {}

void BlockReader::PushData(const uint8_t* data, size_t data_size) {
  source_.PushData(data, data_size);
  should_flush_ = false;
}

bool BlockReader::Next(std::vector<std::string>* out) {
  DCHECK(out);

  bool end_block = false;
  // Read through lines until a non-empty line is found. With a non-empty
  // line is found, start adding the lines to the output and once an empty
  // line if found again, stop adding lines and exit.
  std::string line;
  while (source_.Next(&line)) {
    if (!temp_.empty() && line.empty()) {
      end_block = true;
      break;
    }
    if (!line.empty()) {
      temp_.emplace_back(std::move(line));
    }
  }

  if (!end_block && (!should_flush_ || temp_.empty()))
    return false;

  *out = std::move(temp_);
  return true;
}

void BlockReader::Flush() {
  source_.Flush();
  should_flush_ = true;
}

}  // namespace media
}  // namespace shaka
