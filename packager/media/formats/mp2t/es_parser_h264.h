// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PACKAGER_MEDIA_FORMATS_MP2T_ES_PARSER_H264_H_
#define PACKAGER_MEDIA_FORMATS_MP2T_ES_PARSER_H264_H_

#include <stdint.h>
#include <memory>
#include "packager/base/callback.h"
#include "packager/media/formats/mp2t/es_parser_h26x.h"

namespace shaka {
namespace media {

class H264Parser;

namespace mp2t {

// Remark:
// In this h264 parser, frame splitting is based on AUD nals.
// Mpeg2 TS spec: "2.14 Carriage of Rec. ITU-T H.264 | ISO/IEC 14496-10 video"
// "Each AVC access unit shall contain an access unit delimiter NAL Unit;"
//
class EsParserH264 : public EsParserH26x {
 public:
  EsParserH264(uint32_t pid,
               const NewStreamInfoCB& new_stream_info_cb,
               const EmitSampleCB& emit_sample_cb);
  ~EsParserH264() override;

  // EsParserH26x implementation override.
  void Reset() override;

 private:
  // Processes a NAL unit found in ParseInternal.
  bool ProcessNalu(const Nalu& nalu, VideoSliceInfo* video_slice_info) override;

  // Update the video decoder config based on an H264 SPS.
  // Return true if successful.
  bool UpdateVideoDecoderConfig(int sps_id) override;
  // Calculate video sample duration based on SPS data
  int64_t CalculateSampleDuration(int pps_id) override;
  // Callback to pass the stream configuration.
  NewStreamInfoCB new_stream_info_cb_;

  std::shared_ptr<StreamInfo> last_video_decoder_config_;
  bool decoder_config_check_pending_;

  std::unique_ptr<H264Parser> h264_parser_;
};

}  // namespace mp2t
}  // namespace media
}  // namespace shaka

#endif
