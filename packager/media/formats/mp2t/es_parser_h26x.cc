// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "packager/media/formats/mp2t/es_parser_h26x.h"

#include <stdint.h>

#include "packager/base/logging.h"
#include "packager/base/numerics/safe_conversions.h"
#include "packager/media/base/media_sample.h"
#include "packager/media/base/offset_byte_queue.h"
#include "packager/media/base/timestamp.h"
#include "packager/media/base/video_stream_info.h"
#include "packager/media/codecs/h26x_byte_to_unit_stream_converter.h"
#include "packager/media/formats/mp2t/mp2t_common.h"

namespace shaka {
namespace media {
namespace mp2t {

namespace {

const int kStartCodeSize = 3;
const int kH264NaluHeaderSize = 1;
const int kH265NaluHeaderSize = 2;

}  // namespace

EsParserH26x::EsParserH26x(
    Nalu::CodecType type,
    std::unique_ptr<H26xByteToUnitStreamConverter> stream_converter,
    uint32_t pid,
    const EmitSampleCB& emit_sample_cb)
    : EsParser(pid),
      emit_sample_cb_(emit_sample_cb),
      type_(type),
      es_queue_(new media::OffsetByteQueue()),
      stream_converter_(std::move(stream_converter)) {}

EsParserH26x::~EsParserH26x() {}

bool EsParserH26x::Parse(const uint8_t* buf,
                         int size,
                         int64_t pts,
                         int64_t dts) {
  // Note: Parse is invoked each time a PES packet has been reassembled.
  // Unfortunately, a PES packet does not necessarily map
  // to an h264/h265 access unit, although the HLS recommendation is to use one
  // PES for each access unit (but this is just a recommendation and some
  // streams do not comply with this recommendation).

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

    // Warns if there are a large number of cached timestamps, which should be 1
    // or 2 if everythings works as expected.
    const size_t kWarningSize =
        24;  // An arbitrary number (it is 1 second for a fps of 24).
    LOG_IF(WARNING, timing_desc_list_.size() >= kWarningSize)
        << "Unusually large number of cached timestamps ("
        << timing_desc_list_.size() << ").";
  }

