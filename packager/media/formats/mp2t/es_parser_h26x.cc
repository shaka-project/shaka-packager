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
#include "packager/media/filters/h264_byte_to_unit_stream_converter.h"
#include "packager/media/filters/h265_byte_to_unit_stream_converter.h"
#include "packager/media/formats/mp2t/mp2t_common.h"

namespace edash_packager {
namespace media {
namespace mp2t {

namespace {

H26xByteToUnitStreamConverter* CreateStreamConverter(Nalu::CodecType type) {
  if (type == Nalu::kH264) {
    return new H264ByteToUnitStreamConverter();
  } else {
    DCHECK_EQ(Nalu::kH265, type);
    return new H265ByteToUnitStreamConverter();
  }
}

}  // anonymous namespace

EsParserH26x::EsParserH26x(Nalu::CodecType type,
                           uint32_t pid,
                           const EmitSampleCB& emit_sample_cb)
    : EsParser(pid),
      emit_sample_cb_(emit_sample_cb),
      type_(type),
      es_queue_(new media::OffsetByteQueue()),
      current_access_unit_pos_(0),
      found_access_unit_(false),
      stream_converter_(CreateStreamConverter(type)),
      pending_sample_duration_(0),
      waiting_for_key_frame_(true) {
}

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

  // Skip to the first access unit.
  if (!found_access_unit_) {
    if (!FindNextAccessUnit(current_access_unit_pos_,
                            &current_access_unit_pos_)) {
      return true;
    }
    es_queue_->Trim(current_access_unit_pos_);
    found_access_unit_ = true;
  }

  return ParseInternal();
}

void EsParserH26x::Flush() {
  DVLOG(1) << "EsParserH26x::Flush";

  // Simulate an additional AUD to force emitting the last access unit
  // which is assumed to be complete at this point.
  if (type_ == Nalu::kH264) {
    uint8_t aud[] = {0x00, 0x00, 0x01, 0x09};
    es_queue_->Push(aud, sizeof(aud));
  } else {
    DCHECK_EQ(Nalu::kH265, type_);
    uint8_t aud[] = {0x00, 0x00, 0x01, 0x46, 0x01};
    es_queue_->Push(aud, sizeof(aud));
  }
  ParseInternal();

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
  current_access_unit_pos_ = 0;
  found_access_unit_ = false;
  timing_desc_list_.clear();
  pending_sample_ = scoped_refptr<MediaSample>();
  pending_sample_duration_ = 0;
  waiting_for_key_frame_ = true;
}

bool EsParserH26x::FindNextAccessUnit(int64_t stream_pos,
                                      int64_t* next_unit_pos) {
  // TODO(modmaker): Avoid re-parsing by saving old position.
  // Every access unit must have a VCL entry and defines the end of the access
  // unit.  Track it to return on the element after it so we get the whole
  // access unit.
  bool seen_vcl_nalu = false;
  while (true) {
    const uint8_t* es;
    int size;
    es_queue_->PeekAt(stream_pos, &es, &size);

    // Find a start code.
    uint64_t start_code_offset;
    uint8_t start_code_size;
    bool start_code_found = NaluReader::FindStartCode(
        es, size, &start_code_offset, &start_code_size);
    stream_pos += start_code_offset;

    // No start code found or NALU type not available yet.
    if (!start_code_found ||
        start_code_offset + start_code_size >= static_cast<uint64_t>(size)) {
      return false;
    }

    Nalu nalu;
    const uint8_t* nalu_ptr = es + start_code_offset + start_code_size;
    size_t nalu_size = size - (start_code_offset + start_code_size);
    if (nalu.Initialize(type_, nalu_ptr, nalu_size)) {
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
      // TODO(modmaker): This does not handle nuh_layer_id != 0 correctly.
      // AUD  VCL  SEI  VCL*  VPS  VCL
      //         | Current method splits here.
      //                    | Should split here.
      // If we are searching for the first access unit, then stop at the first
      // NAL unit that can start an access unit.
      if ((seen_vcl_nalu || !found_access_unit_) &&
          nalu.can_start_access_unit()) {
        break;
      }
      bool is_vcl_nalu = nalu.is_video_slice() && nalu.nuh_layer_id() == 0;
      seen_vcl_nalu |= is_vcl_nalu;
    }

    // The current NALU is not an AUD, skip the start code
    // and continue parsing the stream.
    stream_pos += start_code_size;
  }

  *next_unit_pos = stream_pos;
  return true;
}

bool EsParserH26x::ParseInternal() {
  DCHECK_LE(es_queue_->head(), current_access_unit_pos_);
  DCHECK_LE(current_access_unit_pos_, es_queue_->tail());

  // Resume parsing later if no AUD was found.
  int64_t access_unit_end;
  if (!FindNextAccessUnit(current_access_unit_pos_, &access_unit_end))
    return true;

  // At this point, we know we have a full access unit.
  bool is_key_frame = false;
  int pps_id_for_access_unit = -1;

  const uint8_t* es;
  int size;
  es_queue_->PeekAt(current_access_unit_pos_, &es, &size);
  int access_unit_size = base::checked_cast<int, int64_t>(
      access_unit_end - current_access_unit_pos_);
  DCHECK_LE(access_unit_size, size);
  NaluReader reader(type_, kIsAnnexbByteStream, es, access_unit_size);

  // TODO(modmaker): Consider combining with FindNextAccessUnit to avoid
  // scanning the data twice.
  while (true) {
    Nalu nalu;
    bool is_eos = false;
    switch (reader.Advance(&nalu)) {
      case NaluReader::kOk:
        break;
      case NaluReader::kEOStream:
        is_eos = true;
        break;
      default:
        return false;
    }
    if (is_eos)
      break;

    if (!ProcessNalu(nalu, &is_key_frame, &pps_id_for_access_unit))
      return false;
  }

  if (waiting_for_key_frame_) {
    waiting_for_key_frame_ = !is_key_frame;
  }
  if (!waiting_for_key_frame_) {
    // Emit a frame and move the stream to the next AUD position.
    RCHECK(EmitFrame(current_access_unit_pos_, access_unit_size,
                     is_key_frame, pps_id_for_access_unit));
  }
  current_access_unit_pos_ = access_unit_end;
  es_queue_->Trim(current_access_unit_pos_);

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
}  // namespace edash_packager
