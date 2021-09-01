// Copyright 2019 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/event/multi_codec_muxer_listener.h"

#include "packager/base/logging.h"
#include "packager/base/strings/string_split.h"
#include "packager/media/base/stream_info.h"

namespace shaka {
namespace media {

void MultiCodecMuxerListener::OnMediaStart(const MuxerOptions& muxer_options,
                                           const StreamInfo& stream_info,
                                           int32_t time_scale,
                                           ContainerType container_type) {
  size_t num_codecs = 0;
  for (const std::string& codec_string :
       base::SplitString(stream_info.codec_string(), ";", base::KEEP_WHITESPACE,
                         base::SPLIT_WANT_NONEMPTY)) {
    std::unique_ptr<StreamInfo> current_stream_info = stream_info.Clone();
    current_stream_info->set_codec_string(codec_string);
    MuxerListener* current_muxer_listener = MuxerListenerAt(num_codecs++);
    if (!current_muxer_listener) {
      LOG(WARNING) << "'" << codec_string << "' is not handled.";
      continue;
    }
    current_muxer_listener->OnMediaStart(muxer_options, *current_stream_info,
                                         time_scale, container_type);
  }
  // We only need |num_codecs| MuxerListeners.
  LimitNumOfMuxerListners(num_codecs);
}

}  // namespace media
}  // namespace shaka
