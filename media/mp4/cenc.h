// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MP4_CENC_H_
#define MEDIA_MP4_CENC_H_

#include <vector>

#include "base/basictypes.h"
#include "media/base/decrypt_config.h"

namespace media {
namespace mp4 {

class BufferReader;

struct FrameCENCInfo {
  uint8 iv[16];
  std::vector<SubsampleEntry> subsamples;

  FrameCENCInfo();
  ~FrameCENCInfo();
  bool Parse(int iv_size, BufferReader* r);
  size_t GetTotalSizeOfSubsamples() const;
};


}  // namespace mp4
}  // namespace media

#endif  // MEDIA_MP4_CENC_H_
