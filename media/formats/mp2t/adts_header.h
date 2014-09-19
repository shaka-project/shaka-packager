// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef MEDIA_FORMATS_MP2T_ADTS_HEADER_H_
#define MEDIA_FORMATS_MP2T_ADTS_HEADER_H_

#include "base/basictypes.h"

#include <vector>

namespace edash_packager {
namespace media {
namespace mp2t {

/// Class which parses ADTS headers and synthesizes AudioSpecificConfig
/// and audio mime type from ADTS header contents.
class AdtsHeader {
 public:
  AdtsHeader();
  ~AdtsHeader() {}

  /// Get the size of the ADTS frame from a partial or complete frame.
  /// @param data is a pointer to the beginning of the ADTS frame.
  /// @param num_bytes is the number of data bytes at @a data.
  /// @return Size of the ADTS frame (header + payload) if successful, or
  ///         zero otherwise.
  static size_t GetAdtsFrameSize(const uint8* data, size_t num_bytes);

  /// Get the size of the ADTS header from a partial or complete frame.
  /// @param data is a pointer to the beginning of the ADTS frame.
  /// @param num_bytes is the number of data bytes at @a data.
  /// @return Size of the ADTS header if successful, or zero otherwise.
  static size_t GetAdtsHeaderSize(const uint8* data, size_t num_bytes);

  /// Parse an ADTS header, extracting the fields within.
  /// @param adts_frame is an input parameter pointing to the ADTS header
  ///        of an ADTS-framed audio sample.
  /// @param adts_frame_size is the size, in bytes of the input ADTS frame.
  /// @return true if successful, false otherwise.
  bool Parse(const uint8* adts_frame, size_t adts_frame_size);

  /// Synthesize an AudioSpecificConfig record from the fields within the ADTS
  /// header.
  /// @param [out] buffer is a pointer to a vector to contain the
  ///        AudioSpecificConfig.
  /// @return true if successful, false otherwise.
  bool GetAudioSpecificConfig(std::vector<uint8>* buffer) const;

  /// @return The audio profile for this ADTS frame.
  uint8 GetObjectType() const;

  /// @return The sampling frequency for this ADTS frame.
  uint32 GetSamplingFrequency() const;

  /// @return Number of channels for this AAC config.
  uint8 GetNumChannels() const;

 private:
  bool valid_config_;
  uint8 profile_;
  uint8 sampling_frequency_index_;
  uint8 channel_configuration_;

  DISALLOW_COPY_AND_ASSIGN(AdtsHeader);
};

}  // namespace mp2t
}  // namespace media
}  // namespace edash_packager

#endif  // MEDIA_FORMATS_MP2T_ADTS_HEADER_H_
