// Copyright 2016 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_FORMATS_MP2T_PROGRAM_MAP_TABLE_WRITER_H_
#define PACKAGER_MEDIA_FORMATS_MP2T_PROGRAM_MAP_TABLE_WRITER_H_

#include <stdint.h>

#include <vector>

#include "packager/base/macros.h"

namespace shaka {
namespace media {

class BufferWriter;

namespace mp2t {

class ContinuityCounter;

/// Puts PMT into TS packets and writes them to buffer.
/// Note that this does not currently allow encryption without clear lead.
class ProgramMapTableWriter {
 public:
  ProgramMapTableWriter();
  virtual ~ProgramMapTableWriter();

  /// Writes TS packets with PMT for encrypted segments.
  virtual bool EncryptedSegmentPmt(BufferWriter* writer) = 0;

  /// Writes TS packets with PMT for clear segments.
  virtual bool ClearSegmentPmt(BufferWriter* writer) = 0;

  // The pid can be 13 bits long but 8 bits is sufficient for this library.
  // This is the minimum PID that can be used for PMT.
  static const uint8_t kPmtPid = 0x20;

  // This is arbitrary number that is not reserved by the spec.
  static const uint8_t kElementaryPid = 0x50;
};

/// <em>This is not a general purpose PMT writer. This is intended to be used by
/// TsWriter.</em>
class H264ProgramMapTableWriter : public ProgramMapTableWriter {
 public:
  explicit H264ProgramMapTableWriter(ContinuityCounter* continuity_counter);
  ~H264ProgramMapTableWriter() override;

  bool EncryptedSegmentPmt(BufferWriter* writer) override;
  bool ClearSegmentPmt(BufferWriter* writer) override;

 private:
  ContinuityCounter* const continuity_counter_;
  // Set to true if ClearLeadSegmentPmt() has been called. This determines the
  // version number set in EncryptedSegmentPmt().
  bool has_clear_lead_ = false;

  DISALLOW_COPY_AND_ASSIGN(H264ProgramMapTableWriter);
};

// TODO(rkuroiwa): For now just handle AAC, we would want AudioProgramMapTable
// later when we support other audio codecs.
/// <em>This is not a general purpose PMT writer. This is intended to be used by
/// TsWriter.</em>
class AacProgramMapTableWriter : public ProgramMapTableWriter {
 public:
  AacProgramMapTableWriter(
      const std::vector<uint8_t>& aac_audio_specific_config,
      ContinuityCounter* continuity_counter);
  ~AacProgramMapTableWriter() override;

  bool EncryptedSegmentPmt(BufferWriter* writer) override;
  bool ClearSegmentPmt(BufferWriter* writer) override;

 private:
  bool EncryptedSegmentPmtWithParameters(int version,
                                         int current_next_indicator,
                                         BufferWriter* writer);

  const std::vector<uint8_t> aac_audio_specific_config_;
  ContinuityCounter* const continuity_counter_;
  // Set to true if ClearLeadSegmentPmt() has been called. This determines the
  // version number set in EncryptedSegmentPmt().
  bool has_clear_lead_ = false;

  DISALLOW_COPY_AND_ASSIGN(AacProgramMapTableWriter);
};

}  // namespace mp2t
}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_FORMATS_MP2T_PROGRAM_MAP_TABLE_WRITER_H_
