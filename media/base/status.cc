// Copyright (c) 2013 Google Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/status.h"

#include <sstream>

namespace media {

const Status& Status::OK = Status(error::OK, "");
const Status& Status::UNKNOWN = Status(error::UNKNOWN, "");

std::string Status::ToString() const {
  if (error_code_ == error::OK)
    return "OK";

  std::ostringstream string_stream;
  string_stream << error_code_ << ":" << error_message_;
  return string_stream.str();
}

std::ostream& operator<<(std::ostream& os, const Status& x) {
  os << x.ToString();
  return os;
}

}  // namespace media
