// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_FORMATS_MP2T_ADTS_HEADER_H_
#define PACKAGER_MEDIA_FORMATS_MP2T_ADTS_HEADER_H_

#include <stdint.h>

#include <vector>

#include "packager/media/formats/mp2t/audio_header.h"

namespace shaka {
namespace media {
namespace mp2t {

/// Class which parses ADTS frame (header / metadata) and synthesizes
/// AudioSpecificConfig from audio frame content.
class AdtsHeader : public AudioHeader {
 public:
  AdtsHeader() = default;
  ~AdtsHeader() override = default;

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
  AdtsHeader(const AdtsHeader&) = delete;
  AdtsHeader& operator=(const AdtsHeader&) = delete;

  uint8_t protection_absent_ = 0;
  uint16_t frame_size_ = 0;
  uint8_t profile_ = 0;
  uint8_t sampling_frequency_index_ = 0;
  uint8_t channel_configuration_ = 0;
};

}  // namespace mp2t
}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_FORMATS_MP2T_ADTS_HEADER_H_
