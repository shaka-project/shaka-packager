// Copyright 2017 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/origin/origin_handler.h"

namespace shaka {
namespace media {

// Origin handlers are always at the start of a pipeline (chain or handlers)
// and therefore should never receive input via |Process|.
Status OriginHandler::Process(std::unique_ptr<StreamData> stream_data) {
  return Status(error::INTERNAL_ERROR,
                "An origin handlers should never be a downstream handler.");
}

}  // namespace media
}  // namespace shaka
