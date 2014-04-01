// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_MP2T_ES_PARSER_H_
#define MEDIA_FORMATS_MP2T_ES_PARSER_H_

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"

namespace media {

class StreamParserBuffer;

namespace mp2t {

class EsParser {
 public:
  typedef base::Callback<void(scoped_refptr<StreamParserBuffer>)> EmitBufferCB;

  EsParser() {}
  virtual ~EsParser() {}

  // ES parsing.
  // Should use kNoTimestamp when a timestamp is not valid.
  virtual bool Parse(const uint8* buf, int size,
                     base::TimeDelta pts,
                     base::TimeDelta dts) = 0;

  // Flush any pending buffer.
  virtual void Flush() = 0;

  // Reset the state of the ES parser.
  virtual void Reset() = 0;
};

}  // namespace mp2t
}  // namespace media

#endif
