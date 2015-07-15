// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/event/vod_media_info_dump_muxer_listener.h"

#include <google/protobuf/text_format.h>

#include "packager/base/logging.h"
#include "packager/media/base/muxer_options.h"
#include "packager/media/base/stream_info.h"
#include "packager/media/event/muxer_listener_internal.h"
#include "packager/media/file/file.h"
#include "packager/mpd/base/media_info.pb.h"

namespace edash_packager {
namespace media {

VodMediaInfoDumpMuxerListener::VodMediaInfoDumpMuxerListener(
    const std::string& output_file_name)
    : output_file_name_(output_file_name), is_encrypted_(false) {}

VodMediaInfoDumpMuxerListener::~VodMediaInfoDumpMuxerListener() {}

void VodMediaInfoDumpMuxerListener::SetContentProtectionSchemeIdUri(
    const std::string& scheme_id_uri) {
  scheme_id_uri_ = scheme_id_uri;
}

void VodMediaInfoDumpMuxerListener::OnEncryptionInfoReady(
    const std::string& content_protection_uuid,
    const std::string& content_protection_name_version,
    const std::vector<uint8_t>& default_key_id,
    const std::vector<uint8_t>& pssh) {
  content_protection_uuid_ = content_protection_uuid;
  content_protection_name_version_ = content_protection_name_version;
  default_key_id_.assign(default_key_id.begin(), default_key_id.end());
  pssh_.assign(pssh.begin(), pssh.end());
  is_encrypted_ = true;
}

void VodMediaInfoDumpMuxerListener::OnMediaStart(
    const MuxerOptions& muxer_options,
    const StreamInfo& stream_info,
    uint32_t time_scale,
    ContainerType container_type) {
  DCHECK(muxer_options.single_segment);
  media_info_.reset(new MediaInfo());
  if (!internal::GenerateMediaInfo(muxer_options,
                                   stream_info,
                                   time_scale,
                                   container_type,
                                   media_info_.get())) {
    LOG(ERROR) << "Failed to generate MediaInfo from input.";
    return;
  }

  if (is_encrypted_) {
    internal::SetContentProtectionFields(
        content_protection_uuid_, content_protection_name_version_,
        default_key_id_, pssh_, media_info_.get());
  }
}

void VodMediaInfoDumpMuxerListener::OnSampleDurationReady(
    uint32_t sample_duration) {
  // Assume one VideoInfo.
  if (media_info_->has_video_info()) {
    media_info_->mutable_video_info()->set_frame_duration(sample_duration);
  }
}

void VodMediaInfoDumpMuxerListener::OnMediaEnd(bool has_init_range,
                                               uint64_t init_range_start,
                                               uint64_t init_range_end,
                                               bool has_index_range,
                                               uint64_t index_range_start,
                                               uint64_t index_range_end,
                                               float duration_seconds,
                                               uint64_t file_size) {
  DCHECK(media_info_);
  if (!internal::SetVodInformation(has_init_range,
                                   init_range_start,
                                   init_range_end,
                                   has_index_range,
                                   index_range_start,
                                   index_range_end,
                                   duration_seconds,
                                   file_size,
                                   media_info_.get())) {
    LOG(ERROR) << "Failed to generate VOD information from input.";
    return;
  }
  SerializeMediaInfoToFile();
}

void VodMediaInfoDumpMuxerListener::OnNewSegment(uint64_t start_time,
                                                 uint64_t duration,
                                                 uint64_t segment_file_size) {
}

bool VodMediaInfoDumpMuxerListener::SerializeMediaInfoToFile() {
  std::string output_string;
  if (!google::protobuf::TextFormat::PrintToString(*media_info_,
                                                   &output_string)) {
    LOG(ERROR) << "Failed to serialize MediaInfo to string.";
    return false;
  }

  media::File* file = File::Open(output_file_name_.c_str(), "w");
  if (!file) {
    LOG(ERROR) << "Failed to open " << output_file_name_;
    return false;
  }
  if (file->Write(output_string.data(), output_string.size()) <= 0) {
    LOG(ERROR) << "Failed to write MediaInfo to file.";
    file->Close();
    return false;
  }
  if (!file->Close()) {
    LOG(ERROR) << "Failed to close " << output_file_name_;
    return false;
  }
  return true;
}

}  // namespace media
}  // namespace edash_packager
