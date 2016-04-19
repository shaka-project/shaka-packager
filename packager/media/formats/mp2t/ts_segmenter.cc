// Copyright 2016 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/formats/mp2t/ts_segmenter.h"

#include <memory>

#include "packager/media/base/muxer_util.h"
#include "packager/media/base/status.h"

namespace edash_packager {
namespace media {
namespace mp2t {

namespace {
const double kTsTimescale = 90000;
}  // namespace

TsSegmenter::TsSegmenter(const MuxerOptions& options)
    : muxer_options_(options),
      ts_writer_(new TsWriter()),
      pes_packet_generator_(new PesPacketGenerator()) {}
TsSegmenter::~TsSegmenter() {}

Status TsSegmenter::Initialize(const StreamInfo& stream_info) {
  if (muxer_options_.segment_template.empty())
    return Status(error::MUXER_FAILURE, "Segment template not specified.");
  if (!ts_writer_->Initialize(stream_info))
    return Status(error::MUXER_FAILURE, "Failed to initialize TsWriter.");
  if (!pes_packet_generator_->Initialize(stream_info)) {
    return Status(error::MUXER_FAILURE,
                  "Failed to initialize PesPacketGenerator.");
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

void TsSegmenter::InjectTsWriterForTesting(scoped_ptr<TsWriter> writer) {
  ts_writer_ = writer.Pass();
}

void TsSegmenter::InjectPesPacketGeneratorForTesting(
    scoped_ptr<PesPacketGenerator> generator) {
  pes_packet_generator_ = generator.Pass();
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
  ts_writer_file_opened_ = true;
  return Status::OK;
}

Status TsSegmenter::WritePesPacketsToFile() {
  while (pes_packet_generator_->NumberOfReadyPesPackets() > 0u) {
    scoped_ptr<PesPacket> pes_packet =
        pes_packet_generator_->GetNextPesPacket();

    Status status = OpenNewSegmentIfClosed(pes_packet->pts());
    if (!status.ok())
      return status;

    if (!ts_writer_->AddPesPacket(pes_packet.Pass()))
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
    ts_writer_file_opened_ = false;
  }
  current_segment_total_sample_duration_ = 0.0;
  return Status::OK;
}

}  // namespace mp2t
}  // namespace media
}  // namespace edash_packager
