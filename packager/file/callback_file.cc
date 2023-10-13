// Copyright 2017 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/file/callback_file.h>

#include <absl/log/log.h>

#include <packager/macros/compiler.h>
#include <packager/macros/logging.h>

namespace shaka {

CallbackFile::CallbackFile(const char* file_name, const char* mode)
    : File(file_name), file_mode_(mode) {}

CallbackFile::~CallbackFile() {}

bool CallbackFile::Close() {
  delete this;
  return true;
}

int64_t CallbackFile::Read(void* buffer, uint64_t length) {
  if (!callback_params_->read_func) {
    LOG(ERROR) << "Read function not defined.";
    return -1;
  }
  return callback_params_->read_func(name_, buffer, length);
}

int64_t CallbackFile::Write(const void* buffer, uint64_t length) {
  if (!callback_params_->write_func) {
    LOG(ERROR) << "Write function not defined.";
    return -1;
  }
  return callback_params_->write_func(name_, buffer, length);
}

void CallbackFile::CloseForWriting() {}

int64_t CallbackFile::Size() {
  LOG(INFO) << "CallbackFile does not support Size().";
  return -1;
}

bool CallbackFile::Flush() {
  // Do nothing on Flush.
  return true;
}

bool CallbackFile::Seek(uint64_t position) {
  UNUSED(position);
  VLOG(1) << "CallbackFile does not support Seek().";
  return false;
}

bool CallbackFile::Tell(uint64_t* position) {
  UNUSED(position);
  VLOG(1) << "CallbackFile does not support Tell().";
  return false;
}

bool CallbackFile::Open() {
  if (file_mode_ != "r" && file_mode_ != "w" && file_mode_ != "rb" &&
      file_mode_ != "wb") {
    LOG(ERROR) << "CallbackFile does not support file mode " << file_mode_;
    return false;
  }
  return ParseCallbackFileName(file_name(), &callback_params_, &name_);
}

}  // namespace shaka
