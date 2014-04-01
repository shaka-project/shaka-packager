// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_MP2T_ES_PARSER_H264_H_
#define MEDIA_FORMATS_MP2T_ES_PARSER_H264_H_

#include <list>
#include <utility>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "media/base/media_export.h"
#include "media/base/video_decoder_config.h"
#include "media/formats/mp2t/es_parser.h"

namespace media {
class H264Parser;
struct H264SPS;
class OffsetByteQueue;
}

namespace media {
namespace mp2t {

// Remark:
// In this h264 parser, frame splitting is based on AUD nals.
// Mpeg2 TS spec: "2.14 Carriage of Rec. ITU-T H.264 | ISO/IEC 14496-10 video"
// "Each AVC access unit shall contain an access unit delimiter NAL Unit;"
//
class MEDIA_EXPORT EsParserH264 : NON_EXPORTED_BASE(public EsParser) {
 public:
  typedef base::Callback<void(const VideoDecoderConfig&)> NewVideoConfigCB;

  EsParserH264(const NewVideoConfigCB& new_video_config_cb,
               const EmitBufferCB& emit_buffer_cb);
  virtual ~EsParserH264();

  // EsParser implementation.
  virtual bool Parse(const uint8* buf, int size,
                     base::TimeDelta pts,
                     base::TimeDelta dts) OVERRIDE;
  virtual void Flush() OVERRIDE;
  virtual void Reset() OVERRIDE;

 private:
  struct TimingDesc {
    base::TimeDelta dts;
    base::TimeDelta pts;
  };

  // Find the AUD located at or after |*stream_pos|.
  // Return true if an AUD is found.
  // If found, |*stream_pos| corresponds to the position of the AUD start code
  // in the stream. Otherwise, |*stream_pos| corresponds to the last position
  // of the start code parser.
  bool FindAUD(int64* stream_pos);

  // Resumes the H264 ES parsing.
  // Return true if successful.
  bool ParseInternal();

  // Emit a frame whose position in the ES queue starts at |access_unit_pos|.
  // Returns true if successful, false if no PTS is available for the frame.
  bool EmitFrame(int64 access_unit_pos, int access_unit_size,
                 bool is_key_frame, int pps_id);

  // Update the video decoder config based on an H264 SPS.
  // Return true if successful.
  bool UpdateVideoDecoderConfig(const H264SPS* sps);

  // Callbacks to pass the stream configuration and the frames.
  NewVideoConfigCB new_video_config_cb_;
  EmitBufferCB emit_buffer_cb_;

  // Bytes of the ES stream that have not been emitted yet.
  scoped_ptr<media::OffsetByteQueue> es_queue_;
  std::list<std::pair<int64, TimingDesc> > timing_desc_list_;

  // H264 parser state.
  // - |current_access_unit_pos_| is pointing to an annexB syncword
  // representing the first NALU of an H264 access unit.
  scoped_ptr<H264Parser> h264_parser_;
  int64 current_access_unit_pos_;
  int64 next_access_unit_pos_;

  // Last video decoder config.
  VideoDecoderConfig last_video_decoder_config_;
};

}  // namespace mp2t
}  // namespace media

#endif

