// Copyright 2014 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/media/demuxer/demuxer.h>

#include <algorithm>
#include <functional>

#include <absl/log/check.h>
#include <absl/log/log.h>
#include <absl/strings/escaping.h>
#include <absl/strings/numbers.h>
#include <absl/strings/str_format.h>

#include <packager/file.h>
#include <packager/macros/compiler.h>
#include <packager/macros/logging.h>
#include <packager/media/base/decryptor_source.h>
#include <packager/media/base/key_source.h>
#include <packager/media/base/media_sample.h>
#include <packager/media/base/stream_info.h>
#include <packager/media/formats/mp2t/mp2t_media_parser.h>
#include <packager/media/formats/mp4/mp4_media_parser.h>
#include <packager/media/formats/webm/webm_media_parser.h>
#include <packager/media/formats/webvtt/webvtt_parser.h>
#include <packager/media/formats/wvm/wvm_media_parser.h>

namespace {
// 65KB, sufficient to determine the container and likely all init data.
const size_t kInitBufSize = 0x10000;
const size_t kBufSize = 0x200000;  // 2MB
// Maximum number of allowed queued samples. If we are receiving a lot of
// samples before seeing init_event, something is not right. The number
// set here is arbitrary though.
const size_t kQueuedSamplesLimit = 10000;
const size_t kInvalidStreamIndex = static_cast<size_t>(-1);
const size_t kBaseVideoOutputStreamIndex = 0x100;
const size_t kBaseAudioOutputStreamIndex = 0x200;
const size_t kBaseTextOutputStreamIndex = 0x300;

std::string GetStreamLabel(size_t stream_index) {
  switch (stream_index) {
    case kBaseVideoOutputStreamIndex:
      return "video";
    case kBaseAudioOutputStreamIndex:
      return "audio";
    case kBaseTextOutputStreamIndex:
      return "text";
    default:
      return absl::StrFormat("%u", stream_index);
  }
}

bool GetStreamIndex(const std::string& stream_label, size_t* stream_index) {
  DCHECK(stream_index);
  if (stream_label == "video") {
    *stream_index = kBaseVideoOutputStreamIndex;
  } else if (stream_label == "audio") {
    *stream_index = kBaseAudioOutputStreamIndex;
  } else if (stream_label == "text") {
    *stream_index = kBaseTextOutputStreamIndex;
  } else {
    // Expect stream_label to be a zero based stream id.
    if (!absl::SimpleAtoi(stream_label, stream_index)) {
      LOG(ERROR) << "Invalid argument --stream=" << stream_label << "; "
                 << "should be 'audio', 'video', 'text', or a number";
      return false;
    }
  }
  return true;
}

}

