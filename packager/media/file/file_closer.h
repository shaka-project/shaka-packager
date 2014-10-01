// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef MEDIA_FILE_FILE_CLOSER_H_
#define MEDIA_FILE_FILE_CLOSER_H_

#include "packager/base/logging.h"
#include "packager/media/file/file.h"

namespace edash_packager {
namespace media {

/// Used by scoped_ptr to automatically close the file when it goes out of
/// scope.
struct FileCloser {
  inline void operator()(File* file) const {
    if (file != NULL && !file->Close()) {
      LOG(WARNING) << "Failed to close the file properly: "
                   << file->file_name();
    }
  }
};

}  // namespace media
}  // namespace edash_packager

#endif  // MEDIA_FILE_FILE_CLOSER_H_
