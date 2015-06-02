// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "packager/media/formats/mp2t/es_parser_h264.h"

#include <stdint.h>

#include "packager/base/logging.h"
#include "packager/base/numerics/safe_conversions.h"
#include "packager/media/base/media_sample.h"
#include "packager/media/base/offset_byte_queue.h"
#include "packager/media/base/timestamp.h"
#include "packager/media/base/video_stream_info.h"
#include "packager/media/filters/h264_byte_to_unit_stream_converter.h"
#include "packager/media/filters/h264_parser.h"
#include "packager/media/formats/mp2t/mp2t_common.h"

namespace edash_packager {
namespace media {
namespace mp2t {

namespace {

// An AUD NALU is at least 4 bytes:
// 3 bytes for the start code + 1 byte for the NALU type.
const int kMinAUDSize = 4;

}  // anonymous namespace

EsParserH264::EsParserH264(uint32_t pid,
                           const NewStreamInfoCB& new_stream_info_cb,
                           const EmitSampleCB& emit_sample_cb)
    : EsParser(pid),
      new_stream_info_cb_(new_stream_info_cb),
      emit_sample_cb_(emit_sample_cb),
      es_queue_(new media::OffsetByteQueue()),
      h264_parser_(new H264Parser()),
      current_access_unit_pos_(0),
      next_access_unit_pos_(0),
      stream_converter_(new H264ByteToUnitStreamConverter),
      decoder_config_check_pending_(false),
      pending_sample_duration_(0),
      waiting_for_key_frame_(true) {
}

EsParserH264::~EsParserH264() {
}

bool EsParserH264::Parse(const uint8_t* buf,
                         int size,
                         int64_t pts,
                         int64_t dts) {
  // Note: Parse is invoked each time a PES packet has been reassembled.
  // Unfortunately, a PES packet does not necessarily map
  // to an h264 access unit, although the HLS recommendation is to use one PES
  // for each access unit (but this is just a recommendation and some streams
  // do not comply with this recommendation).

  // HLS recommendation: "In AVC video, you should have both a DTS and a
  // PTS in each PES header".
  // However, some streams do not comply with this recommendation.
  DVLOG_IF(1, pts == kNoTimestamp) << "Each video PES should have a PTS";
  if (pts != kNoTimestamp) {
    TimingDesc timing_desc;
    timing_desc.pts = pts;
    timing_desc.dts = (dts != kNoTimestamp) ? dts : pts;

    // Link the end of the byte queue with the incoming timing descriptor.
    timing_desc_list_.push_back(
        std::pair<int64_t, TimingDesc>(es_queue_->tail(), timing_desc));
  }

  // Add the incoming bytes to the ES queue.
  es_queue_->Push(buf, size);
  return ParseInternal();
}

void EsParserH264::Flush() {
  DVLOG(1) << "EsParserH264::Flush";

  if (FindAUD(&current_access_unit_pos_)) {
    // Simulate an additional AUD to force emitting the last access unit
    // which is assumed to be complete at this point.
    uint8_t aud[] = {0x00, 0x00, 0x01, 0x09};
    es_queue_->Push(aud, sizeof(aud));
    ParseInternal();
  }

  if (pending_sample_) {
    // Flush pending sample.
    DCHECK(pending_sample_duration_);
    pending_sample_->set_duration(pending_sample_duration_);
    emit_sample_cb_.Run(pid(), pending_sample_);
    pending_sample_ = scoped_refptr<MediaSample>();
  }
}

void EsParserH264::Reset() {
  DVLOG(1) << "EsParserH264::Reset";
  es_queue_.reset(new media::OffsetByteQueue());
  h264_parser_.reset(new H264Parser());
  current_access_unit_pos_ = 0;
  next_access_unit_pos_ = 0;
  timing_desc_list_.clear();
  last_video_decoder_config_ = scoped_refptr<StreamInfo>();
  decoder_config_check_pending_ = false;
  pending_sample_ = scoped_refptr<MediaSample>();
  pending_sample_duration_ = 0;
  waiting_for_key_frame_ = true;
}

bool EsParserH264::FindAUD(int64_t* stream_pos) {
  while (true) {
    const uint8_t* es;
    int size;
    es_queue_->PeekAt(*stream_pos, &es, &size);

    // Find a start code and move the stream to the start code parser position.
    off_t start_code_offset;
    off_t start_code_size;
    bool start_code_found = H264Parser::FindStartCode(
        es, size, &start_code_offset, &start_code_size);
    *stream_pos += start_code_offset;

    // No H264 start code found or NALU type not available yet.
    if (!start_code_found || start_code_offset + start_code_size >= size)
      return false;

    // Exit the parser loop when an AUD is found.
    // Note: NALU header for an AUD:
    // - nal_ref_idc must be 0
    // - nal_unit_type must be H264NALU::kAUD
    if (es[start_code_offset + start_code_size] == H264NALU::kAUD)
      break;

    // The current NALU is not an AUD, skip the start code
    // and continue parsing the stream.
    *stream_pos += start_code_size;
  }

  return true;
}

bool EsParserH264::ParseInternal() {
  DCHECK_LE(es_queue_->head(), current_access_unit_pos_);
  DCHECK_LE(current_access_unit_pos_, next_access_unit_pos_);
  DCHECK_LE(next_access_unit_pos_, es_queue_->tail());

  // Find the next AUD located at or after |current_access_unit_pos_|. This is
  // needed since initially |current_access_unit_pos_| might not point to
  // an AUD.
  // Discard all the data before the updated |current_access_unit_pos_|
  // since it won't be used again.
  bool aud_found = FindAUD(&current_access_unit_pos_);
  es_queue_->Trim(current_access_unit_pos_);
  if (next_access_unit_pos_ < current_access_unit_pos_)
    next_access_unit_pos_ = current_access_unit_pos_;

  // Resume parsing later if no AUD was found.
  if (!aud_found)
    return true;

  // Find the next AUD to make sure we have a complete access unit.
  if (next_access_unit_pos_ < current_access_unit_pos_ + kMinAUDSize) {
    next_access_unit_pos_ = current_access_unit_pos_ + kMinAUDSize;
    DCHECK_LE(next_access_unit_pos_, es_queue_->tail());
  }
  if (!FindAUD(&next_access_unit_pos_))
    return true;

  // At this point, we know we have a full access unit.
  bool is_key_frame = false;
  int pps_id_for_access_unit = -1;

  const uint8_t* es;
  int size;
  es_queue_->PeekAt(current_access_unit_pos_, &es, &size);
  int access_unit_size = base::checked_cast<int, int64_t>(
      next_access_unit_pos_ - current_access_unit_pos_);
  DCHECK_LE(access_unit_size, size);
  h264_parser_->SetStream(es, access_unit_size);

  while (true) {
    bool is_eos = false;
    H264NALU nalu;
    switch (h264_parser_->AdvanceToNextNALU(&nalu)) {
      case H264Parser::kOk:
        break;
      case H264Parser::kInvalidStream:
      case H264Parser::kUnsupportedStream:
        return false;
      case H264Parser::kEOStream:
        is_eos = true;
        break;
    }
    if (is_eos)
      break;

    switch (nalu.nal_unit_type) {
      case H264NALU::kAUD: {
        DVLOG(LOG_LEVEL_ES) << "NALU: AUD";
        break;
      }
      case H264NALU::kSPS: {
        DVLOG(LOG_LEVEL_ES) << "NALU: SPS";
        int sps_id;
        if (h264_parser_->ParseSPS(&sps_id) != H264Parser::kOk)
          return false;
        decoder_config_check_pending_ = true;
        break;
      }
      case H264NALU::kPPS: {
        DVLOG(LOG_LEVEL_ES) << "NALU: PPS";
        int pps_id;
        if (h264_parser_->ParsePPS(&pps_id) != H264Parser::kOk) {
          // Allow PPS parsing to fail if waiting for SPS.
          if (last_video_decoder_config_)
            return false;
        } else {
          decoder_config_check_pending_ = true;
        }
        break;
      }
      case H264NALU::kIDRSlice:
      case H264NALU::kNonIDRSlice: {
        is_key_frame = (nalu.nal_unit_type == H264NALU::kIDRSlice);
        DVLOG(LOG_LEVEL_ES) << "NALU: slice IDR=" << is_key_frame;
        H264SliceHeader shdr;
        if (h264_parser_->ParseSliceHeader(nalu, &shdr) != H264Parser::kOk) {
          // Only accept an invalid SPS/PPS at the beginning when the stream
          // does not necessarily start with an SPS/PPS/IDR.
          if (last_video_decoder_config_)
            return false;
        } else {
          pps_id_for_access_unit = shdr.pic_parameter_set_id;
        }
        break;
      }
      default: {
        DVLOG(LOG_LEVEL_ES) << "NALU: " << nalu.nal_unit_type;
      }
    }
  }

  if (waiting_for_key_frame_) {
    waiting_for_key_frame_ = !is_key_frame;
  }
  if (!waiting_for_key_frame_) {
    // Emit a frame and move the stream to the next AUD position.
    RCHECK(EmitFrame(current_access_unit_pos_, access_unit_size,
                     is_key_frame, pps_id_for_access_unit));
  }
  current_access_unit_pos_ = next_access_unit_pos_;
  es_queue_->Trim(current_access_unit_pos_);

  return true;
}

bool EsParserH264::EmitFrame(int64_t access_unit_pos,
                             int access_unit_size,
                             bool is_key_frame,
                             int pps_id) {
  // Get the access unit timing info.
  TimingDesc current_timing_desc = {kNoTimestamp, kNoTimestamp};
  while (!timing_desc_list_.empty() &&
         timing_desc_list_.front().first <= access_unit_pos) {
    current_timing_desc = timing_desc_list_.front().second;
    timing_desc_list_.pop_front();
  }
  if (current_timing_desc.pts == kNoTimestamp)
    return false;

  // Emit a frame.
  DVLOG(LOG_LEVEL_ES) << "Emit frame: stream_pos=" << current_access_unit_pos_
                      << " size=" << access_unit_size;
  int es_size;
  const uint8_t* es;
  es_queue_->PeekAt(current_access_unit_pos_, &es, &es_size);
  CHECK_GE(es_size, access_unit_size);

  // Convert frame to unit stream format.
  std::vector<uint8_t> converted_frame;
  if (!stream_converter_->ConvertByteStreamToNalUnitStream(
          es, access_unit_size, &converted_frame)) {
    DLOG(ERROR) << "Failure to convert video frame to unit stream format.";
    return false;
  }

  if (decoder_config_check_pending_) {
    // Update the video decoder configuration if needed.
    const H264PPS* pps = h264_parser_->GetPPS(pps_id);
    if (!pps) {
      // Only accept an invalid PPS at the beginning when the stream
      // does not necessarily start with an SPS/PPS/IDR.
      // In this case, the initial frames are conveyed to the upper layer with
      // an invalid VideoDecoderConfig and it's up to the upper layer
      // to process this kind of frame accordingly.
      if (last_video_decoder_config_)
        return false;
    } else {
      const H264SPS* sps = h264_parser_->GetSPS(pps->seq_parameter_set_id);
      if (!sps)
        return false;
      RCHECK(UpdateVideoDecoderConfig(sps));
      decoder_config_check_pending_ = false;
    }
  }

  // Create the media sample, emitting always the previous sample after
  // calculating its duration.
  scoped_refptr<MediaSample> media_sample = MediaSample::CopyFrom(
      converted_frame.data(), converted_frame.size(), is_key_frame);
  media_sample->set_dts(current_timing_desc.dts);
  media_sample->set_pts(current_timing_desc.pts);
  if (pending_sample_) {
    DCHECK_GT(media_sample->dts(), pending_sample_->dts());
    pending_sample_duration_ = media_sample->dts() - pending_sample_->dts();
    pending_sample_->set_duration(pending_sample_duration_);
    emit_sample_cb_.Run(pid(), pending_sample_);
  }
  pending_sample_ = media_sample;

  return true;
}

bool EsParserH264::UpdateVideoDecoderConfig(const H264SPS* sps) {
  std::vector<uint8_t> decoder_config_record;
  if (!stream_converter_->GetAVCDecoderConfigurationRecord(
          &decoder_config_record)) {
    DLOG(ERROR) << "Failure to construct an AVCDecoderConfigurationRecord";
    return false;
  }

  if (last_video_decoder_config_) {
    if (last_video_decoder_config_->extra_data() != decoder_config_record) {
      // Video configuration has changed. Issue warning.
      // TODO(tinskip): Check the nature of the configuration change. Only
      // minor configuration changes (such as frame ordering) can be handled
      // gracefully by decoders without notification. Major changes (such as
      // video resolution changes) should be treated as errors.
      LOG(WARNING) << "H.264 decoder configuration has changed.";
      last_video_decoder_config_->set_extra_data(decoder_config_record);
    }
    return true;
  }

  // TODO: a MAP unit can be either 16 or 32 pixels.
  // although it's 16 pixels for progressive non MBAFF frames.
  uint16_t width = (sps->pic_width_in_mbs_minus1 + 1) * 16;
  uint16_t height = (sps->pic_height_in_map_units_minus1 + 1) * 16;

  last_video_decoder_config_ = scoped_refptr<StreamInfo>(
      new VideoStreamInfo(
          pid(),
          kMpeg2Timescale,
          kInfiniteDuration,
          kCodecH264,
          VideoStreamInfo::GetCodecString(kCodecH264,
                                          decoder_config_record[1],
                                          decoder_config_record[2],
                                          decoder_config_record[3]),
          std::string(),
          width,
          height,
          0,
          H264ByteToUnitStreamConverter::kUnitStreamNaluLengthSize,
          decoder_config_record.data(),
          decoder_config_record.size(),
          false));
  DVLOG(1) << "Profile IDC: " << sps->profile_idc;
  DVLOG(1) << "Level IDC: " << sps->level_idc;
  DVLOG(1) << "Pic width: " << width;
  DVLOG(1) << "Pic height: " << height;
  DVLOG(1) << "log2_max_frame_num_minus4: "
           << sps->log2_max_frame_num_minus4;
  DVLOG(1) << "SAR: width=" << sps->sar_width
           << " height=" << sps->sar_height;

  // Video config notification.
  new_stream_info_cb_.Run(last_video_decoder_config_);

  return true;
}

}  // namespace mp2t
}  // namespace media
}  // namespace edash_packager
