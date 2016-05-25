// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "packager/media/formats/mp2t/es_parser_h264.h"

#include <stdint.h>

#include "packager/base/logging.h"
#include "packager/media/base/media_sample.h"
#include "packager/media/base/timestamp.h"
#include "packager/media/base/video_stream_info.h"
#include "packager/media/filters/avc_decoder_configuration_record.h"
#include "packager/media/filters/h264_byte_to_unit_stream_converter.h"
#include "packager/media/filters/h264_parser.h"
#include "packager/media/formats/mp2t/mp2t_common.h"

namespace shaka {
namespace media {
namespace mp2t {

EsParserH264::EsParserH264(uint32_t pid,
                           const NewStreamInfoCB& new_stream_info_cb,
                           const EmitSampleCB& emit_sample_cb)
    : EsParserH26x(Nalu::kH264,
                   scoped_ptr<H26xByteToUnitStreamConverter>(
                       new H264ByteToUnitStreamConverter()),
                   pid,
                   emit_sample_cb),
      new_stream_info_cb_(new_stream_info_cb),
      decoder_config_check_pending_(false),
      h264_parser_(new H264Parser()) {}

EsParserH264::~EsParserH264() {}

void EsParserH264::Reset() {
  DVLOG(1) << "EsParserH264::Reset";
  h264_parser_.reset(new H264Parser());
  last_video_decoder_config_ = scoped_refptr<StreamInfo>();
  decoder_config_check_pending_ = false;
  EsParserH26x::Reset();
}

bool EsParserH264::ProcessNalu(const Nalu& nalu,
                               bool* is_key_frame,
                               int* pps_id_for_access_unit) {
  switch (nalu.type()) {
    case Nalu::H264_AUD: {
      DVLOG(LOG_LEVEL_ES) << "Nalu: AUD";
      break;
    }
    case Nalu::H264_SPS: {
      DVLOG(LOG_LEVEL_ES) << "Nalu: SPS";
      int sps_id;
      if (h264_parser_->ParseSps(nalu, &sps_id) != H264Parser::kOk)
        return false;
      decoder_config_check_pending_ = true;
      break;
    }
    case Nalu::H264_PPS: {
      DVLOG(LOG_LEVEL_ES) << "Nalu: PPS";
      int pps_id;
      if (h264_parser_->ParsePps(nalu, &pps_id) != H264Parser::kOk) {
        // Allow PPS parsing to fail if waiting for SPS.
        if (last_video_decoder_config_)
          return false;
      } else {
        decoder_config_check_pending_ = true;
      }
      break;
    }
    case Nalu::H264_IDRSlice:
    case Nalu::H264_NonIDRSlice: {
      *is_key_frame = (nalu.type() == Nalu::H264_IDRSlice);
      DVLOG(LOG_LEVEL_ES) << "Nalu: slice IDR=" << is_key_frame;
      H264SliceHeader shdr;
      if (h264_parser_->ParseSliceHeader(nalu, &shdr) != H264Parser::kOk) {
        // Only accept an invalid SPS/PPS at the beginning when the stream
        // does not necessarily start with an SPS/PPS/IDR.
        if (last_video_decoder_config_)
          return false;
      } else {
        *pps_id_for_access_unit = shdr.pic_parameter_set_id;
      }
      break;
    }
    default: {
      DVLOG(LOG_LEVEL_ES) << "Nalu: " << nalu.type();
    }
  }

  return true;
}

bool EsParserH264::UpdateVideoDecoderConfig(int pps_id) {
  // Update the video decoder configuration if needed.
  if (!decoder_config_check_pending_)
    return true;

  const H264Pps* pps = h264_parser_->GetPps(pps_id);
  const H264Sps* sps;
  if (!pps) {
    // Only accept an invalid PPS at the beginning when the stream
    // does not necessarily start with an SPS/PPS/IDR.
    // In this case, the initial frames are conveyed to the upper layer with
    // an invalid VideoDecoderConfig and it's up to the upper layer
    // to process this kind of frame accordingly.
    return last_video_decoder_config_ == nullptr;
  } else {
    sps = h264_parser_->GetSps(pps->seq_parameter_set_id);
    if (!sps)
      return false;
    decoder_config_check_pending_ = false;
  }

  std::vector<uint8_t> decoder_config_record;
  if (!stream_converter()->GetDecoderConfigurationRecord(
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

  uint32_t coded_width = 0;
  uint32_t coded_height = 0;
  uint32_t pixel_width = 0;
  uint32_t pixel_height = 0;
  if (!ExtractResolutionFromSps(*sps, &coded_width, &coded_height, &pixel_width,
                                &pixel_height)) {
    LOG(ERROR) << "Failed to parse SPS.";
    return false;
  }

  last_video_decoder_config_ = scoped_refptr<StreamInfo>(new VideoStreamInfo(
      pid(), kMpeg2Timescale, kInfiniteDuration, kCodecH264,
      AVCDecoderConfigurationRecord::GetCodecString(decoder_config_record[1],
                                                    decoder_config_record[2],
                                                    decoder_config_record[3]),
      std::string(), coded_width, coded_height, pixel_width, pixel_height, 0,
      H264ByteToUnitStreamConverter::kUnitStreamNaluLengthSize,
      decoder_config_record.data(), decoder_config_record.size(), false));
  DVLOG(1) << "Profile IDC: " << sps->profile_idc;
  DVLOG(1) << "Level IDC: " << sps->level_idc;
  DVLOG(1) << "log2_max_frame_num_minus4: " << sps->log2_max_frame_num_minus4;

  // Video config notification.
  new_stream_info_cb_.Run(last_video_decoder_config_);

  return true;
}

}  // namespace mp2t
}  // namespace media
}  // namespace shaka
