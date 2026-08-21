// Copyright 2014 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/media/base/muxer.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <set>
#include <string>
#include <utility>

#include <absl/log/check.h>
#include <absl/log/log.h>

#include <packager/macros/compiler.h>
#include <packager/macros/status.h>
#include <packager/media/base/encryption_config.h>
#include <packager/media/base/media_handler.h>
#include <packager/media/base/media_sample.h>
#include <packager/media/base/muxer_options.h>
#include <packager/media/base/muxer_util.h>
#include <packager/media/base/text_sample.h>
#include <packager/media/event/muxer_listener.h>
#include <packager/media/event/progress_listener.h>
#include <packager/status.h>
#include <packager/utils/clock.h>

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

bool Muxer::AllStreamInfoReceived() const {
  if (streams_.size() < num_input_streams())
    return false;
  for (const auto& stream : streams_) {
    if (!stream)
      return false;
  }
  return true;
}

Status Muxer::Process(std::unique_ptr<StreamData> stream_data) {
  Status status;
  switch (stream_data->stream_data_type) {
    case StreamDataType::kStreamInfo: {
      // |streams_| is indexed by input stream index so that it stays in sync
      // with the stream index carried by samples and segment info, which
      // matters when several input streams are multiplexed into one output.
      const size_t stream_index = stream_data->stream_index;
      if (streams_.size() <= stream_index)
        streams_.resize(stream_index + 1);
      streams_[stream_index] = std::move(stream_data->stream_info);
      return ReinitializeMuxer(kStartTime, stream_index);
    }
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
          RETURN_IF_ERROR(
              ReinitializeMuxer(scaled_time, stream_data->stream_index));
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
  flushed_streams_.insert(input_stream_index);
  RETURN_IF_ERROR(OnStreamEnded(input_stream_index));
  // With more than one input stream multiplexed into a single output, the
  // output can only be finalized once every input stream is done.
  if (flushed_streams_.size() < num_input_streams())
    return Status::OK;
  return Finalize();
}

Status Muxer::OnStreamEnded(size_t stream_id) {
  UNUSED(stream_id);
  return Status::OK;
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

Status Muxer::ReinitializeMuxer(int64_t timestamp, size_t stream_id) {
  DCHECK_LT(stream_id, streams_.size());
  DCHECK(streams_[stream_id]);
  if (muxer_listener_ && streams_[stream_id]->is_encrypted()) {
    const EncryptionConfig& encryption_config =
        streams_[stream_id]->encryption_config();
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
