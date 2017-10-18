// Copyright 2016 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_FORMATS_MP2T_PROGRAM_MAP_TABLE_WRITER_H_
#define PACKAGER_MEDIA_FORMATS_MP2T_PROGRAM_MAP_TABLE_WRITER_H_

#include <stdint.h>

#include <vector>

#include "packager/media/base/buffer_writer.h"
// TODO(kqyang): Move codec to codec.h.
#include "packager/media/base/stream_info.h"
#include "packager/media/formats/mp2t/continuity_counter.h"

namespace shaka {
namespace media {

class BufferWriter;

namespace mp2t {

/// Puts PMT into TS packets and writes them to buffer.
class ProgramMapTableWriter {
 public:
  explicit ProgramMapTableWriter(Codec codec);
  virtual ~ProgramMapTableWriter() = default;

  /// Writes TS packets with PMT for encrypted segments.
  // Virtual for testing.
  virtual bool EncryptedSegmentPmt(BufferWriter* writer);

  /// Writes TS packets with PMT for clear segments.
  // Virtual for testing.
  virtual bool ClearSegmentPmt(BufferWriter* writer);

  // The pid can be 13 bits long but 8 bits is sufficient for this library.
  // This is the minimum PID that can be used for PMT.
  static const uint8_t kPmtPid = 0x20;

  // This is arbitrary number that is not reserved by the spec.
  static const uint8_t kElementaryPid = 0x50;

 protected:
  /// @return the underlying codec.
  Codec codec() const { return codec_; }

 private:
  ProgramMapTableWriter(const ProgramMapTableWriter&) = delete;
  ProgramMapTableWriter& operator=(const ProgramMapTableWriter&) = delete;

  // Writes descriptors for PMT (only needed for encrypted PMT).
  virtual bool WriteDescriptors(BufferWriter* writer) const = 0;

  const Codec codec_;
  ContinuityCounter continuity_counter_;
  BufferWriter clear_pmt_;
  BufferWriter encrypted_pmt_;
};

/// ProgramMapTableWriter for video codecs.
class VideoProgramMapTableWriter : public ProgramMapTableWriter {
 public:
  explicit VideoProgramMapTableWriter(Codec codec);
  ~VideoProgramMapTableWriter() override = default;

 private:
  VideoProgramMapTableWriter(const VideoProgramMapTableWriter&) = delete;
  VideoProgramMapTableWriter& operator=(const VideoProgramMapTableWriter&) =
      delete;

  bool WriteDescriptors(BufferWriter* writer) const override;
};

/// ProgramMapTableWriter for video codecs.
class AudioProgramMapTableWriter : public ProgramMapTableWriter {
 public:
  AudioProgramMapTableWriter(Codec codec,
                             const std::vector<uint8_t>& audio_specific_config);
  ~AudioProgramMapTableWriter() override = default;

 private:
  AudioProgramMapTableWriter(const AudioProgramMapTableWriter&) = delete;
  AudioProgramMapTableWriter& operator=(const AudioProgramMapTableWriter&) =
      delete;

  // Writers descriptors for PMT (only needed for encrypted PMT).
  bool WriteDescriptors(BufferWriter* descriptors) const override;

  const std::vector<uint8_t> audio_specific_config_;
};

}  // namespace mp2t
}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_FORMATS_MP2T_PROGRAM_MAP_TABLE_WRITER_H_
