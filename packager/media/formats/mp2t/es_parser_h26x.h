// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_MP2T_ES_PARSER_H26x_H_
#define MEDIA_FORMATS_MP2T_ES_PARSER_H26x_H_

#include <stdint.h>

#include <deque>
#include <list>
#include <memory>

#include "packager/base/callback.h"
#include "packager/base/compiler_specific.h"
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
               std::unique_ptr<H26xByteToUnitStreamConverter> stream_converter,
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
  struct NaluInfo {
    // NOTE: Nalu does not own the memory pointed by its data pointers.  The
    // caller owns and maintains the memory.
    Nalu nalu;
    // The offset of the NALU from the beginning of the stream, usable as an
    // argument to OffsetByteQueue.  This points to the start code.
    uint64_t position;
    uint8_t start_code_size;
  };

  // Processes a NAL unit found in ParseInternal.  The @a pps_id_for_access_unit
  // value will be passed to UpdateVideoDecoderConfig.
  virtual bool ProcessNalu(const Nalu& nalu,
                           bool* is_key_frame,
                           int* pps_id_for_access_unit) = 0;

  // Update the video decoder config.
  // Return true if successful.
  virtual bool UpdateVideoDecoderConfig(int pps_id) = 0;

  // Skips to the first access unit available.  Returns whether an access unit
  // is found.
  bool SkipToFirstAccessUnit();

  // Finds the next NAL unit by finding the next start code.  This will modify
  // the search position.
  // Returns true when it has found the next NALU.
  bool SearchForNextNalu();

  // Process an access unit that spans the given NAL units (end is exclusive
  // and should point to a valid object).
  bool ProcessAccessUnit(std::deque<NaluInfo>::iterator end);

  // Resumes the H26x ES parsing.
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
  std::unique_ptr<media::OffsetByteQueue> es_queue_;
  std::list<std::pair<int64_t, TimingDesc>> timing_desc_list_;

  // Parser state.
  // The position of the search head.
  uint64_t current_search_position_;
  // The NALU that make up the current access unit.  This may include elements
  // from the next access unit.  The last item is the NAL unit currently
  // being processed.
  std::deque<NaluInfo> access_unit_nalus_;

  // Filter to convert H.264/H.265 Annex B byte stream to unit stream.
  std::unique_ptr<H26xByteToUnitStreamConverter> stream_converter_;

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
