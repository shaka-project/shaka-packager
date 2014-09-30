// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_MP2T_ES_PARSER_H264_H_
#define MEDIA_FORMATS_MP2T_ES_PARSER_H264_H_

#include <stdint.h>

#include <list>
#include <utility>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "media/formats/mp2t/es_parser.h"

namespace edash_packager {
namespace media {

class H264ByteToUnitStreamConverter;
class H264Parser;
class OffsetByteQueue;
struct H264SPS;

namespace mp2t {

// Remark:
// In this h264 parser, frame splitting is based on AUD nals.
// Mpeg2 TS spec: "2.14 Carriage of Rec. ITU-T H.264 | ISO/IEC 14496-10 video"
// "Each AVC access unit shall contain an access unit delimiter NAL Unit;"
//
class EsParserH264 : public EsParser {
 public:
  EsParserH264(uint32_t pid,
               const NewStreamInfoCB& new_stream_info_cb,
               const EmitSampleCB& emit_sample_cb);
  virtual ~EsParserH264();

  // EsParser implementation overrides.
  virtual bool Parse(const uint8_t* buf,
                     int size,
                     int64_t pts,
                     int64_t dts) OVERRIDE;
  virtual void Flush() OVERRIDE;
  virtual void Reset() OVERRIDE;

 private:
  struct TimingDesc {
    int64_t dts;
    int64_t pts;
  };

  // Find the AUD located at or after |*stream_pos|.
  // Return true if an AUD is found.
  // If found, |*stream_pos| corresponds to the position of the AUD start code
  // in the stream. Otherwise, |*stream_pos| corresponds to the last position
  // of the start code parser.
  bool FindAUD(int64_t* stream_pos);

  // Resumes the H264 ES parsing.
  // Return true if successful.
  bool ParseInternal();

  // Emit a frame whose position in the ES queue starts at |access_unit_pos|.
  // Returns true if successful, false if no PTS is available for the frame.
  bool EmitFrame(int64_t access_unit_pos,
                 int access_unit_size,
                 bool is_key_frame,
                 int pps_id);

  // Update the video decoder config based on an H264 SPS.
  // Return true if successful.
  bool UpdateVideoDecoderConfig(const H264SPS* sps);

  // Callbacks to pass the stream configuration and the frames.
  NewStreamInfoCB new_stream_info_cb_;
  EmitSampleCB emit_sample_cb_;

  // Bytes of the ES stream that have not been emitted yet.
  scoped_ptr<media::OffsetByteQueue> es_queue_;
  std::list<std::pair<int64_t, TimingDesc> > timing_desc_list_;

  // H264 parser state.
  // - |current_access_unit_pos_| is pointing to an annexB syncword
  // representing the first NALU of an H264 access unit.
  scoped_ptr<H264Parser> h264_parser_;
  int64_t current_access_unit_pos_;
  int64_t next_access_unit_pos_;

  // Filter to convert H.264 Annex B byte stream to unit stream.
  scoped_ptr<H264ByteToUnitStreamConverter> stream_converter_;

  // Last video decoder config.
  scoped_refptr<StreamInfo> last_video_decoder_config_;
  bool decoder_config_check_pending_;

  // Frame for which we do not yet have a duration.
  scoped_refptr<MediaSample> pending_sample_;
  uint64_t pending_sample_duration_;

  // Indicates whether waiting for first key frame.
  bool waiting_for_key_frame_;
};

}  // namespace mp2t
}  // namespace media
}  // namespace edash_packager

#endif
