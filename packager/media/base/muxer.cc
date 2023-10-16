// Copyright 2014 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/media/base/muxer.h>

#include <algorithm>
#include <chrono>

#include <packager/macros/compiler.h>
#include <packager/macros/logging.h>
#include <packager/macros/status.h>
#include <packager/media/base/media_sample.h>
#include <packager/media/base/muxer_util.h>

namespace shaka {
namespace media {
namespace {
const bool kInitialEncryptionInfo = true;
const int64_t kStartTime = 0;
}  // namespace

Muxer::Muxer(const MuxerOptions& options)
    : options_(options), clock_(new Clock) {
  // "$" is only allowed if the output file name is a template, which is used to
  // support one file per Representation per Period when there are Ad Cues.
  if (options_.output_file_name.find("$") != std::string::npos)
    output_file_template_ = options_.output_file_name;
}

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
      return ReinitializeMuxer(kStartTime);
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
      return AddMediaSample(stream_data->stream_index,
                            *stream_data->media_sample);
    case StreamDataType::kTextSample:
      return AddTextSample(stream_data->stream_index,
                           *stream_data->text_sample);
    case StreamDataType::kCueEvent:
      if (muxer_listener_) {
        const int64_t time_scale =
            streams_[stream_data->stream_index]->time_scale();
        const double time_in_seconds = stream_data->cue_event->time_in_seconds;
        const int64_t scaled_time =
            static_cast<int64_t>(time_in_seconds * time_scale);
        muxer_listener_->OnCueEvent(scaled_time,
                                    stream_data->cue_event->cue_data);

        // Finalize and re-initialize Muxer to generate different content files.
        if (!output_file_template_.empty()) {
          RETURN_IF_ERROR(Finalize());
          RETURN_IF_ERROR(ReinitializeMuxer(scaled_time));
        }
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

Status Muxer::OnFlushRequest(size_t input_stream_index) {
  UNUSED(input_stream_index);
  return Finalize();
}

Status Muxer::AddMediaSample(size_t stream_id, const MediaSample& sample) {
  UNUSED(stream_id);
  UNUSED(sample);
  return Status::OK;
}

Status Muxer::AddTextSample(size_t stream_id, const TextSample& sample) {
  UNUSED(stream_id);
  UNUSED(sample);
  return Status::OK;
}

Status Muxer::ReinitializeMuxer(int64_t timestamp) {
  if (muxer_listener_ && streams_.back()->is_encrypted()) {
    const EncryptionConfig& encryption_config =
        streams_.back()->encryption_config();
    muxer_listener_->OnEncryptionInfoReady(
        kInitialEncryptionInfo, encryption_config.protection_scheme,
        encryption_config.key_id, encryption_config.constant_iv,
        encryption_config.key_system_info);
    current_key_id_ = encryption_config.key_id;
  }
  if (!output_file_template_.empty()) {
    // Update |output_file_name| with an actual file name, which will be used by
    // the subclasses.
    options_.output_file_name =
        GetSegmentName(output_file_template_, timestamp, output_file_index_++,
                       options_.bandwidth);
  }
  return InitializeMuxer();
}

}  // namespace media
}  // namespace shaka
