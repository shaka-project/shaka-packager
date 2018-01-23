// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/status.h"

#include "packager/base/logging.h"
#include "packager/base/strings/stringprintf.h"

namespace shaka {

namespace error {
namespace {
std::string ErrorCodeToString(Code error_code) {
  switch (error_code) {
    case OK:
      return "OK";
    case UNKNOWN:
      return "UNKNOWN";
    case CANCELLED:
      return "CANCELLED";
    case INVALID_ARGUMENT:
      return "INVALID_ARGUMENT";
    case UNIMPLEMENTED:
      return "UNIMPLEMENTED";
    case FILE_FAILURE:
      return "FILE_FAILURE";
    case END_OF_STREAM:
      return "END_OF_STREAM";
    case HTTP_FAILURE:
      return "HTTP_FAILURE";
    case PARSER_FAILURE:
      return "PARSER_FAILURE";
    case ENCRYPTION_FAILURE:
      return "ENCRYPTION_FAILURE";
    case CHUNKING_ERROR:
      return "CHUNKING_ERROR";
    case MUXER_FAILURE:
      return "MUXER_FAILURE";
    case FRAGMENT_FINALIZED:
      return "FRAGMENT_FINALIZED";
    case SERVER_ERROR:
      return "SERVER_ERROR";
    case INTERNAL_ERROR:
      return "INTERNAL_ERROR";
    case STOPPED:
      return "STOPPED";
    case TIME_OUT:
      return "TIME_OUT";
    case NOT_FOUND:
      return "NOT_FOUND";
    case ALREADY_EXISTS:
      return "ALREADY_EXISTS";
    default:
      NOTIMPLEMENTED() << "Unknown Status Code: " << error_code;
      return "UNKNOWN_STATUS";
  }
}
}  // namespace
}  // namespace error

const Status Status::OK = Status(error::OK, "");
const Status Status::UNKNOWN = Status(error::UNKNOWN, "");

Status::Status(error::Code error_code, const std::string& error_message)
    : error_code_(error_code) {
  if (!ok()) {
    error_message_ = error_message;
    if (!error_message.empty())
      VLOG(1) << ToString();
  }
}

void Status::Update(Status new_status) {
  if (ok())
    *this = std::move(new_status);
}

std::string Status::ToString() const {
  if (error_code_ == error::OK)
    return "OK";

  return base::StringPrintf("%d (%s): %s",
                            error_code_,
                            error::ErrorCodeToString(error_code_).c_str(),
                            error_message_.c_str());
}

std::ostream& operator<<(std::ostream& os, const Status& x) {
  os << x.ToString();
  return os;
}

}  // namespace shaka
