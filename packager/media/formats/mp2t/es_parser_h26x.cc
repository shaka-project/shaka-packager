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
      current_search_position_(0),
      stream_converter_(std::move(stream_converter)),
      pending_sample_duration_(0),
      waiting_for_key_frame_(true) {}

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
  }

  // Add the incoming bytes to the ES queue.
  es_queue_->Push(buf, size);

  // We should always have entries in the vector and it should always start
  // with |can_start_access_unit == true|.  If not, we are just starting and
  // should skip to the first access unit.
  if (access_unit_nalus_.empty()) {
    if (!SkipToFirstAccessUnit())
      return true;
  }
  DCHECK(!access_unit_nalus_.empty());
  DCHECK(access_unit_nalus_.front().nalu.can_start_access_unit());

  return ParseInternal();
}

void EsParserH26x::Flush() {
  DVLOG(1) << "EsParserH26x::Flush";

  // Simulate an additional AUD to force emitting the last access unit
  // which is assumed to be complete at this point.
  if (type_ == Nalu::kH264) {
    const uint8_t aud[] = {0x00, 0x00, 0x01, 0x09};
    es_queue_->Push(aud, sizeof(aud));
  } else {
    DCHECK_EQ(Nalu::kH265, type_);
    const uint8_t aud[] = {0x00, 0x00, 0x01, 0x46, 0x01};
    es_queue_->Push(aud, sizeof(aud));
  }

  CHECK(ParseInternal());

  // Note that the end argument is exclusive.  We do not want to include the
  // fake AUD we just added, so the argument should point to the AUD.
  if (access_unit_nalus_.size() > 1 &&
      !ProcessAccessUnit(access_unit_nalus_.end() - 1)) {
    LOG(WARNING) << "Error processing last access unit.";
  }

  if (pending_sample_) {
    // Flush pending sample.
    DCHECK(pending_sample_duration_);
    pending_sample_->set_duration(pending_sample_duration_);
    emit_sample_cb_.Run(pid(), pending_sample_);
    pending_sample_ = scoped_refptr<MediaSample>();
  }
}

void EsParserH26x::Reset() {
  es_queue_.reset(new media::OffsetByteQueue());
  current_search_position_ = 0;
  access_unit_nalus_.clear();
  timing_desc_list_.clear();
  pending_sample_ = scoped_refptr<MediaSample>();
  pending_sample_duration_ = 0;
  waiting_for_key_frame_ = true;
}

bool EsParserH26x::SkipToFirstAccessUnit() {
  DCHECK(access_unit_nalus_.empty());
  while (access_unit_nalus_.empty()) {
    if (!SearchForNextNalu())
      return false;

    // If we can't start an access unit, remove it and continue.
    DCHECK_EQ(1u, access_unit_nalus_.size());
    if (!access_unit_nalus_.back().nalu.can_start_access_unit())
      access_unit_nalus_.clear();
  }
  return true;
}

bool EsParserH26x::SearchForNextNalu() {
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
  const uint8_t* nalu_ptr = es + start_code_offset + start_code_size;
  // This size is likely inaccurate, this is just to get the header info.
  const int64_t next_nalu_size = es_size - start_code_offset - start_code_size;
  if (next_nalu_size <
      (type_ == Nalu::kH264 ? kH264NaluHeaderSize : kH265NaluHeaderSize)) {
    // There was not enough data, wait for more.
    return false;
  }

  Nalu next_nalu;
  if (!next_nalu.Initialize(type_, nalu_ptr, next_nalu_size)) {
    // The next NAL unit is invalid, skip it and search again.
    current_search_position_ += start_code_offset + start_code_size;
    return SearchForNextNalu();
  }

  current_search_position_ += start_code_offset + start_code_size;

  NaluInfo info;
  info.position = current_search_position_ - start_code_size;
  info.start_code_size = start_code_size;
  info.nalu = next_nalu;
  access_unit_nalus_.push_back(info);

  return true;
}

bool EsParserH26x::ProcessAccessUnit(std::deque<NaluInfo>::iterator end) {
  DCHECK(end < access_unit_nalus_.end());
  auto begin = access_unit_nalus_.begin();
  const uint8_t* es;
  int es_size;
  es_queue_->PeekAt(begin->position, &es, &es_size);
  DCHECK_GE(static_cast<uint64_t>(es_size), (end->position - begin->position));

  // Process the NAL units in the access unit.
  bool is_key_frame = false;
  int pps_id = -1;
  for (auto it = begin; it != end; ++it) {
    if (it->nalu.nuh_layer_id() == 0) {
      // Update the NALU because the data pointer may have been invalidated.
      CHECK(it->nalu.Initialize(
          type_, es + (it->position - begin->position) + it->start_code_size,
          ((it+1)->position - it->position) - it->start_code_size));
      if (!ProcessNalu(it->nalu, &is_key_frame, &pps_id))
        return false;
    }
  }

  if (is_key_frame)
    waiting_for_key_frame_ = false;
  if (!waiting_for_key_frame_) {
    const uint64_t access_unit_size = end->position - begin->position;
    RCHECK(EmitFrame(begin->position, access_unit_size, is_key_frame, pps_id));
  }

  return true;
}

bool EsParserH26x::ParseInternal() {
  while (true) {
    if (!SearchForNextNalu())
      return true;

    // ITU H.264 sec. 7.4.1.2.3
    // H264: The first of the NAL units with |can_start_access_unit() == true|
    //   after the last VCL NAL unit of a primary coded picture specifies the
    //   start of a new access unit. |nuh_layer_id()| is for H265 only; it is
    //   included below for ease of computation (the value is always 0).
    // ITU H.265 sec. 7.4.2.4.4
    // H265: The first of the NAL units with |can_start_access_unit() == true|
    //   after the last VCL NAL unit preceding firstBlPicNalUnit (the first
    //   VCL NAL unit of a coded picture with nuh_layer_id equal to 0), if
    //   any, specifies the start of a new access unit.
    DCHECK(!access_unit_nalus_.empty());
    if (!access_unit_nalus_.back().nalu.is_video_slice() ||
        access_unit_nalus_.back().nalu.nuh_layer_id() != 0) {
      continue;
    }

    // First, find the end of the access unit.  Search backward to find the
    // first VCL NALU before the current one.
    auto access_unit_end_rit = access_unit_nalus_.rbegin();
    bool found_vcl = false;
    for (auto rit = access_unit_nalus_.rbegin() + 1;
         rit != access_unit_nalus_.rend(); ++rit) {
      if (rit->nalu.is_video_slice()) {
        found_vcl = true;
        break;
      } else if (rit->nalu.can_start_access_unit()) {
        // The start of the next access unit is the first unit with
        // |can_start_access_unit| after the previous VCL unit.
        access_unit_end_rit = rit;
      }
    }
    if (!found_vcl)
      return true;

    // Get a forward iterator that corresponds to the same element pointed by
    // |access_unit_end_rit|. Note: |end| refers to the exclusive end and
    // will point to a valid object.
    auto end = (access_unit_end_rit + 1).base();
    if (!ProcessAccessUnit(end))
      return false;

    // Delete the data we have already processed.
    es_queue_->Trim(end->position);
    access_unit_nalus_.erase(access_unit_nalus_.begin(), end);
  }
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
                      << " size=" << access_unit_size;
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

}  // namespace mp2t
}  // namespace media
}  // namespace shaka
