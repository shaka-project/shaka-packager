// Copyright 2017 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_FORMATS_MP2T_AC3_HEADER_H_
#define PACKAGER_MEDIA_FORMATS_MP2T_AC3_HEADER_H_

#include <stdint.h>

#include <vector>

#include "packager/media/formats/mp2t/audio_header.h"

namespace shaka {
namespace media {
namespace mp2t {

/// Class which parses AC3 frame (header / metadata) and synthesizes
/// AudioSpecificConfig from audio frame content.
class Ac3Header : public AudioHeader {
 public:
  Ac3Header() = default;
  ~Ac3Header() override = default;

  /// @name AudioHeader implementation overrides.
  /// @{
  bool IsSyncWord(const uint8_t* buf) const override;
  size_t GetMinFrameSize() const override;
  size_t GetSamplesPerFrame() const override;
  bool Parse(const uint8_t* adts_frame, size_t adts_frame_size) override;
  size_t GetHeaderSize() const override;
  size_t GetFrameSize() const override;
  void GetAudioSpecificConfig(std::vector<uint8_t>* buffer) const override;
  uint8_t GetObjectType() const override;
  uint32_t GetSamplingFrequency() const override;
  uint8_t GetNumChannels() const override;
  /// @}

 private:
  Ac3Header(const Ac3Header&) = delete;
  Ac3Header& operator=(const Ac3Header&) = delete;

  uint8_t fscod_ = 0;       // Sample rate code
  uint8_t frmsizecod_ = 0;  // Frame size code
  uint8_t bsid_ = 0;        // Bit stream identification
  uint8_t bsmod_ = 0;       // Bit stream mode
  uint8_t acmod_ = 0;       // Audio coding mode
  uint8_t lfeon_ = 0;       // Low frequency effects channel on
};

}  // namespace mp2t
}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_FORMATS_MP2T_AC3_HEADER_H_
