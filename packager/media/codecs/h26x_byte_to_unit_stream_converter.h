// Copyright 2016 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_CODECS_H26X_BYTE_TO_UNIT_STREAM_CONVERTER_H_
#define PACKAGER_MEDIA_CODECS_H26X_BYTE_TO_UNIT_STREAM_CONVERTER_H_

#include <cstdint>
#include <vector>

#include <packager/macros/classes.h>
#include <packager/media/base/video_stream_info.h>
#include <packager/media/codecs/nalu_reader.h>

namespace shaka {
namespace media {

class BufferWriter;

/// A base class that is used to convert H.26x byte streams to NAL unit streams.
class H26xByteToUnitStreamConverter {
 public:
  static constexpr size_t kUnitStreamNaluLengthSize = 4;

  /// Create a byte to unit stream converter with specified codec type.
  /// The setting of @a KeepParameterSetNalus is defined by a gflag.
  explicit H26xByteToUnitStreamConverter(Nalu::CodecType type);

  /// Create a byte to unit stream converter with specified codec type and
  /// desired output stream format (whether to include parameter set nal units).
  H26xByteToUnitStreamConverter(Nalu::CodecType type,
                                H26xStreamFormat stream_format);

  virtual ~H26xByteToUnitStreamConverter();

  /// Converts a whole byte stream encoded video frame to NAL unit stream
  /// format.
  /// @param input_frame is a buffer containing a whole H.26x frame in byte
  ///        stream format.
  /// @param input_frame_size is the size of the H.26x frame, in bytes.
  /// @param output_frame is a pointer to a vector which will receive the
  ///        converted frame.
  /// @return true if successful, false otherwise.
  bool ConvertByteStreamToNalUnitStream(const uint8_t* input_frame,
                                        size_t input_frame_size,
                                        std::vector<uint8_t>* output_frame);

  /// Creates either an AVCDecoderConfigurationRecord or a
  /// HEVCDecoderConfigurationRecord from the units extracted from the byte
  /// stream.
  /// @param decoder_config is a pointer to a vector, which on successful
  ///        return will contain the computed record.
  /// @return true if successful, or false otherwise.
  virtual bool GetDecoderConfigurationRecord(
      std::vector<uint8_t>* decoder_config) const = 0;

  H26xStreamFormat stream_format() const { return stream_format_; }

 protected:
  bool strip_parameter_set_nalus() const {
    return stream_format_ ==
           H26xStreamFormat::kNalUnitStreamWithoutParameterSetNalus;
  }

  // Warn if (nalu_ptr, nalu_size) does not match with |vector|.
  void WarnIfNotMatch(int nalu_type,
                      const uint8_t* nalu_ptr,
                      size_t nalu_size,
                      const std::vector<uint8_t>& vector);

 private:
  // Process the given Nalu.  If this returns true, it was handled and should
  // not be copied to the buffer.
  virtual bool ProcessNalu(const Nalu& nalu) = 0;

  Nalu::CodecType type_;
  H26xStreamFormat stream_format_;

  DISALLOW_COPY_AND_ASSIGN(H26xByteToUnitStreamConverter);
};

}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_CODECS_H26x_BYTE_TO_UNIT_STREAM_CONVERTER_H_
