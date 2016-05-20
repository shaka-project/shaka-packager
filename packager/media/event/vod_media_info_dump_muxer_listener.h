// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd
//
// Implementation of MuxerListener that converts the info to a MediaInfo
// protobuf and dumps it to a file.
// This is specifically for VOD.

#ifndef MEDIA_EVENT_VOD_MEDIA_INFO_DUMP_MUXER_LISTENER_H_
#define MEDIA_EVENT_VOD_MEDIA_INFO_DUMP_MUXER_LISTENER_H_

#include <string>
#include <vector>

#include "packager/base/compiler_specific.h"
#include "packager/base/memory/scoped_ptr.h"
#include "packager/media/base/muxer_options.h"
#include "packager/media/event/muxer_listener.h"

namespace shaka {

class MediaInfo;

namespace media {

class VodMediaInfoDumpMuxerListener : public MuxerListener {
 public:
  VodMediaInfoDumpMuxerListener(const std::string& output_file_name);
  ~VodMediaInfoDumpMuxerListener() override;

  /// @name MuxerListener implementation overrides.
  /// @{
  void OnEncryptionInfoReady(bool is_initial_encryption_info,
                             FourCC protection_scheme,
                             const std::vector<uint8_t>& default_key_id,
                             const std::vector<uint8_t>& iv,
                             const std::vector<ProtectionSystemSpecificInfo>&
                                 key_system_info) override;
  void OnMediaStart(const MuxerOptions& muxer_options,
                    const StreamInfo& stream_info,
                    uint32_t time_scale,
                    ContainerType container_type) override;
  void OnSampleDurationReady(uint32_t sample_duration) override;
  void OnMediaEnd(bool has_init_range,
                  uint64_t init_range_start,
                  uint64_t init_range_end,
                  bool has_index_range,
                  uint64_t index_range_start,
                  uint64_t index_range_end,
                  float duration_seconds,
                  uint64_t file_size) override;
  void OnNewSegment(const std::string& file_name,
                    uint64_t start_time,
                    uint64_t duration,
                    uint64_t segment_file_size) override;
  /// @}

  /// Write @a media_info to @a output_file_path in human readable format.
  /// @param media_info is the MediaInfo to write out.
  /// @param output_file_path is the path of the output file.
  /// @return true on success, false otherwise.
  // TODO(rkuroiwa): Move this to muxer_listener_internal and rename
  // muxer_listener_internal to muxer_listener_util.
  static bool WriteMediaInfoToFile(const MediaInfo& media_info,
                                   const std::string& output_file_path);

 private:

  std::string output_file_name_;
  scoped_ptr<MediaInfo> media_info_;

  bool is_encrypted_;
  // Storage for values passed to OnEncryptionInfoReady().
  FourCC protection_scheme_;
  std::string default_key_id_;
  std::vector<ProtectionSystemSpecificInfo> key_system_info_;

  DISALLOW_COPY_AND_ASSIGN(VodMediaInfoDumpMuxerListener);
};

}  // namespace media
}  // namespace shaka

#endif  // MEDIA_EVENT_VOD_MEDIA_INFO_DUMP_MUXER_LISTENER_H_
