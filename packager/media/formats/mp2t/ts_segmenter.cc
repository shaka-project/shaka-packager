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
const int kTsTimescale = 90000;
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

  return Status::OK;
}

Status TsSegmenter::Finalize() {
  if (!pes_packet_generator_->Flush()) {
    return Status(error::MUXER_FAILURE,
                  "Failed to finalize PesPacketGenerator.");
  }

  Status status = WritePesPacketsToFiles();
  if (!status.ok())
    return status;

  if (!ts_writer_file_opened_)
    return Status::OK;

  if (!ts_writer_->FinalizeSegment())
    return Status(error::MUXER_FAILURE, "Failed to finalize TsPacketWriter.");
  ts_writer_file_opened_ = false;
  return Status::OK;
}

Status TsSegmenter::AddSample(scoped_refptr<MediaSample> sample) {
  if (!pes_packet_generator_->PushSample(sample)) {
    return Status(error::MUXER_FAILURE,
                  "Failed to add sample to PesPacketGenerator.");
  }
  // TODO(rkuriowa): Only segment files before a key frame.
  return WritePesPacketsToFiles();
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

Status TsSegmenter::FinalizeSegmentIfPastSegmentDuration() {
  if (current_segment_duration_ > muxer_options_.segment_duration) {
    if (!ts_writer_->FinalizeSegment())
      return Status(error::FILE_FAILURE, "Failed to finalize segment.");
    ts_writer_file_opened_ = false;
    current_segment_duration_ = 0.0;
  }
  return Status::OK;
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

Status TsSegmenter::WritePesPacketsToFiles() {
  while (pes_packet_generator_->NumberOfReadyPesPackets() > 0u) {
    scoped_ptr<PesPacket> pes_packet =
        pes_packet_generator_->GetNextPesPacket();

    Status status = OpenNewSegmentIfClosed(pes_packet->pts());
    if (!status.ok())
      return status;

    const double pes_packet_duration = pes_packet->duration();

    if (!ts_writer_->AddPesPacket(pes_packet.Pass()))
      return Status(error::MUXER_FAILURE, "Failed to add PES packet.");

    current_segment_duration_ += pes_packet_duration / kTsTimescale;

    status = FinalizeSegmentIfPastSegmentDuration();
    if (!status.ok())
      return status;
  }
  return Status::OK;
}

}  // namespace mp2t
}  // namespace media
}  // namespace edash_packager
