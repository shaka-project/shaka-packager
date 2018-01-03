// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/base/muxer.h"

#include <algorithm>

#include "packager/media/base/media_sample.h"

namespace shaka {
namespace media {
namespace {
const bool kInitialEncryptionInfo = true;
}  // namespace

Muxer::Muxer(const MuxerOptions& options)
    : options_(options), cancelled_(false), clock_(NULL) {}

Muxer::~Muxer() {}

void Muxer::Cancel() {
  cancelled_ = true;
}

void Muxer::SetMuxerListener(std::unique_ptr<MuxerListener> muxer_listener) {
  muxer_listener_ = std::move(muxer_listener);
}

void Muxer::SetProgressListener(
    std::unique_ptr<ProgressListener> progress_listener) {
  progress_listener_ = std::move(progress_listener);
}

Status Muxer::Process(std::unique_ptr<StreamData> stream_data) {
  Status status;
  switch (stream_data->stream_data_type) {
    case StreamDataType::kStreamInfo:
      streams_.push_back(std::move(stream_data->stream_info));
      if (muxer_listener_ && streams_.back()->is_encrypted()) {
        const EncryptionConfig& encryption_config =
            streams_.back()->encryption_config();
        muxer_listener_->OnEncryptionInfoReady(
            kInitialEncryptionInfo, encryption_config.protection_scheme,
            encryption_config.key_id, encryption_config.constant_iv,
            encryption_config.key_system_info);
        current_key_id_ = encryption_config.key_id;
      }
      return InitializeMuxer();
    case StreamDataType::kSegmentInfo: {
      const auto& segment_info = *stream_data->segment_info;
      if (muxer_listener_ && segment_info.is_encrypted) {
        const EncryptionConfig* encryption_config =
            segment_info.key_rotation_encryption_config.get();
        // Only call OnEncryptionInfoReady again when key updates.
        if (encryption_config && encryption_config->key_id != current_key_id_) {
          muxer_listener_->OnEncryptionInfoReady(
              !kInitialEncryptionInfo, encryption_config->protection_scheme,
              encryption_config->key_id, encryption_config->constant_iv,
              encryption_config->key_system_info);
          current_key_id_ = encryption_config->key_id;
        }
        if (!encryption_started_) {
          encryption_started_ = true;
          muxer_listener_->OnEncryptionStart();
        }
      }
      return FinalizeSegment(stream_data->stream_index, segment_info);
    }
    case StreamDataType::kMediaSample:
      return AddSample(stream_data->stream_index,
                       *stream_data->media_sample);
    case StreamDataType::kCueEvent:
      if (muxer_listener_) {
        muxer_listener_->OnCueEvent(stream_data->cue_event->timestamp,
                                    stream_data->cue_event->cue_data);
      }
      break;
    default:
      VLOG(3) << "Stream data type "
              << static_cast<int>(stream_data->stream_data_type) << " ignored.";
      break;
  }
  // No dispatch for muxer.
  return Status::OK;
}

}  // namespace media
}  // namespace shaka