namespace shaka {
namespace media {

Demuxer::Demuxer(const std::string& file_name)
    : file_name_(file_name), buffer_(new uint8_t[kBufSize]) {}

Demuxer::~Demuxer() {
  if (media_file_)
    media_file_->Close();
}

void Demuxer::SetKeySource(std::unique_ptr<KeySource> key_source) {
  key_source_ = std::move(key_source);
}

Status Demuxer::Run() {
  LOG(INFO) << "Demuxer::Run() on file '" << file_name_ << "'.";
  Status status = InitializeParser();
  // ParserInitEvent callback is called after a few calls to Parse(), which sets
  // up the streams. Only after that, we can verify the outputs below.
  while (!all_streams_ready_ && status.ok())
    status.Update(Parse());
  // If no output is defined, then return success after receiving all stream
  // info.
  if (all_streams_ready_ && output_handlers().empty())
    return Status::OK;
  if (!init_event_status_.ok())
    return init_event_status_;
  if (!status.ok())
    return status;
  // Check if all specified outputs exists.
  for (const auto& pair : output_handlers()) {
    if (std::find(stream_indexes_.begin(), stream_indexes_.end(), pair.first) ==
        stream_indexes_.end()) {
      LOG(ERROR) << "Invalid argument, stream=" << GetStreamLabel(pair.first)
                 << " not available.";
      return Status(error::INVALID_ARGUMENT, "Stream not available");
    }
  }

  while (!cancelled_ && status.ok())
    status.Update(Parse());
  if (cancelled_ && status.ok())
    return Status(error::CANCELLED, "Demuxer run cancelled");

  if (status.error_code() == error::END_OF_STREAM) {
    for (size_t stream_index : stream_indexes_) {
      status = FlushDownstream(stream_index);
      if (!status.ok())
        return status;
    }
    return Status::OK;
  }
  return status;
}

void Demuxer::Cancel() {
  cancelled_ = true;
}

Status Demuxer::SetHandler(const std::string& stream_label,
                           std::shared_ptr<MediaHandler> handler) {
  size_t stream_index = kInvalidStreamIndex;
  if (!GetStreamIndex(stream_label, &stream_index)) {
    return Status(error::INVALID_ARGUMENT,
                  "Invalid stream: " + stream_label);
  }
  return MediaHandler::SetHandler(stream_index, std::move(handler));
}

void Demuxer::SetLanguageOverride(const std::string& stream_label,
                                  const std::string& language_override) {
  size_t stream_index = kInvalidStreamIndex;
  if (!GetStreamIndex(stream_label, &stream_index))
    LOG(WARNING) << "Invalid stream for language override " << stream_label;
  language_overrides_[stream_index] = language_override;
}

Status Demuxer::InitializeParser() {
  DCHECK(!media_file_);
  DCHECK(!all_streams_ready_);

  LOG(INFO) << "Initialize Demuxer for file '" << file_name_ << "'.";

  media_file_ = File::Open(file_name_.c_str(), "r");
  if (!media_file_) {
    return Status(error::FILE_FAILURE,
                  "Cannot open file for reading " + file_name_);
  }

  int64_t bytes_read = 0;
  bool eof = false;
  if (input_format_.empty()) {
    // Read enough bytes before detecting the container.
    while (static_cast<size_t>(bytes_read) < kInitBufSize) {
      int64_t read_result =
          media_file_->Read(buffer_.get() + bytes_read, kInitBufSize);
      if (read_result < 0)
        return Status(error::FILE_FAILURE, "Cannot read file " + file_name_);
      if (read_result == 0) {
        eof = true;
        break;
      }
      bytes_read += read_result;
    }
    container_name_ = DetermineContainer(buffer_.get(), bytes_read);
  } else {
    container_name_ = DetermineContainerFromFormatName(input_format_);
  }

  // Initialize media parser.
  switch (container_name_) {
    case CONTAINER_MOV:
      parser_.reset(new mp4::MP4MediaParser());
      break;
    case CONTAINER_MPEG2TS:
      parser_.reset(new mp2t::Mp2tMediaParser());
      break;
      // Widevine classic (WVM) is derived from MPEG2PS. We do not support
      // non-WVM MPEG2PS file, thus we do not differentiate between the two.
      // Every MPEG2PS file is assumed to be WVM file. If it turns out not the
      // case, an error will be reported when trying to parse the file as WVM
      // file.
    case CONTAINER_MPEG2PS:
      FALLTHROUGH_INTENDED;
    case CONTAINER_WVM:
      parser_.reset(new wvm::WvmMediaParser());
      break;
    case CONTAINER_WEBM:
      parser_.reset(new WebMMediaParser());
      break;
    case CONTAINER_WEBVTT:
      parser_.reset(new WebVttParser());
      break;
    case CONTAINER_UNKNOWN: {
      const int64_t kDumpSizeLimit = 512;
      LOG(ERROR) << "Failed to detect the container type from the buffer: "
                 << absl::BytesToHexString(absl::string_view(
                        reinterpret_cast<const char*>(buffer_.get()),
                        std::min(bytes_read, kDumpSizeLimit)));
      return Status(error::INVALID_ARGUMENT,
                    "Failed to detect the container type.");
    }
    default:
      NOTIMPLEMENTED() << "Container " << container_name_
                       << " is not supported.";
      return Status(error::UNIMPLEMENTED, "Container not supported.");
  }

  parser_->Init(
      std::bind(&Demuxer::ParserInitEvent, this, std::placeholders::_1),
      std::bind(&Demuxer::NewMediaSampleEvent, this, std::placeholders::_1,
                std::placeholders::_2),
      std::bind(&Demuxer::NewTextSampleEvent, this, std::placeholders::_1,
                std::placeholders::_2),
      key_source_.get());

  // Handle trailing 'moov'.
  if (container_name_ == CONTAINER_MOV &&
      File::IsLocalRegularFile(file_name_.c_str())) {
    // TODO(kqyang): Investigate whether we can reuse the existing file
    // descriptor |media_file_| instead of opening the same file again.
    static_cast<mp4::MP4MediaParser*>(parser_.get())->LoadMoov(file_name_);
  }
  if (!parser_->Parse(buffer_.get(), bytes_read) || (eof && !parser_->Flush())) {
    return Status(error::PARSER_FAILURE,
                  "Cannot parse media file " + file_name_);
  }
  return Status::OK;
}

void Demuxer::ParserInitEvent(
    const std::vector<std::shared_ptr<StreamInfo>>& stream_infos) {
  if (dump_stream_info_) {
    printf("\nFile \"%s\":\n", file_name_.c_str());
    printf("Found %zu stream(s).\n", stream_infos.size());
    for (size_t i = 0; i < stream_infos.size(); ++i)
      printf("Stream [%zu] %s\n", i, stream_infos[i]->ToString().c_str());
  }

  int base_stream_index = 0;
  bool video_handler_set =
      output_handlers().find(kBaseVideoOutputStreamIndex) !=
      output_handlers().end();
  bool audio_handler_set =
      output_handlers().find(kBaseAudioOutputStreamIndex) !=
      output_handlers().end();
  bool text_handler_set =
      output_handlers().find(kBaseTextOutputStreamIndex) !=
      output_handlers().end();
  for (const std::shared_ptr<StreamInfo>& stream_info : stream_infos) {
    size_t stream_index = base_stream_index;
    if (video_handler_set && stream_info->stream_type() == kStreamVideo) {
      stream_index = kBaseVideoOutputStreamIndex;
      // Only for the first video stream.
      video_handler_set = false;
    }
    if (audio_handler_set && stream_info->stream_type() == kStreamAudio) {
      stream_index = kBaseAudioOutputStreamIndex;
      // Only for the first audio stream.
      audio_handler_set = false;
    }
    if (text_handler_set && stream_info->stream_type() == kStreamText) {
      stream_index = kBaseTextOutputStreamIndex;
      text_handler_set = false;
    }

    const bool handler_set =
        output_handlers().find(stream_index) != output_handlers().end();
    if (handler_set) {
      track_id_to_stream_index_map_[stream_info->track_id()] = stream_index;
      stream_indexes_.push_back(stream_index);
      auto iter = language_overrides_.find(stream_index);
      if (iter != language_overrides_.end() &&
          stream_info->stream_type() != kStreamVideo) {
        stream_info->set_language(iter->second);
      }
      if (stream_info->is_encrypted()) {
        init_event_status_.Update(Status(error::INVALID_ARGUMENT,
                                         "A decryption key source is not "
                                         "provided for an encrypted stream."));
      } else {
        init_event_status_.Update(
            DispatchStreamInfo(stream_index, stream_info));
      }
    } else {
      track_id_to_stream_index_map_[stream_info->track_id()] =
          kInvalidStreamIndex;
    }
    ++base_stream_index;
  }
  all_streams_ready_ = true;
}

bool Demuxer::NewMediaSampleEvent(uint32_t track_id,
                                  std::shared_ptr<MediaSample> sample) {
  if (!all_streams_ready_) {
    if (queued_media_samples_.size() >= kQueuedSamplesLimit) {
      LOG(ERROR) << "Queued samples limit reached: " << kQueuedSamplesLimit;
      return false;
    }
    queued_media_samples_.emplace_back(track_id, sample);
    return true;
  }
  if (!init_event_status_.ok()) {
    return false;
  }

  while (!queued_media_samples_.empty()) {
    if (!PushMediaSample(queued_media_samples_.front().track_id,
                         queued_media_samples_.front().sample)) {
      return false;
    }
    queued_media_samples_.pop_front();
  }
  return PushMediaSample(track_id, sample);
}

bool Demuxer::NewTextSampleEvent(uint32_t track_id,
                                 std::shared_ptr<TextSample> sample) {
  if (!all_streams_ready_) {
    if (queued_text_samples_.size() >= kQueuedSamplesLimit) {
      LOG(ERROR) << "Queued samples limit reached: " << kQueuedSamplesLimit;
      return false;
    }
    queued_text_samples_.emplace_back(track_id, sample);
    return true;
  }
  if (!init_event_status_.ok()) {
    return false;
  }

  while (!queued_text_samples_.empty()) {
    if (!PushTextSample(queued_text_samples_.front().track_id,
                        queued_text_samples_.front().sample)) {
      return false;
    }
    queued_text_samples_.pop_front();
  }
  return PushTextSample(track_id, sample);
}

bool Demuxer::PushMediaSample(uint32_t track_id,
                              std::shared_ptr<MediaSample> sample) {
  auto stream_index_iter = track_id_to_stream_index_map_.find(track_id);
  if (stream_index_iter == track_id_to_stream_index_map_.end()) {
    LOG(ERROR) << "Track " << track_id << " not found.";
    return false;
  }
  if (stream_index_iter->second == kInvalidStreamIndex)
    return true;
  Status status = DispatchMediaSample(stream_index_iter->second, sample);
  if (!status.ok()) {
    LOG(ERROR) << "Failed to process sample " << stream_index_iter->second
               << " " << status;
    return false;
  }
  return true;
}

bool Demuxer::PushTextSample(uint32_t track_id,
                             std::shared_ptr<TextSample> sample) {
  auto stream_index_iter = track_id_to_stream_index_map_.find(track_id);
  if (stream_index_iter == track_id_to_stream_index_map_.end()) {
    LOG(ERROR) << "Track " << track_id << " not found.";
    return false;
  }
  if (stream_index_iter->second == kInvalidStreamIndex)
    return true;
  Status status = DispatchTextSample(stream_index_iter->second, sample);
  if (!status.ok()) {
    LOG(ERROR) << "Failed to process sample " << stream_index_iter->second
               << " " << status;
    return false;
  }
  return true;
}

Status Demuxer::Parse() {
  DCHECK(media_file_);
  DCHECK(parser_);
  DCHECK(buffer_);

  int64_t bytes_read = media_file_->Read(buffer_.get(), kBufSize);
  if (bytes_read == 0) {
    if (!parser_->Flush())
      return Status(error::PARSER_FAILURE, "Failed to flush.");
    return Status(error::END_OF_STREAM, "");
  } else if (bytes_read < 0) {
    return Status(error::FILE_FAILURE, "Cannot read file " + file_name_);
  }

  return parser_->Parse(buffer_.get(), bytes_read)
             ? Status::OK
             : Status(error::PARSER_FAILURE,
                      "Cannot parse media file " + file_name_);
}

}  // namespace media
}  // namespace shaka