  // Add the incoming bytes to the ES queue.
  es_queue_->Push(buf, size);
  return ParseInternal();
}

bool EsParserH26x::Flush() {
  DVLOG(1) << "EsParserH26x::Flush";

  // Simulate two additional AUDs to force emitting the last access unit
  // which is assumed to be complete at this point.
  // Two AUDs are needed because the exact size of a NAL unit can only be
  // determined after seeing the next NAL unit, so we need a second AUD to
  // finish the parsing of the first AUD.
  if (type_ == Nalu::kH264) {
    const uint8_t aud[] = {0x00, 0x00, 0x01, 0x09, 0x00, 0x00, 0x01, 0x09};
    es_queue_->Push(aud, sizeof(aud));
  } else {
    DCHECK_EQ(Nalu::kH265, type_);
    const uint8_t aud[] = {0x00, 0x00, 0x01, 0x46, 0x01,
                           0x00, 0x00, 0x01, 0x46, 0x01};
    es_queue_->Push(aud, sizeof(aud));
  }

  RCHECK(ParseInternal());

  if (pending_sample_) {
    // Flush pending sample.
    if (!pending_sample_duration_) {
      pending_sample_duration_ = CalculateSampleDuration(pending_sample_pps_id_);
    }
    pending_sample_->set_duration(pending_sample_duration_);
    emit_sample_cb_.Run(std::move(pending_sample_));
  }
  return true;
}

void EsParserH26x::Reset() {
  es_queue_.reset(new media::OffsetByteQueue());
  current_search_position_ = 0;
  current_access_unit_position_ = 0;
  current_video_slice_info_.valid = false;
  next_access_unit_position_set_ = false;
  next_access_unit_position_ = 0;
  current_nalu_info_.reset();
  timing_desc_list_.clear();
  pending_sample_ = std::shared_ptr<MediaSample>();
  pending_sample_duration_ = 0;
  waiting_for_key_frame_ = true;
}

bool EsParserH26x::SearchForNalu(uint64_t* position, Nalu* nalu) {
  const uint8_t* es;
  int es_size;
  es_queue_->PeekAt(current_search_position_, &es, &es_size);

  // Find a start code.
  uint64_t start_code_offset;
  uint8_t start_code_size;
  const bool start_code_found = NaluReader::FindStartCode(
      es, es_size, &start_code_offset, &start_code_size);

  if (!start_code_found) {
    // We didn't find a start code, so we don't have to search this data again.
    if (es_size > kStartCodeSize)
      current_search_position_ += es_size - kStartCodeSize;
    return false;
  }

  // Ensure the next NAL unit is a real NAL unit.
  const uint8_t* next_nalu_ptr = es + start_code_offset + start_code_size;
  // This size is likely inaccurate, this is just to get the header info.
  const int64_t next_nalu_size = es_size - start_code_offset - start_code_size;
  if (next_nalu_size <
      (type_ == Nalu::kH264 ? kH264NaluHeaderSize : kH265NaluHeaderSize)) {
    // There was not enough data, wait for more.
    return false;
  }

  // Update search position for next nalu.
  current_search_position_ += start_code_offset + start_code_size;

  // |next_nalu_info_| is made global intentionally to avoid repetitive memory
  // allocation which could create memory fragments.
  if (!next_nalu_info_)
    next_nalu_info_.reset(new NaluInfo);
  if (!next_nalu_info_->nalu.Initialize(type_, next_nalu_ptr, next_nalu_size)) {
    // This NAL unit is invalid, skip it and search again.
    return SearchForNalu(position, nalu);
  }
  next_nalu_info_->position = current_search_position_ - start_code_size;
  next_nalu_info_->start_code_size = start_code_size;

  const bool current_nalu_set = current_nalu_info_ ? true : false;
  if (current_nalu_info_) {
    // Starting position for the nalu including start code.
    *position = current_nalu_info_->position;
    // Update the NALU because the data pointer may have been invalidated.
    const uint8_t* current_nalu_ptr =
        next_nalu_ptr +
        (current_nalu_info_->position + current_nalu_info_->start_code_size) -
        current_search_position_;
    const uint64_t current_nalu_size = next_nalu_info_->position -
                                       current_nalu_info_->position -
                                       current_nalu_info_->start_code_size;
    CHECK(nalu->Initialize(type_, current_nalu_ptr, current_nalu_size));
  }
  current_nalu_info_.swap(next_nalu_info_);
  return current_nalu_set ? true : SearchForNalu(position, nalu);
}

bool EsParserH26x::ParseInternal() {
  uint64_t position;
  Nalu nalu;
  VideoSliceInfo video_slice_info;
  while (SearchForNalu(&position, &nalu)) {
    // ITU H.264 sec. 7.4.1.2.3
    // H264: The first of the NAL units with |can_start_access_unit() == true|
    //   after the last VCL NAL unit of a primary coded picture specifies the
    //   start of a new access unit.
    // ITU H.265 sec. 7.4.2.4.4
    // H265: The first of the NAL units with |can_start_access_unit() == true|
    //   after the last VCL NAL unit preceding firstBlPicNalUnit (the first
    //   VCL NAL unit of a coded picture with nuh_layer_id equal to 0), if
    //   any, specifies the start of a new access unit.
    if (nalu.can_start_access_unit()) {
      if (!next_access_unit_position_set_) {
        next_access_unit_position_set_ = true;
        next_access_unit_position_ = position;
      }
      RCHECK(ProcessNalu(nalu, &video_slice_info));
      if (nalu.is_vcl() && !video_slice_info.valid) {
        // This could happen only if decoder config is not available yet. Drop
        // this frame.
        DCHECK(!current_video_slice_info_.valid);
        next_access_unit_position_set_ = false;
        continue;
      }
    } else if (nalu.is_vcl()) {
      // This isn't the first VCL NAL unit. Next access unit should start after
      // this NAL unit.
      next_access_unit_position_set_ = false;
      continue;
    }

    // AUD shall be the first NAL unit if present. There shall be at most one
    // AUD in any access unit. We can emit the current access unit which shall
    // not contain the AUD.
    if (nalu.is_aud()) {
      RCHECK(EmitCurrentAccessUnit());
      continue;
    }

    // We can only determine if the current access unit ends after seeing
    // another VCL NAL unit.
    if (!video_slice_info.valid)
      continue;

    // Check if it is the first VCL NAL unit of a primary coded picture. It is
    // always true for H265 as nuh_layer_id shall be == 0 at this point.
    bool is_first_vcl_nalu = true;
    if (type_ == Nalu::kH264) {
      if (current_video_slice_info_.valid) {
        // ITU H.264 sec. 7.4.1.2.4 Detection of the first VCL NAL unit of a
        // primary coded picture. Only pps_id and frame_num are checked here.
        is_first_vcl_nalu =
            video_slice_info.frame_num != current_video_slice_info_.frame_num ||
            video_slice_info.pps_id != current_video_slice_info_.pps_id;
      }
    }
    if (!is_first_vcl_nalu) {
      // This isn't the first VCL NAL unit. Next access unit should start after
      // this NAL unit.
      next_access_unit_position_set_ = false;
      continue;
    }

    DCHECK(next_access_unit_position_set_);
    RCHECK(EmitCurrentAccessUnit());

    // Delete the data we have already processed.
    es_queue_->Trim(next_access_unit_position_);

    current_access_unit_position_ = next_access_unit_position_;
    current_video_slice_info_ = video_slice_info;
    next_access_unit_position_set_ = false;
  }
  return true;
}

bool EsParserH26x::EmitCurrentAccessUnit() {
  if (current_video_slice_info_.valid) {
    if (current_video_slice_info_.is_key_frame)
      waiting_for_key_frame_ = false;
    if (!waiting_for_key_frame_) {
      RCHECK(
          EmitFrame(current_access_unit_position_,
                    next_access_unit_position_ - current_access_unit_position_,
                    current_video_slice_info_.is_key_frame,
                    current_video_slice_info_.pps_id));
    }
    current_video_slice_info_.valid = false;
  }
  return true;
}

bool EsParserH26x::EmitFrame(int64_t access_unit_pos,
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
  DVLOG(LOG_LEVEL_ES) << "Emit frame: stream_pos=" << access_unit_pos
                      << " size=" << access_unit_size << " pts "
                      << current_timing_desc.pts << " timing_desc_list size "
                      << timing_desc_list_.size();
  int es_size;
  const uint8_t* es;
  es_queue_->PeekAt(access_unit_pos, &es, &es_size);

  // Convert frame to unit stream format.
  std::vector<uint8_t> converted_frame;
  if (!stream_converter_->ConvertByteStreamToNalUnitStream(
          es, access_unit_size, &converted_frame)) {
    DLOG(ERROR) << "Failure to convert video frame to unit stream format.";
    return false;
  }

  // Update the video decoder configuration if needed.
  RCHECK(UpdateVideoDecoderConfig(pps_id));

  // Create the media sample, emitting always the previous sample after
  // calculating its duration.
  std::shared_ptr<MediaSample> media_sample = MediaSample::CopyFrom(
      converted_frame.data(), converted_frame.size(), is_key_frame);
  media_sample->set_dts(current_timing_desc.dts);
  media_sample->set_pts(current_timing_desc.pts);
  if (pending_sample_) {
    if (media_sample->dts() <= pending_sample_->dts()) {
      LOG(WARNING) << "[MPEG-2 TS] PID " << pid() << " dts "
                   << media_sample->dts()
                   << " less than or equal to previous dts "
                   << pending_sample_->dts();
      // Keep the sample but adjust the sample duration to a very small value,
      // in case that the sample is still needed for the decoding afterwards.
      const int64_t kArbitrarySmallDuration = 0.001 * kMpeg2Timescale;  // 1ms.
      pending_sample_->set_duration(kArbitrarySmallDuration);
    } else {
      int64_t sample_duration = media_sample->dts() - pending_sample_->dts();
      pending_sample_->set_duration(sample_duration);

      const int kArbitraryGapScale = 10;
      if (pending_sample_duration_ &&
          sample_duration > kArbitraryGapScale * pending_sample_duration_) {
        LOG(WARNING) << "[MPEG-2 TS] PID " << pid() << " Possible GAP at dts "
                     << pending_sample_->dts() << " with next sample at dts "
                     << media_sample->dts() << " (difference "
                     << sample_duration << ")";
      }

      pending_sample_duration_ = sample_duration;
    }
    emit_sample_cb_.Run(std::move(pending_sample_));
  }
  pending_sample_ = media_sample;
  pending_sample_pps_id_ = pps_id;

  return true;
}

}  // namespace mp2t
}  // namespace media
}  // namespace shaka
