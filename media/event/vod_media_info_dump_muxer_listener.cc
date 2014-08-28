// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "media/event/vod_media_info_dump_muxer_listener.h"

#include <google/protobuf/text_format.h>

#include "base/logging.h"
#include "media/base/muxer_options.h"
#include "media/base/stream_info.h"
#include "media/event/muxer_listener_internal.h"
#include "media/file/file.h"
#include "mpd/base/media_info.pb.h"

namespace media {
namespace event {

using dash_packager::MediaInfo;

VodMediaInfoDumpMuxerListener::VodMediaInfoDumpMuxerListener(
    const std::string& output_file_name)
    : output_file_name_(output_file_name) {}

VodMediaInfoDumpMuxerListener::~VodMediaInfoDumpMuxerListener() {}

void VodMediaInfoDumpMuxerListener::SetContentProtectionSchemeIdUri(
    const std::string& scheme_id_uri) {
  scheme_id_uri_ = scheme_id_uri;
}

void VodMediaInfoDumpMuxerListener::OnMediaStart(
    const MuxerOptions& muxer_options,
    const std::vector<StreamInfo*>& stream_infos,
    uint32 time_scale,
    ContainerType container_type,
    bool is_encrypted) {
  DCHECK(muxer_options.single_segment);
  media_info_.reset(new MediaInfo());
  if (!internal::GenerateMediaInfo(muxer_options,
                                   stream_infos,
                                   time_scale,
                                   container_type,
                                   media_info_.get())) {
    LOG(ERROR) << "Failed to generate MediaInfo from input.";
    return;
  }

  if (is_encrypted) {
    if (!internal::AddContentProtectionElements(
            container_type, scheme_id_uri_, media_info_.get())) {
      LOG(ERROR) << "Failed to add content protection elements.";
      return;
    }
  }
}

void VodMediaInfoDumpMuxerListener::OnMediaEnd(bool has_init_range,
                                               uint64 init_range_start,
                                               uint64 init_range_end,
                                               bool has_index_range,
                                               uint64 index_range_start,
                                               uint64 index_range_end,
                                               float duration_seconds,
                                               uint64 file_size) {
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

void VodMediaInfoDumpMuxerListener::OnNewSegment(uint64 start_time,
                                                 uint64 duration,
                                                 uint64 segment_file_size) {
  NOTIMPLEMENTED();
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

}  // namespace event
}  // namespace media
