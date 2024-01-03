// Copyright 2014 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd
//
// Implementation of MuxerListener that converts the info to a MediaInfo
// protobuf and dumps it to a file.
// This is specifically for VOD.

#ifndef PACKAGER_MEDIA_EVENT_VOD_MEDIA_INFO_DUMP_MUXER_LISTENER_H_
#define PACKAGER_MEDIA_EVENT_VOD_MEDIA_INFO_DUMP_MUXER_LISTENER_H_

#include <memory>
#include <string>
#include <vector>

#include <packager/macros/classes.h>
#include <packager/media/base/muxer_options.h>
#include <packager/media/event/muxer_listener.h>

namespace shaka {

class MediaInfo;

namespace media {

class VodMediaInfoDumpMuxerListener : public MuxerListener {
 public:
  VodMediaInfoDumpMuxerListener(const std::string& output_file_name, bool use_segment_list);
  ~VodMediaInfoDumpMuxerListener() override;

  /// @name MuxerListener implementation overrides.
  /// @{
  void OnEncryptionInfoReady(bool is_initial_encryption_info,
                             FourCC protection_scheme,
                             const std::vector<uint8_t>& default_key_id,
                             const std::vector<uint8_t>& iv,
                             const std::vector<ProtectionSystemSpecificInfo>&
                                 key_system_info) override;
  void OnEncryptionStart() override;
  void OnMediaStart(const MuxerOptions& muxer_options,
                    const StreamInfo& stream_info,
                    int32_t time_scale,
                    ContainerType container_type) override;
  void OnSampleDurationReady(int32_t sample_duration) override;
  void OnMediaEnd(const MediaRanges& media_ranges,
                  float duration_seconds) override;
  void OnNewSegment(const std::string& file_name,
                    int64_t start_time,
                    int64_t duration,
                    uint64_t segment_file_size) override;
  void OnKeyFrame(int64_t timestamp,
                  uint64_t start_byte_offset,
                  uint64_t size) override;
  void OnCueEvent(int64_t timestamp, const std::string& cue_data) override;
  /// @}

  /// Write @a media_info to @a output_file_path in human readable format.
  /// @param media_info is the MediaInfo to write out.
  /// @param output_file_path is the path of the output file.
  /// @return true on success, false otherwise.
  // TODO(rkuroiwa): Move this to muxer_listener_internal and rename
  // muxer_listener_internal to muxer_listener_util.
  static bool WriteMediaInfoToFile(const MediaInfo& media_info,
                                   const std::string& output_file_path);

  void set_use_segment_list(bool value) {use_segment_list_ = value;}

 private:
  std::string output_file_name_;
  std::unique_ptr<MediaInfo> media_info_;
  uint64_t max_bitrate_ = 0;

  bool is_encrypted_ = false;
  // Storage for values passed to OnEncryptionInfoReady().
  FourCC protection_scheme_;
  std::vector<uint8_t> default_key_id_;
  std::vector<ProtectionSystemSpecificInfo> key_system_info_;

  bool use_segment_list_ = false;

  DISALLOW_COPY_AND_ASSIGN(VodMediaInfoDumpMuxerListener);
};

}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_EVENT_VOD_MEDIA_INFO_DUMP_MUXER_LISTENER_H_
