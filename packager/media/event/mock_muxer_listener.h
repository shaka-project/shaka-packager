// Copyright 2016 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_EVENT_MOCK_MUXER_LISTENER_H_
#define PACKAGER_MEDIA_EVENT_MOCK_MUXER_LISTENER_H_

#include <gmock/gmock.h>

#include "packager/media/base/muxer_options.h"
#include "packager/media/base/protection_system_specific_info.h"
#include "packager/media/base/stream_info.h"
#include "packager/media/event/muxer_listener.h"

namespace edash_packager {
namespace media {

class MockMuxerListener : public MuxerListener {
 public:
  MockMuxerListener();
  ~MockMuxerListener() override;

  MOCK_METHOD4(
      OnEncryptionInfoReady,
      void(bool is_initial_encryption_info,
           const std::vector<uint8_t>& key_id,
           const std::vector<uint8_t>& iv,
           const std::vector<ProtectionSystemSpecificInfo>& key_system_info));

  MOCK_METHOD4(OnMediaStart,
               void(const MuxerOptions& muxer_options,
                    const StreamInfo& stream_info,
                    uint32_t time_scale,
                    ContainerType container_type));

  MOCK_METHOD1(OnSampleDurationReady, void(uint32_t sample_duration));

  MOCK_METHOD8(OnMediaEnd,
               void(bool has_init_range,
                    uint64_t init_range_start,
                    uint64_t init_range_end,
                    bool has_index_range,
                    uint64_t index_range_start,
                    uint64_t index_range_end,
                    float duration_seconds,
                    uint64_t file_size));

  MOCK_METHOD4(OnNewSegment,
               void(const std::string& segment_name,
                    uint64_t start_time,
                    uint64_t duration,
                    uint64_t segment_file_size));
};

}  // namespace media
}  // namespace edash_packager

#endif  // PACKAGER_MEDIA_EVENT_MOCK_MUXER_LISTENER_H_
