// Copyright 2014 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_PUBLIC_STATUS_H_
#define PACKAGER_PUBLIC_STATUS_H_

#include <iostream>
#include <string>

#include <packager/export.h>

namespace shaka {

namespace error {

/// Error codes for the packager APIs.
enum Code {
  // Not an error; returned on success
  OK,

  // Unknown error.  An example of where this error may be returned is
  // errors raised by APIs that do not return enough error information
  // may be converted to this error.
  UNKNOWN,

  // The operation was cancelled (typically by the caller).
  CANCELLED,

  // Client specified an invalid argument. INVALID_ARGUMENT indicates
  // arguments that are problematic regardless of the state of the system
  // (e.g. a malformed file name).
  INVALID_ARGUMENT,

  // Operation is not implemented or not supported/enabled.
  UNIMPLEMENTED,

  // Cannot open file.
  FILE_FAILURE,

  // End of stream.
  END_OF_STREAM,

  // Failure to get HTTP response successfully,
  HTTP_FAILURE,

  // Unable to parse the media file.
  PARSER_FAILURE,

  // Failed to do the encryption.
  ENCRYPTION_FAILURE,

  // Error when trying to do chunking.
  CHUNKING_ERROR,

  // Fail to mux the media file.
  MUXER_FAILURE,

  // This track fragment is finalized.
  FRAGMENT_FINALIZED,

  // Server errors. Receives malformed response from server.
  SERVER_ERROR,

  // Internal errors. Some invariants have been broken.
  INTERNAL_ERROR,

  // The operation was stopped.
  STOPPED,

  // The operation timed out.
  TIME_OUT,

  // Value was not found.
  NOT_FOUND,

  // The entity that a client attempted to create (e.g., file or directory)
  // already exists.
  ALREADY_EXISTS,

  // Error when trying to generate trick play stream.
  TRICK_PLAY_ERROR,
};

}  // namespace error

class SHAKA_EXPORT Status {
 public:
  /// Creates a "successful" status.
  Status() : error_code_(error::OK) {}

  /// Create a status with the specified code, and error message.
  /// If "error_code == error::OK", error_message is ignored and a Status
  /// object identical to Status::OK is constructed.
  Status(error::Code error_code, const std::string& error_message);

  /// @name Some pre-defined Status objects.
  /// @{
  static const Status OK;  // Identical to 0-arg constructor.
  static const Status UNKNOWN;
  /// @}

  /// If "ok()", stores "new_status" into *this.  If "!ok()", preserves
  /// the current "error_code()/error_message()",
  ///
  /// Convenient way of keeping track of the first error encountered.
  /// Instead of:
  ///   if (overall_status.ok()) overall_status = new_status
  /// Use:
  ///   overall_status.Update(new_status);
  void Update(Status new_status);

  bool ok() const { return error_code_ == error::OK; }
  error::Code error_code() const { return error_code_; }
  const std::string& error_message() const { return error_message_; }

  bool operator==(const Status& x) const {
    return error_code_ == x.error_code() && error_message_ == x.error_message();
  }
  bool operator!=(const Status& x) const { return !(*this == x); }

  /// @return A combination of the error code name and message.
  std::string ToString() const;

 private:
  error::Code error_code_;
  std::string error_message_;

  // Not using DISALLOW_COPY_AND_ASSIGN here intentionally to allow the compiler
  // generated copy constructor and assignment operator.
};

std::ostream& operator<<(std::ostream& os, const Status& x);

}  // namespace shaka

#endif  // PACKAGER_PUBLIC_STATUS_H_
