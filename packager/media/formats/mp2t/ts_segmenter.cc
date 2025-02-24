// Copyright 2016 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/media/formats/mp2t/ts_segmenter.h>

#include <memory>

#include <absl/log/check.h>

#include <packager/macros/status.h>
#include <packager/media/base/audio_stream_info.h>
#include <packager/media/base/muxer_util.h>
#include <packager/media/base/video_stream_info.h>
#include <packager/media/event/muxer_listener.h>
#include <packager/media/formats/mp2t/pes_packet.h>
#include <packager/media/formats/mp2t/program_map_table_writer.h>
#include <packager/media/formats/mp2t/ts_packet_writer_util.h>
#include <packager/status.h>

namespace shaka {
namespace media {
namespace mp2t {

namespace {
const double kTsTimescale = 90000;

bool IsAudioCodec(Codec codec) {
  return codec >= kCodecAudio && codec < kCodecAudioMaxPlusOne;
}

bool IsVideoCodec(Codec codec) {
  return codec >= kCodecVideo && codec < kCodecVideoMaxPlusOne;
}

}  // namespace

TsSegmenter::TsSegmenter(const MuxerOptions& options, MuxerListener* listener)
    : listener_(listener),
      transport_stream_timestamp_offset_(
          options.transport_stream_timestamp_offset_ms * kTsTimescale / 1000),
      pes_packet_generator_(
          new PesPacketGenerator(transport_stream_timestamp_offset_)) {}

TsSegmenter::~TsSegmenter() {}

Status TsSegmenter::Initialize(const StreamInfo& stream_info) {
  if (!pes_packet_generator_->Initialize(stream_info)) {
    return Status(error::MUXER_FAILURE,
                  "Failed to initialize PesPacketGenerator.");
  }

  const StreamType stream_type = stream_info.stream_type();
  if (stream_type != StreamType::kStreamVideo &&
      stream_type != StreamType::kStreamAudio) {
    LOG(ERROR) << "TsWriter cannot handle stream type " << stream_type
               << " yet.";
    return Status(error::MUXER_FAILURE, "Unsupported stream type.");
  }

  codec_ = stream_info.codec();
  if (stream_type == StreamType::kStreamAudio)
    audio_codec_config_ = stream_info.codec_config();

  timescale_scale_ = kTsTimescale / stream_info.time_scale();
  return Status::OK;
}

Status TsSegmenter::Finalize() {
  return Status::OK;
}

Status TsSegmenter::AddSample(const MediaSample& sample) {
  if (!ts_writer_) {
    uint32_t segment_number = muxer_options_.mp4_params.sequence_number;
    std::unique_ptr<ProgramMapTableWriter> pmt_writer;
    if (codec_ == kCodecAC3) {
      // https://goo.gl/N7Tvqi MPEG-2 Stream Encryption Format for HTTP Live
      // Streaming 2.3.2.2 AC-3 Setup: For AC-3, the setup_data in the
      // audio_setup_information is the first 10 bytes of the audio data (the
      // syncframe()).
      // For unencrypted AC3, the setup_data is not used, so what is in there
      // does not matter.
      const size_t kSetupDataSize = 10u;
      if (sample.data_size() < kSetupDataSize) {
        LOG(ERROR) << "Sample is too small for AC3: " << sample.data_size();
        return Status(error::MUXER_FAILURE, "Sample is too small for AC3.");
      }
      const std::vector<uint8_t> setup_data(sample.data(),
                                            sample.data() + kSetupDataSize);
      pmt_writer.reset(
          new AudioProgramMapTableWriter(codec_, setup_data, segment_number));
    } else if (IsAudioCodec(codec_)) {
      pmt_writer.reset(new AudioProgramMapTableWriter(
          codec_, audio_codec_config_, segment_number));
    } else {
      DCHECK(IsVideoCodec(codec_));
      pmt_writer.reset(new VideoProgramMapTableWriter(codec_, segment_number));
    }
    ts_writer_.reset(new TsWriter(std::move(pmt_writer), segment_number));
  }

  if (sample.is_encrypted())
    ts_writer_->SignalEncrypted();

  if (!segment_started_ && !sample.is_key_frame())
    LOG(WARNING) << "A segment will start with a non key frame.";

  if (!pes_packet_generator_->PushSample(sample)) {
    return Status(error::MUXER_FAILURE,
                  "Failed to add sample to PesPacketGenerator.");
  }
  return WritePesPackets();
}

void TsSegmenter::InjectTsWriterForTesting(std::unique_ptr<TsWriter> writer) {
  ts_writer_ = std::move(writer);
}

void TsSegmenter::InjectPesPacketGeneratorForTesting(
    std::unique_ptr<PesPacketGenerator> generator) {
  pes_packet_generator_ = std::move(generator);
}

void TsSegmenter::SetSegmentStartedForTesting(bool value) {
  segment_started_ = value;
}

Status TsSegmenter::StartSegmentIfNeeded(int64_t next_pts) {
  if (segment_started_)
    return Status::OK;
  segment_start_timestamp_ = next_pts;
  if (!ts_writer_->NewSegment(&segment_buffer_))
    return Status(error::MUXER_FAILURE, "Failed to initialize new segment.");
  segment_started_ = true;
  return Status::OK;
}

Status TsSegmenter::WritePesPackets() {
  while (pes_packet_generator_->NumberOfReadyPesPackets() > 0u) {
    std::unique_ptr<PesPacket> pes_packet =
        pes_packet_generator_->GetNextPesPacket();

    Status status = StartSegmentIfNeeded(pes_packet->pts());
    if (!status.ok())
      return status;

    if (listener_ && IsVideoCodec(codec_) && pes_packet->is_key_frame()) {
      uint64_t start_pos = segment_buffer_.Size();
      const int64_t timestamp = pes_packet->pts();
      if (!ts_writer_->AddPesPacket(std::move(pes_packet), &segment_buffer_))
        return Status(error::MUXER_FAILURE, "Failed to add PES packet.");

      uint64_t end_pos = segment_buffer_.Size();

      listener_->OnKeyFrame(timestamp, start_pos, end_pos - start_pos);
    } else {
      if (!ts_writer_->AddPesPacket(std::move(pes_packet), &segment_buffer_))
        return Status(error::MUXER_FAILURE, "Failed to add PES packet.");
    }
  }
  return Status::OK;
}

Status TsSegmenter::FinalizeSegment(int64_t start_timestamp, int64_t duration) {
  if (!pes_packet_generator_->Flush()) {
    return Status(error::MUXER_FAILURE, "Failed to flush PesPacketGenerator.");
  }
  Status status = WritePesPackets();
  if (!status.ok())
    return status;

  // Since the ending CC of the previous segment could be anything,
  // we must end all ES packets at 0xf so that the next segment can start
  // at 0x0. This can be done by stuffing null packets at the end of the segment
  // for each elementary stream
  if (muxer_options_.enable_null_ts_packet_stuffing) {
    ContinuityCounter& es_continuity_counter =
        ts_writer_->es_continuity_counter();
    while (es_continuity_counter.GetCurrent() != 0) {
      const int pid = ProgramMapTableWriter::kElementaryPid;
      BufferWriter null_ts_packet_buffer;
      // TODO(Fordyce): do the stuffing packets need a payload?
      // null_ts_packet_buffer.AppendInt(static_cast<uint8_t>(TsSection::kPidNullPacket));
      WritePayloadToBufferWriter(
          null_ts_packet_buffer.Buffer(), null_ts_packet_buffer.Size(), false,
          pid, false, 0, &es_continuity_counter, &segment_buffer_);
    }
  }

  return Status::OK;
}

}  // namespace mp2t
}  // namespace media
}  // namespace shaka
