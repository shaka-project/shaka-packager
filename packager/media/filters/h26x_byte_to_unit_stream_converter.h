// Copyright 2016 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef MEDIA_FILTERS_H26X_BYTE_TO_UNIT_STREAM_CONVERTER_H_
#define MEDIA_FILTERS_H26X_BYTE_TO_UNIT_STREAM_CONVERTER_H_

#include <stdint.h>

#include <vector>

#include "packager/media/filters/nalu_reader.h"

namespace edash_packager {
namespace media {

class BufferWriter;

/// A base class that is used to convert H.26x byte streams to NAL unit streams.
class H26xByteToUnitStreamConverter {
 public:
  static const size_t kUnitStreamNaluLengthSize = 4;

  H26xByteToUnitStreamConverter(Nalu::CodecType type);
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

 private:
  // Process the given Nalu.  If this returns true, it was handled and should
  // not be copied to the buffer.
  virtual bool ProcessNalu(const Nalu& nalu) = 0;

  Nalu::CodecType type_;

  DISALLOW_COPY_AND_ASSIGN(H26xByteToUnitStreamConverter);
};

}  // namespace media
}  // namespace edash_packager

#endif  // MEDIA_FILTERS_H26x_BYTE_TO_UNIT_STREAM_CONVERTER_H_

