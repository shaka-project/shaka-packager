// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PACKAGER_MEDIA_FORMATS_MP2T_TS_SECTION_PAT_H_
#define PACKAGER_MEDIA_FORMATS_MP2T_TS_SECTION_PAT_H_

#include <functional>

#include <packager/macros/classes.h>
#include <packager/media/formats/mp2t/ts_section_psi.h>

namespace shaka {
namespace media {
namespace mp2t {

class TsSectionPat : public TsSectionPsi {
 public:
  // RegisterPmtCb::Run(int program_number, int pmt_pid);
  typedef std::function<void(int, int)> RegisterPmtCb;

  explicit TsSectionPat(const RegisterPmtCb& register_pmt_cb);
  ~TsSectionPat() override;

  // TsSectionPsi implementation.
  bool ParsePsiSection(BitReader* bit_reader) override;
  void ResetPsiSection() override;

 private:
  RegisterPmtCb register_pmt_cb_;

  // Parameters from the PAT.
  int version_number_;

  DISALLOW_COPY_AND_ASSIGN(TsSectionPat);
};

}  // namespace mp2t
}  // namespace media
}  // namespace shaka

#endif

