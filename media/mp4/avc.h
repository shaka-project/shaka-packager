// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MP4_AVC_H_
#define MEDIA_MP4_AVC_H_

#include <vector>

#include "base/basictypes.h"
#include "media/base/media_export.h"

namespace media {
namespace mp4 {

struct AVCDecoderConfigurationRecord;

class MEDIA_EXPORT AVC {
 public:
  static bool ConvertFrameToAnnexB(int length_size, std::vector<uint8>* buffer);

  static bool ConvertConfigToAnnexB(
      const AVCDecoderConfigurationRecord& avc_config,
      std::vector<uint8>* buffer);
};

}  // namespace mp4
}  // namespace media

#endif  // MEDIA_MP4_AVC_H_
