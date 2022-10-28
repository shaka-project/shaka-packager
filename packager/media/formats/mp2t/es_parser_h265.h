// Copyright 2016 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_FORMATS_MP2T_ES_PARSER_H265_H_
#define PACKAGER_MEDIA_FORMATS_MP2T_ES_PARSER_H265_H_

#include <stdint.h>

#include <list>
#include <memory>
#include <utility>

#include "packager/base/callback.h"
#include "packager/base/compiler_specific.h"
#include "packager/media/formats/mp2t/es_parser_h26x.h"

namespace shaka {
namespace media {

class H265Parser;

namespace mp2t {

class EsParserH265 : public EsParserH26x {
 public:
  EsParserH265(uint32_t pid,
               const NewStreamInfoCB& new_stream_info_cb,
               const EmitSampleCB& emit_sample_cb);
  ~EsParserH265() override;

  // EsParserH26x implementation override.
  void Reset() override;

 private:
  // Processes a NAL unit found in ParseInternal.
  bool ProcessNalu(const Nalu& nalu, VideoSliceInfo* video_slice_info) override;

  // Update the video decoder config based on an H264 SPS.
  // Return true if successful.
  bool UpdateVideoDecoderConfig(int sps_id) override;

  int64_t CalculateSampleDuration(int pps_id) override;
  // Callback to pass the stream configuration.
  NewStreamInfoCB new_stream_info_cb_;

  // Last video decoder config.
  std::shared_ptr<StreamInfo> last_video_decoder_config_;
  bool decoder_config_check_pending_;

  std::unique_ptr<H265Parser> h265_parser_;
};

}  // namespace mp2t
}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_FORMATS_MP2T_ES_PARSER_H265_H_
