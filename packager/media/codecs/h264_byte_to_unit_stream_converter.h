// Copyright 2014 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_CODECS_H264_BYTE_TO_UNIT_STREAM_CONVERTER_H_
#define PACKAGER_MEDIA_CODECS_H264_BYTE_TO_UNIT_STREAM_CONVERTER_H_

#include <cstddef>
#include <cstdint>
#include <vector>

#include <packager/macros/classes.h>
#include <packager/media/codecs/h26x_byte_to_unit_stream_converter.h>

namespace shaka {
namespace media {

/// Class which converts H.264 byte streams (as specified in ISO/IEC 14496-10
/// Annex B) into H.264 NAL unit streams (as specified in ISO/IEC 14496-15).
class H264ByteToUnitStreamConverter : public H26xByteToUnitStreamConverter {
 public:
  /// Create a H264 byte to unit stream converter.
  /// The setting of @a KeepParameterSetNalus is defined by a gflag.
  H264ByteToUnitStreamConverter();

  /// Create a H264 byte to unit stream converter with desired output stream
  /// format (whether to include parameter set nal units).
  explicit H264ByteToUnitStreamConverter(H26xStreamFormat stream_format);

  ~H264ByteToUnitStreamConverter() override;

  /// @name H26xByteToUnitStreamConverter implementation override.
  /// @{
  bool GetDecoderConfigurationRecord(
      std::vector<uint8_t>* decoder_config) const override;
  /// @}

 private:
  bool ProcessNalu(const Nalu& nalu) override;

  std::vector<uint8_t> last_sps_;
  std::vector<uint8_t> last_pps_;
  std::vector<uint8_t> last_sps_ext_;

  DISALLOW_COPY_AND_ASSIGN(H264ByteToUnitStreamConverter);
};

}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_CODECS_H264_BYTE_TO_UNIT_STREAM_CONVERTER_H_
