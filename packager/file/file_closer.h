// Copyright 2014 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef MEDIA_FILE_FILE_CLOSER_H_
#define MEDIA_FILE_FILE_CLOSER_H_

#include <absl/log/log.h>

#include <packager/file.h>

namespace shaka {

/// Used by std::unique_ptr to automatically close the file when it goes out of
/// scope.
struct FileCloser {
  inline void operator()(File* file) const {
    if (file != NULL) {
      const std::string filename = file->file_name();
      if (!file->Close()) {
        LOG(WARNING) << "Failed to close the file properly: " << filename;
      }
    }
  }
};

}  // namespace shaka

#endif  // MEDIA_FILE_FILE_CLOSER_H_
