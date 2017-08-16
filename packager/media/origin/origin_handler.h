// Copyright 2017 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_ORIGIN_ORIGIN_HANDLER_H_
#define PACKAGER_MEDIA_ORIGIN_ORIGIN_HANDLER_H_

#include "packager/media/base/media_handler.h"

namespace shaka {
namespace media {

// Origin handlers are handlers that sit at the head of a pipeline (chain of
// handlers). They are expect to take input from an alternative source (like
// a file or network connection).
class OriginHandler : public MediaHandler {
 public:
  OriginHandler() = default;

  // Process all data and send messages down stream. This is the main
  // method of the handler. Since origin handlers do not take input via
  // |Process|, run will take input from an alternative source. This call
  // is expect to be blocking. To exit a call to |Run|, |Cancel| should
  // be used.
  virtual Status Run() = 0;

  // Non-blocking call to the handler, requesting that it exit the
  // current call to |Run|. The handler should stop processing data
  // as soon is convenient.
  virtual void Cancel() = 0;

 private:
  OriginHandler(const OriginHandler&) = delete;
  OriginHandler& operator=(const OriginHandler&) = delete;

  Status Process(std::unique_ptr<StreamData> stream_data) override;
};

}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_ORIGIN_ORIGIN_HANDLER_H_
