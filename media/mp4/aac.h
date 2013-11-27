// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MP4_AAC_H_
#define MEDIA_MP4_AAC_H_

#include <vector>

#include "base/basictypes.h"

namespace media {

class BitReader;

namespace mp4 {

// This class parses the AAC information from decoder specific information
// embedded in the esds box in an ISO BMFF file.
// Please refer to ISO 14496 Part 3 Table 1.13 - Syntax of AudioSpecificConfig
// for more details.
// TODO(kqyang): the class name is not appropriate, it should be
// AACAudioSpecificConfig instead.
class AAC {
 public:
  AAC();
  ~AAC();

  // Parse the AAC config from the raw binary data embedded in esds box.
  // The function will parse the data and get the ElementaryStreamDescriptor,
  // then it will parse the ElementaryStreamDescriptor to get audio stream
  // configurations.
  bool Parse(const std::vector<uint8>& data);

  // Gets the output sample rate for the AAC stream.
  // |sbr_in_mimetype| should be set to true if the SBR mode is
  // signalled in the mimetype. (ie mp4a.40.5 in the codecs parameter).
  uint32 GetOutputSamplesPerSecond(bool sbr_in_mimetype) const;

  // Gets number of channels for the AAC stream.
  // |sbr_in_mimetype| should be set to true if the SBR mode is
  // signalled in the mimetype. (ie mp4a.40.5 in the codecs parameter).
  uint8 GetNumChannels(bool sbr_in_mimetype) const;

  // This function converts a raw AAC frame into an AAC frame with an ADTS
  // header. On success, the function returns true and stores the converted data
  // in the buffer. The function returns false on failure and leaves the buffer
  // unchanged.
  bool ConvertToADTS(std::vector<uint8>* buffer) const;

  uint8 audio_object_type() const {
    return audio_object_type_;
  }

  uint32 frequency() const {
    return frequency_;
  }

  uint8 num_channels() const {
    return num_channels_;
  }

  // Size in bytes of the ADTS header added by ConvertEsdsToADTS().
  static const size_t kADTSHeaderSize = 7;

 private:
  bool SkipDecoderGASpecificConfig(BitReader* bit_reader) const;
  bool SkipErrorSpecificConfig() const;
  bool SkipGASpecificConfig(BitReader* bit_reader) const;

  // The following variables store the AAC specific configuration information
  // that are used to generate the ADTS header.
  uint8 audio_object_type_;
  uint8 frequency_index_;
  uint8 channel_config_;
  // Is Parametric Stereo on?
  bool ps_present_;

  // The following variables store audio configuration information.
  // They are based on the AAC specific configuration but can be overridden
  // by extensions in elementary stream descriptor.
  uint32 frequency_;
  uint32 extension_frequency_;
  uint8 num_channels_;
};

}  // namespace mp4

}  // namespace media

#endif  // MEDIA_MP4_AAC_H_
