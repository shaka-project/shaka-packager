// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PACKAGER_MEDIA_FORMATS_MP2T_ES_PARSER_H26x_H_
#define PACKAGER_MEDIA_FORMATS_MP2T_ES_PARSER_H26x_H_

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
  bool Flush() override;
  void Reset() override;

 protected:
  struct VideoSliceInfo {
    bool valid = false;
    bool is_key_frame = false;
    // Both pps_id and frame_num are extracted from slice header (frame_num is
    // only for H.264).
    int pps_id = 0;
    int frame_num = 0;
  };

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
    uint64_t position = 0;
    uint8_t start_code_size = 0;
  };

  // Processes a NAL unit found in ParseInternal. |video_slice_info| should not
  // be null, it will contain the video slice info if it is a video slice nalu
  // and it is processed successfully; otherwise the |valid| member will be set
  // to false with other members untouched.
  virtual bool ProcessNalu(const Nalu& nalu,
                           VideoSliceInfo* video_slice_info) = 0;

  // Update the video decoder config.
  // Return true if successful.
  virtual bool UpdateVideoDecoderConfig(int pps_id) = 0;

  // Finds the NAL unit by finding the next start code.  This will modify the
  // search position.
  // Returns true when it has found the NALU.
  bool SearchForNalu(uint64_t* position, Nalu* nalu);

  // Resumes the H26x ES parsing.
  // Return true if successful.
  bool ParseInternal();

  // Emit the current access unit if exists.
  bool EmitCurrentAccessUnit();

  // Emit a frame whose position in the ES queue starts at |access_unit_pos|.
  // Returns true if successful, false if no PTS is available for the frame.
  bool EmitFrame(int64_t access_unit_pos,
                 int access_unit_size,
                 bool is_key_frame,
                 int pps_id);
  // Calculates frame duration based on SPS frame data
  virtual int64_t CalculateSampleDuration(int pps_id) = 0;

  // Callback to pass the frames.
  EmitSampleCB emit_sample_cb_;

  // The type of stream being parsed.
  Nalu::CodecType type_;

  // Bytes of the ES stream that have not been emitted yet.
  std::unique_ptr<media::OffsetByteQueue> es_queue_;
  std::list<std::pair<int64_t, TimingDesc>> timing_desc_list_;

  // Parser state.
  // The position of the search head.
  uint64_t current_search_position_ = 0;
  // Current access unit starting position.
  uint64_t current_access_unit_position_ = 0;
  // The VideoSliceInfo in the current access unit, useful for first vcl nalu
  // detection (for H.264).
  VideoSliceInfo current_video_slice_info_;
  bool next_access_unit_position_set_ = false;
  uint64_t next_access_unit_position_ = 0;
  // Current nalu information.
  std::unique_ptr<NaluInfo> current_nalu_info_;
  // This is really a temporary storage for the next nalu information.
  std::unique_ptr<NaluInfo> next_nalu_info_;

  // Filter to convert H.264/H.265 Annex B byte stream to unit stream.
  std::unique_ptr<H26xByteToUnitStreamConverter> stream_converter_;

  // Frame for which we do not yet have a duration.
  std::shared_ptr<MediaSample> pending_sample_;
  int pending_sample_pps_id_ = -1;
  int64_t pending_sample_duration_ = 0;

  // Indicates whether waiting for first key frame.
  bool waiting_for_key_frame_ = true;
};

}  // namespace mp2t
}  // namespace media
}  // namespace shaka

#endif
