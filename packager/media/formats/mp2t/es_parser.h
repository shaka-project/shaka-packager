// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PACKAGER_MEDIA_FORMATS_MP2T_ES_PARSER_H_
#define PACKAGER_MEDIA_FORMATS_MP2T_ES_PARSER_H_

#include <stdint.h>

#include "packager/base/callback.h"

namespace shaka {
namespace media {

class MediaSample;
class StreamInfo;

namespace mp2t {

class EsParser {
 public:
  typedef base::Callback<void(const std::shared_ptr<StreamInfo>&)>
      NewStreamInfoCB;
  typedef base::Callback<void(uint32_t, const std::shared_ptr<MediaSample>&)>
      EmitSampleCB;

  EsParser(uint32_t pid) : pid_(pid) {}
  virtual ~EsParser() {}

  // ES parsing.
  // Should use kNoTimestamp when a timestamp is not valid.
  virtual bool Parse(const uint8_t* buf,
                     int size,
                     int64_t pts,
                     int64_t dts) = 0;

  // Flush any pending buffer.
  virtual void Flush() = 0;

  // Reset the state of the ES parser.
  virtual void Reset() = 0;

  uint32_t pid() { return pid_; }

 private:
  uint32_t pid_;
};

}  // namespace mp2t
}  // namespace media
}  // namespace shaka

#endif
