// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_MP4_AAC_AUDIO_SPECIFIC_CONFIG_H_
#define MEDIA_FORMATS_MP4_AAC_AUDIO_SPECIFIC_CONFIG_H_

#include <stddef.h>
#include <stdint.h>

#include <vector>

namespace shaka {
namespace media {

class BitReader;

namespace mp4 {

// Methods are virtual for mocking.
/// This class parses the AAC information from decoder specific information
/// embedded in the @b esds box in an ISO BMFF file.
/// Please refer to ISO 14496 Part 3 Table 1.13 - Syntax of AudioSpecificConfig
/// for more details.
class AACAudioSpecificConfig {
 public:
  AACAudioSpecificConfig();
  virtual ~AACAudioSpecificConfig();

  /// Parse the AAC config from decoder specific information embedded in an @b
  /// esds box. The function will parse the data and get the
  /// ElementaryStreamDescriptor, then it will parse the
  /// ElementaryStreamDescriptor to get audio stream configurations.
  /// @param data contains decoder specific information from an @b esds box.
  /// @return true if successful, false otherwise.
  virtual bool Parse(const std::vector<uint8_t>& data);

  /// Convert a raw AAC frame into an AAC frame with an ADTS header.
  /// @param[in,out] buffer contains the raw AAC frame on input, and the
  ///                converted frame on output if successful; it is untouched
  ///                on failure.
  /// @return true on success, false otherwise.
  virtual bool ConvertToADTS(std::vector<uint8_t>* buffer) const;

  /// @param sbr_in_mimetype indicates whether SBR mode is specified in the
  ///        mimetype, i.e. codecs parameter contains mp4a.40.5.
  /// @return Output sample rate for the AAC stream.
  uint32_t GetOutputSamplesPerSecond(bool sbr_in_mimetype) const;

  /// @param sbr_in_mimetype indicates whether SBR mode is specified in the
  ///        mimetype, i.e. codecs parameter contains mp4a.40.5.
  /// @return Number of channels for the AAC stream.
  uint8_t GetNumChannels(bool sbr_in_mimetype) const;

  /// @return The audio object type for this AAC config.
  uint8_t audio_object_type() const { return audio_object_type_; }

  /// @return The sampling frequency for this AAC config.
  uint32_t frequency() const { return frequency_; }

  /// @return Number of channels for this AAC config.
  uint8_t num_channels() const { return num_channels_; }

  /// Size in bytes of the ADTS header added by ConvertEsdsToADTS().
  static const size_t kADTSHeaderSize = 7;

 private:
  bool SkipDecoderGASpecificConfig(BitReader* bit_reader) const;
  bool SkipErrorSpecificConfig() const;
  bool SkipGASpecificConfig(BitReader* bit_reader) const;

  // The following variables store the AAC specific configuration information
  // that are used to generate the ADTS header.
  uint8_t audio_object_type_;
  uint8_t frequency_index_;
  uint8_t channel_config_;
  // Is Parametric Stereo on?
  bool ps_present_;

  // The following variables store audio configuration information.
  // They are based on the AAC specific configuration but can be overridden
  // by extensions in elementary stream descriptor.
  uint32_t frequency_;
  uint32_t extension_frequency_;
  uint8_t num_channels_;
};

}  // namespace mp4
}  // namespace media
}  // namespace shaka

#endif  // MEDIA_FORMATS_MP4_AAC_AUDIO_SPECIFIC_CONFIG_H_
