// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PACKAGER_MEDIA_FORMATS_MP2T_TS_SECTION_PSI_H_
#define PACKAGER_MEDIA_FORMATS_MP2T_TS_SECTION_PSI_H_

#include <packager/macros/classes.h>
#include <packager/media/base/byte_queue.h>
#include <packager/media/formats/mp2t/ts_section.h>

namespace shaka {
namespace media {

class BitReader;

namespace mp2t {

class TsSectionPsi : public TsSection {
 public:
  TsSectionPsi();
  ~TsSectionPsi() override;

  // TsSection implementation.
  bool Parse(bool payload_unit_start_indicator,
             const uint8_t* buf,
             int size) override;
  bool Flush() override;
  void Reset() override;

  // Parse the content of the PSI section.
  virtual bool ParsePsiSection(BitReader* bit_reader) = 0;

  // Reset the state of the PSI section.
  virtual void ResetPsiSection() = 0;

 private:
  void ResetPsiState();

  // Bytes of the current PSI.
  ByteQueue psi_byte_queue_;

  // Do not start parsing before getting a unit start indicator.
  bool wait_for_pusi_;

  // Number of leading bytes to discard (pointer field).
  int leading_bytes_to_discard_;

  DISALLOW_COPY_AND_ASSIGN(TsSectionPsi);
};

}  // namespace mp2t
}  // namespace media
}  // namespace shaka

#endif

