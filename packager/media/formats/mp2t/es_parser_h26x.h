// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_MP2T_ES_PARSER_H26x_H_
#define MEDIA_FORMATS_MP2T_ES_PARSER_H26x_H_

#include <stdint.h>

#include <list>

#include "packager/base/callback.h"
#include "packager/base/compiler_specific.h"
#include "packager/base/memory/scoped_ptr.h"
#include "packager/media/codecs/nalu_reader.h"
#include "packager/media/formats/mp2t/es_parser.h"

namespace shaka {
namespace media {

class H26xByteToUnitStreamConverter;
class OffsetByteQueue;

namespace mp2t {

// A base class for common code between the H.264/H.265 es parsers.
class EsParserH26x : public EsParser {
 public:
  EsParserH26x(Nalu::CodecType type,
               scoped_ptr<H26xByteToUnitStreamConverter> stream_converter,
               uint32_t pid,
               const EmitSampleCB& emit_sample_cb);
  ~EsParserH26x() override;

  // EsParser implementation overrides.
  bool Parse(const uint8_t* buf, int size, int64_t pts, int64_t dts) override;
  void Flush() override;
  void Reset() override;

 protected:
  const H26xByteToUnitStreamConverter* stream_converter() const {
    return stream_converter_.get();
  }

 private:
  struct TimingDesc {
    int64_t dts;
    int64_t pts;
  };

  // Processes a NAL unit found in ParseInternal.  The @a pps_id_for_access_unit
  // value will be passed to UpdateVideoDecoderConfig.
  virtual bool ProcessNalu(const Nalu& nalu,
                           bool* is_key_frame,
                           int* pps_id_for_access_unit) = 0;

  // Update the video decoder config.
  // Return true if successful.
  virtual bool UpdateVideoDecoderConfig(int pps_id) = 0;

  // Find the start of the next access unit staring at |stream_pos|.
  // Return true if the end is found.
  // If found, |*next_unit_start| contains the start of the next access unit.
  // Otherwise, |*next_unit_start| is unchanged.
  bool FindNextAccessUnit(int64_t stream_pos, int64_t* next_unit_start);

  // Resumes the H264 ES parsing.
  // Return true if successful.
  bool ParseInternal();

  // Emit a frame whose position in the ES queue starts at |access_unit_pos|.
  // Returns true if successful, false if no PTS is available for the frame.
  bool EmitFrame(int64_t access_unit_pos,
                 int access_unit_size,
                 bool is_key_frame,
                 int pps_id);

  // Callback to pass the frames.
  EmitSampleCB emit_sample_cb_;

  // The type of stream being parsed.
  Nalu::CodecType type_;

  // Bytes of the ES stream that have not been emitted yet.
  scoped_ptr<media::OffsetByteQueue> es_queue_;
  std::list<std::pair<int64_t, TimingDesc>> timing_desc_list_;

  // Parser state.
  // - |current_access_unit_pos_| is pointing to an annexB syncword
  // representing the first NALU of an access unit.
  int64_t current_access_unit_pos_;
  bool found_access_unit_;

  // Filter to convert H.264/H.265 Annex B byte stream to unit stream.
  scoped_ptr<H26xByteToUnitStreamConverter> stream_converter_;

  // Frame for which we do not yet have a duration.
  scoped_refptr<MediaSample> pending_sample_;
  uint64_t pending_sample_duration_;

  // Indicates whether waiting for first key frame.
  bool waiting_for_key_frame_;
};

}  // namespace mp2t
}  // namespace media
}  // namespace shaka

#endif
