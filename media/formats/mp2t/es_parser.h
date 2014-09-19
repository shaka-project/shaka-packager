// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_MP2T_ES_PARSER_H_
#define MEDIA_FORMATS_MP2T_ES_PARSER_H_

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/ref_counted.h"

namespace edash_packager {
namespace media {

class MediaSample;
class StreamInfo;

namespace mp2t {

class EsParser {
 public:
  typedef base::Callback<void(scoped_refptr<StreamInfo>&)> NewStreamInfoCB;
  typedef base::Callback<void(uint32, scoped_refptr<MediaSample>&)> EmitSampleCB;

  EsParser(uint32 pid)
      : pid_(pid) {}
  virtual ~EsParser() {}

  // ES parsing.
  // Should use kNoTimestamp when a timestamp is not valid.
  virtual bool Parse(const uint8* buf, int size, int64 pts, int64 dts) = 0;

  // Flush any pending buffer.
  virtual void Flush() = 0;

  // Reset the state of the ES parser.
  virtual void Reset() = 0;

  uint32 pid() { return pid_; }

 private:
  uint32 pid_;
};

}  // namespace mp2t
}  // namespace media
}  // namespace edash_packager

#endif
