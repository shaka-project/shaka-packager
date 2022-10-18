// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PACKAGER_MEDIA_CODECS_AAC_AUDIO_SPECIFIC_CONFIG_H_
#define PACKAGER_MEDIA_CODECS_AAC_AUDIO_SPECIFIC_CONFIG_H_

#include <stddef.h>
#include <stdint.h>

#include <vector>

namespace shaka {
namespace media {

class BitReader;

// Methods are virtual for mocking.
/// This class parses the AAC information from decoder specific information
/// embedded in the @b esds box in an ISO BMFF file.
/// Please refer to ISO 14496 Part 3 Table 1.13 - Syntax of AudioSpecificConfig
/// for more details.
class AACAudioSpecificConfig {
 public:
  // Audio Object Types specified in ISO 14496-3 (2005), Table 1.3
  enum AudioObjectType {
    AOT_NULL             = 0,
    AOT_AAC_MAIN         = 1,   // Main
    AOT_AAC_LC           = 2,   // Low Complexity
    AOT_AAC_SSR          = 3,   // Scalable Sample Rate
    AOT_AAC_LTP          = 4,   // Long Term Prediction
    AOT_SBR              = 5,   // Spectral Band Replication
    AOT_AAC_SCALABLE     = 6,   // Scalable
    AOT_TWINVQ           = 7,   // Twin Vector Quantizer
    AOT_CELP             = 8,   // Code Excited Linear Prediction
    AOT_HVXC             = 9,   // Harmonic Vector eXcitation Coding
    AOT_TTSI             = 12,  // Text-To-Speech Interface
    AOT_MAINSYNTH        = 13,  // Main Synthesis
    AOT_WAVESYNTH        = 14,  // Wavetable Synthesis
    AOT_MIDI             = 15,  // General MIDI
    AOT_SAFX             = 16,  // Algorithmic Synthesis and Audio Effects
    AOT_ER_AAC_LC        = 17,  // Error Resilient Low Complexity
    AOT_ER_AAC_LTP       = 19,  // Error Resilient Long Term Prediction
    AOT_ER_AAC_SCALABLE  = 20,  // Error Resilient Scalable
    AOT_ER_TWINVQ        = 21,  // Error Resilient Twin Vector Quantizer
    AOT_ER_BSAC          = 22,  // Error Resilient Bit-Sliced Arithmetic Coding
    AOT_ER_AAC_LD        = 23,  // Error Resilient Low Delay
    AOT_ER_CELP          = 24,  // Error Resilient Code Excited Linear
                                // Prediction
    AOT_ER_HVXC          = 25,  // Error Resilient Harmonic Vector eXcitation
                                // Coding
    AOT_ER_HILN          = 26,  // Error Resilient Harmonic and Individual Lines
                                // plus Noise
    AOT_ER_PARAM         = 27,  // Error Resilient Parametric
    AOT_SSC              = 28,  // SinuSoidal Coding
    AOT_PS               = 29,  // Parametric Stereo
    AOT_SURROUND         = 30,  // MPEG Surround
    AOT_ESCAPE           = 31,  // Escape Value
    AOT_L1               = 32,  // Layer 1
    AOT_L2               = 33,  // Layer 2
    AOT_L3               = 34,  // Layer 3
    AOT_DST              = 35,  // Direct Stream Transfer
    AOT_ALS              = 36,  // Audio LosslesS
    AOT_SLS              = 37,  // Scalable LosslesS
    AOT_SLS_NON_CORE     = 38,  // Scalable LosslesS (non core)
    AOT_ER_AAC_ELD       = 39,  // Error Resilient Enhanced Low Delay
    AOT_SMR_SIMPLE       = 40,  // Symbolic Music Representation Simple
    AOT_SMR_MAIN         = 41,  // Symbolic Music Representation Main
    AOT_USAC             = 42,  // Unified Speech and Audio Coding
    AOT_SAOC             = 43,  // Spatial Audio Object Coding
    AOT_LD_SURROUND      = 44,  // Low Delay MPEG Surround
    SAOC_DE              = 45,  // Spatial Audio Object Coding Dialogue Enhancement
  };

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
  /// @param data points to the raw AAC frame to be converted.
  /// @param data_size the size of a raw AAC frame.
  /// @param[out] audio_frame contains the converted frame if successful; it is
  /// untouched on failure.
  /// @return true on success, false otherwise.
  virtual bool ConvertToADTS(const uint8_t* data,
                             size_t data_size,
                             std::vector<uint8_t>* audio_frame) const;

  /// @return The audio object type for this AAC config, with possible extension
  ///         considered.
  AudioObjectType GetAudioObjectType() const;

  /// @return Sample rate for the AAC stream, with possible extensions
  ///         considered.
  uint32_t GetSamplesPerSecond() const;

  /// @return Number of channels for the AAC stream, with possible extensions
  ///         considered.
  uint8_t GetNumChannels() const;

  /// Size in bytes of the ADTS header added by ConvertEsdsToADTS().
  static const size_t kADTSHeaderSize = 7;

  /// @return whether Spectral Band Repliation (SBR) is present in the stream.
  bool sbr_present() const { return sbr_present_; }
  /// Indicate whether SBR is present in the stream.
  void set_sbr_present(bool sbr_present) { sbr_present_ = sbr_present; }

 private:
  bool ParseAudioObjectType(BitReader* bit_reader);
  bool ParseDecoderGASpecificConfig(BitReader* bit_reader);
  bool SkipErrorSpecificConfig() const;
  // Parse GASpecificConfig. Calls |ParseProgramConfigElement| if
  // |channel_config_| == 0.
  bool ParseGASpecificConfig(BitReader* bit_reader);
  // Parse program_config_element(). |num_channels_| will be updated.
  bool ParseProgramConfigElement(BitReader* bit_reader);

  // The following variables store the AAC specific configuration information
  // that are used to generate the ADTS header.
  AudioObjectType audio_object_type_ = AOT_NULL;
  uint8_t frequency_index_ = 0;
  uint8_t channel_config_ = 0;
  // Is Spectral Band Replication (SBR) available?
  bool sbr_present_ = false;
  // Is Parametric Stereo available?
  bool ps_present_ = false;

  // The following variables store audio configuration information.
  // They are based on the AAC specific configuration but can be overridden
  // by extensions in elementary stream descriptor.
  uint32_t frequency_ = 0;
  uint32_t extension_frequency_ = 0;
  uint8_t num_channels_ = 0;
};

}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_CODECS_AAC_AUDIO_SPECIFIC_CONFIG_H_
