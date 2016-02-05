// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef MEDIA_FILTERS_H264_BYTE_TO_UNIT_STREAM_CONVERTER_H_
#define MEDIA_FILTERS_H264_BYTE_TO_UNIT_STREAM_CONVERTER_H_

#include <stddef.h>
#include <stdint.h>

#include <vector>

namespace edash_packager {
namespace media {

class BufferWriter;
class Nalu;

/// Class which converts H.264 byte streams (as specified in ISO/IEC 14496-10
/// Annex B) into H.264 NAL unit streams (as specified in ISO/IEC 14496-15).
class H264ByteToUnitStreamConverter {
 public:
  static const size_t kUnitStreamNaluLengthSize = 4;

  H264ByteToUnitStreamConverter();
  ~H264ByteToUnitStreamConverter();

  /// Converts a whole AVC byte stream encoded video frame to NAL unit stream
  /// format.
  /// @param input_frame is a buffer containing a whole H.264 frame in byte
  ///        stream format.
  /// @param input_frame_size is the size of the H.264 frame, in bytes.
  /// @param output_frame is a pointer to a vector which will receive the
  ///        converted frame.
  /// @return true if successful, false otherwise.
  bool ConvertByteStreamToNalUnitStream(const uint8_t* input_frame,
                                        size_t input_frame_size,
                                        std::vector<uint8_t>* output_frame);

  /// Synthesizes an AVCDecoderConfigurationRecord from the SPS and PPS NAL
  /// units extracted from the AVC byte stream.
  /// @param decoder_config is a pointer to a vector, which on successful
  ///        return will contain the computed AVCDecoderConfigurationRecord.
  /// @return true if successful, or false otherwise.
  bool GetAVCDecoderConfigurationRecord(std::vector<uint8_t>* decoder_config);

 private:
  void ProcessNalu(const Nalu& nalu,
                   BufferWriter* output_buffer);

  std::vector<uint8_t> last_sps_;
  std::vector<uint8_t> last_pps_;
};

}  // namespace media
}  // namespace edash_packager

#endif  // MEDIA_FILTERS_H264_BYTE_TO_UNIT_STREAM_CONVERTER_H_
