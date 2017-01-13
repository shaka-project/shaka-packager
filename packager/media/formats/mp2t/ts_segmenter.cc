// Copyright 2016 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/formats/mp2t/ts_segmenter.h"

#include <memory>

#include "packager/media/base/aes_encryptor.h"
#include "packager/media/base/key_source.h"
#include "packager/media/base/muxer_util.h"
#include "packager/media/base/status.h"
#include "packager/media/base/video_stream_info.h"
#include "packager/media/event/muxer_listener.h"
#include "packager/media/event/progress_listener.h"

namespace shaka {
namespace media {
namespace mp2t {

namespace {
const double kTsTimescale = 90000;
}  // namespace

TsSegmenter::TsSegmenter(const MuxerOptions& options, MuxerListener* listener)
    : muxer_options_(options),
      listener_(listener),
      ts_writer_(new TsWriter()),
      pes_packet_generator_(new PesPacketGenerator()) {}
TsSegmenter::~TsSegmenter() {}

Status TsSegmenter::Initialize(const StreamInfo& stream_info,
                               KeySource* encryption_key_source,
                               uint32_t max_sd_pixels,
                               uint32_t max_hd_pixels,
                               uint32_t max_uhd1_pixels,
                               double clear_lead_in_seconds) {
  if (muxer_options_.segment_template.empty())
    return Status(error::MUXER_FAILURE, "Segment template not specified.");
  if (!ts_writer_->Initialize(stream_info))
    return Status(error::MUXER_FAILURE, "Failed to initialize TsWriter.");
  if (!pes_packet_generator_->Initialize(stream_info)) {
    return Status(error::MUXER_FAILURE,
                  "Failed to initialize PesPacketGenerator.");
  }

  if (encryption_key_source) {
    std::unique_ptr<EncryptionKey> encryption_key(new EncryptionKey());
    const KeySource::TrackType type =
        GetTrackTypeForEncryption(stream_info, max_sd_pixels,
                                  max_hd_pixels, max_uhd1_pixels);
    Status status = encryption_key_source->GetKey(type, encryption_key.get());

    if (encryption_key->iv.empty()) {
      if (!AesCryptor::GenerateRandomIv(FOURCC_cbcs, &encryption_key->iv)) {
        return Status(error::INTERNAL_ERROR, "Failed to generate random iv.");
      }
    }
    if (!status.ok())
      return status;

    encryption_key_ = std::move(encryption_key);
    clear_lead_in_seconds_ = clear_lead_in_seconds;

    if (listener_) {
      // For now this only happens once, so send true.
      const bool kIsInitialEncryptionInfo = true;
      listener_->OnEncryptionInfoReady(
          kIsInitialEncryptionInfo, FOURCC_cbcs, encryption_key_->key_id,
          encryption_key_->iv, encryption_key_->key_system_info);
    }

    status = NotifyEncrypted();
    if (!status.ok())
      return status;
  }

  timescale_scale_ = kTsTimescale / stream_info.time_scale();
  return Status::OK;
}

Status TsSegmenter::Finalize() {
  return Flush();
}

// First checks whether the sample is a key frame. If so and the segment has
// passed the segment duration, then flush the generator and write all the data
// to file.
Status TsSegmenter::AddSample(scoped_refptr<MediaSample> sample) {
  const bool passed_segment_duration =
      current_segment_total_sample_duration_ > muxer_options_.segment_duration;
  if (sample->is_key_frame() && passed_segment_duration) {
    Status status = Flush();
    if (!status.ok())
      return status;
  }

  if (!ts_writer_file_opened_ && !sample->is_key_frame())
    LOG(WARNING) << "A segment will start with a non key frame.";

  if (!pes_packet_generator_->PushSample(sample)) {
    return Status(error::MUXER_FAILURE,
                  "Failed to add sample to PesPacketGenerator.");
  }

  const double scaled_sample_duration = sample->duration() * timescale_scale_;
  current_segment_total_sample_duration_ +=
      scaled_sample_duration / kTsTimescale;

  return WritePesPacketsToFile();
}

void TsSegmenter::InjectTsWriterForTesting(std::unique_ptr<TsWriter> writer) {
  ts_writer_ = std::move(writer);
}

void TsSegmenter::InjectPesPacketGeneratorForTesting(
    std::unique_ptr<PesPacketGenerator> generator) {
  pes_packet_generator_ = std::move(generator);
}

void TsSegmenter::SetTsWriterFileOpenedForTesting(bool value) {
  ts_writer_file_opened_ = value;
}

Status TsSegmenter::OpenNewSegmentIfClosed(uint32_t next_pts) {
  if (ts_writer_file_opened_)
    return Status::OK;
  const std::string segment_name =
      GetSegmentName(muxer_options_.segment_template, next_pts,
                     segment_number_++, muxer_options_.bandwidth);
  if (!ts_writer_->NewSegment(segment_name))
    return Status(error::MUXER_FAILURE, "Failed to initilize TsPacketWriter.");
  current_segment_start_time_ = next_pts;
  current_segment_path_ = segment_name;
  ts_writer_file_opened_ = true;
  return Status::OK;
}

Status TsSegmenter::WritePesPacketsToFile() {
  while (pes_packet_generator_->NumberOfReadyPesPackets() > 0u) {
    std::unique_ptr<PesPacket> pes_packet =
        pes_packet_generator_->GetNextPesPacket();

    Status status = OpenNewSegmentIfClosed(pes_packet->pts());
    if (!status.ok())
      return status;

    if (!ts_writer_->AddPesPacket(std::move(pes_packet)))
      return Status(error::MUXER_FAILURE, "Failed to add PES packet.");
  }
  return Status::OK;
}

Status TsSegmenter::Flush() {
  if (!pes_packet_generator_->Flush()) {
    return Status(error::MUXER_FAILURE,
                  "Failed to flush PesPacketGenerator.");
  }
  Status status = WritePesPacketsToFile();
  if (!status.ok())
    return status;

  // This method may be called from Finalize() so ts_writer_file_opened_ could
  // be false.
  if (ts_writer_file_opened_) {
    if (!ts_writer_->FinalizeSegment()) {
      return Status(error::MUXER_FAILURE, "Failed to finalize TsWriter.");
    }
    if (listener_) {
      const int64_t file_size =
          File::GetFileSize(current_segment_path_.c_str());
      listener_->OnNewSegment(
          current_segment_path_, current_segment_start_time_,
          current_segment_total_sample_duration_ * kTsTimescale, file_size);
    }
    ts_writer_file_opened_ = false;
    total_duration_in_seconds_ += current_segment_total_sample_duration_;
  }
  current_segment_total_sample_duration_ = 0.0;
  current_segment_start_time_ = 0;
  current_segment_path_.clear();
  return NotifyEncrypted();
}

Status TsSegmenter::NotifyEncrypted() {
  if (encryption_key_ && total_duration_in_seconds_ >= clear_lead_in_seconds_) {
    if (listener_)
      listener_->OnEncryptionStart();

    if (!pes_packet_generator_->SetEncryptionKey(std::move(encryption_key_)))
      return Status(error::INTERNAL_ERROR, "Failed to set encryption key.");
    ts_writer_->SignalEncrypted();
  }
  return Status::OK;
}

}  // namespace mp2t
}  // namespace media
}  // namespace shaka
