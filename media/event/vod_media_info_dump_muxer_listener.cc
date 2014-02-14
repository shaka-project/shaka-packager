// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "media/event/vod_media_info_dump_muxer_listener.h"

#include "base/logging.h"
#include "media/base/muxer_options.h"
#include "media/base/stream_info.h"
#include "media/event/vod_muxer_listener_internal.h"
#include "media/file/file.h"
#include "mpd/base/media_info.pb.h"
#include "third_party/protobuf/src/google/protobuf/text_format.h"

namespace media {
namespace event {

using dash_packager::MediaInfo;

namespace {
bool IsAnyStreamEncrypted(const std::vector<StreamInfo*>& stream_infos) {
  typedef std::vector<StreamInfo*>::const_iterator Iterator;
  for (Iterator it = stream_infos.begin(); it != stream_infos.end(); ++it) {
    if ((*it)->is_encrypted())
      return true;
  }

  return false;
}
}  // namespace

VodMediaInfoDumpMuxerListener::VodMediaInfoDumpMuxerListener(File* output_file)
    : file_(output_file),
      reference_time_scale_(0),
      container_type_(kContainerUnknown) {}

VodMediaInfoDumpMuxerListener::~VodMediaInfoDumpMuxerListener() {}

void VodMediaInfoDumpMuxerListener::SetContentProtectionSchemeIdUri(
    const std::string& scheme_id_uri) {
  scheme_id_uri_ = scheme_id_uri;
}

void VodMediaInfoDumpMuxerListener::OnMediaStart(
    const MuxerOptions& muxer_options,
    const std::vector<StreamInfo*>& stream_infos,
    uint32 time_scale,
    ContainerType container_type) {
  muxer_options_ = muxer_options;
  reference_time_scale_ = time_scale;
  container_type_ = container_type;
}

void VodMediaInfoDumpMuxerListener::OnMediaEnd(
    const std::vector<StreamInfo*>& stream_infos,
    bool has_init_range,
    uint64 init_range_start,
    uint64 init_range_end,
    bool has_index_range,
    uint64 index_range_start,
    uint64 index_range_end,
    float duration_seconds,
    uint64 file_size) {
  MediaInfo media_info;
  if (!internal::GenerateMediaInfo(muxer_options_,
                                   stream_infos,
                                   has_init_range,
                                   init_range_start,
                                   init_range_end,
                                   has_index_range,
                                   index_range_start,
                                   index_range_end,
                                   duration_seconds,
                                   file_size,
                                   reference_time_scale_,
                                   container_type_,
                                   &media_info)) {
    LOG(ERROR) << "Failed to generate MediaInfo from input.";
    return;
  }

  if (IsAnyStreamEncrypted(stream_infos)) {
    if (scheme_id_uri_.empty()) {
      LOG(ERROR) << "The stream is encrypted but no schemeIdUri specified for "
                    "ContentProtection.";
      return;
    }

    MediaInfo::ContentProtectionXml* content_protection =
        media_info.add_content_protections();
    content_protection->set_scheme_id_uri(scheme_id_uri_);
  }

  SerializeMediaInfoToFile(media_info);
}

void VodMediaInfoDumpMuxerListener::OnNewSegment(uint64 start_time,
                                                 uint64 duration,
                                                 uint64 segment_file_size) {
  NOTIMPLEMENTED();
}

void VodMediaInfoDumpMuxerListener::SerializeMediaInfoToFile(
    const MediaInfo& media_info) {
  std::string output_string;
  if (!google::protobuf::TextFormat::PrintToString(media_info,
                                                   &output_string)) {
    LOG(ERROR) << "Failed to serialize MediaInfo to string.";
    return;
  }

  if (file_->Write(output_string.data(), output_string.size()) <= 0) {
    LOG(ERROR) << "Failed to write MediaInfo to file.";
    return;
  }

  file_->Flush();
}

}  // namespace event
}  // namespace media
