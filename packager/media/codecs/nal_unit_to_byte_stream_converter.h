// Copyright 2016 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_CODECS_NAL_UNIT_TO_BYTE_STREAM_CONVERTER_H_
#define PACKAGER_MEDIA_CODECS_NAL_UNIT_TO_BYTE_STREAM_CONVERTER_H_

#include <cstdint>
#include <vector>

#include <packager/macros/classes.h>
#include <packager/media/base/decrypt_config.h>
#include <packager/media/codecs/avc_decoder_configuration_record.h>

namespace shaka {
namespace media {

class BufferWriter;
class VideoStreamInfo;

/// Inserts emulation prevention byte (0x03) where necessary.
/// It is safe to call this again on the output, i.e. it is OK to "re-escape".
/// This cannot do in-place escaping.
/// @param input is the data to be escaped. This may not be the same (internal)
///        buffer as @a output.
/// @param input_size is the size of the input.
/// @param output is the escaped data.
void EscapeNalByteSequence(const uint8_t* input,
                           size_t input_size,
                           BufferWriter* output);

// Methods are virtual for mocking.
class NalUnitToByteStreamConverter {
 public:
  NalUnitToByteStreamConverter();
  virtual ~NalUnitToByteStreamConverter();

  /// This must be called before calling other methods.
  /// @param decoder_configuration_data is the pointer to a decoder config data.
  /// @param decoder_configuration_data_size is the size of @a
  ///        decoder_configuration_data.
  /// @return true on success, false otherwise.
  virtual bool Initialize(const uint8_t* decoder_configuration_data,
                          size_t decoder_configuration_data_size);

  /// Converts unit stream to byte stream using the data passed to Initialize().
  /// The method will function correctly even if @a sample is encrypted using
  /// SAMPLE-AES encryption.
  /// @param sample is the sample to be converted.
  /// @param sample_size is the size of @a sample.
  /// @param is_key_frame indicates if the sample is a key frame.
  /// @param[out] output is set to the the converted sample, on success.
  /// @return true on success, false otherwise.
  virtual bool ConvertUnitToByteStream(const uint8_t* sample,
                                       size_t sample_size,
                                       bool is_key_frame,
                                       std::vector<uint8_t>* output);

  /// Converts unit stream to byte stream using the data passed to Initialize()
  /// and update the corresponding subsamples of the media sample.
  /// The method will function correctly even if @a sample is encrypted using
  /// SAMPLE-AES encryption.
  /// @param sample is the sample to be converted.
  /// @param sample_size is the size of @a sample.
  /// @param is_key_frame indicates if the sample is a key frame.
  /// @param escape_encrypted_nalu indicates whether an encrypted nalu should be
  ///        escaped. This is needed for Apple Sample AES. Note that
  ///        |subsamples| on return contains the sizes before escaping.
  /// @param[out] output is set to the the converted sample, on success.
  /// @param[in,out] subsamples has the input subsamples and output updated
  ///                subsamples, on success.
  /// @return true on success, false otherwise.
  virtual bool ConvertUnitToByteStreamWithSubsamples(
      const uint8_t* sample,
      size_t sample_size,
      bool is_key_frame,
      bool escape_encrypted_nalu,
      std::vector<uint8_t>* output,
      std::vector<SubsampleEntry>* subsamples);

 private:
  friend class NalUnitToByteStreamConverterTest;

  int nalu_length_size_;
  AVCDecoderConfigurationRecord decoder_config_;
  std::vector<uint8_t> decoder_configuration_in_byte_stream_;

  DISALLOW_COPY_AND_ASSIGN(NalUnitToByteStreamConverter);
};

}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_CODECS_NAL_UNIT_TO_BYTE_STREAM_CONVERTER_H_
