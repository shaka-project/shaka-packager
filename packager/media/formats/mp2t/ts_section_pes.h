// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PACKAGER_MEDIA_FORMATS_MP2T_TS_SECTION_PES_H_
#define PACKAGER_MEDIA_FORMATS_MP2T_TS_SECTION_PES_H_

#include <cstdint>
#include <memory>

#include <packager/macros/classes.h>
#include <packager/media/base/byte_queue.h>
#include <packager/media/formats/mp2t/ts_section.h>

namespace shaka {
namespace media {
namespace mp2t {

class EsParser;

class TsSectionPes : public TsSection {
 public:
  explicit TsSectionPes(std::unique_ptr<EsParser> es_parser);
  ~TsSectionPes() override;

  // TsSection implementation.
  bool Parse(bool payload_unit_start_indicator,
             const uint8_t* buf,
             int size) override;
  bool Flush() override;
  void Reset() override;

 private:
  // Emit a reassembled PES packet.
  // Return true if successful.
  // |emit_for_unknown_size| is used to force emission for PES packets
  // whose size is unknown.
  bool Emit(bool emit_for_unknown_size);

  // Parse a PES packet, return true if successful.
  bool ParseInternal(const uint8_t* raw_pes, int raw_pes_size);

  void ResetPesState();

  // Bytes of the current PES.
  ByteQueue pes_byte_queue_;

  // ES parser.
  std::unique_ptr<EsParser> es_parser_;

  // Do not start parsing before getting a unit start indicator.
  bool wait_for_pusi_;

  // Used to unroll PTS and DTS.
  bool previous_pts_valid_;
  int64_t previous_pts_;
  bool previous_dts_valid_;
  int64_t previous_dts_;

  DISALLOW_COPY_AND_ASSIGN(TsSectionPes);
};

}  // namespace mp2t
}  // namespace media
}  // namespace shaka

#endif

