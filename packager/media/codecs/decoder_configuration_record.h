// Copyright 2016 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_CODECS_DECODER_CONFIGURATION_RECORD_H_
#define PACKAGER_MEDIA_CODECS_DECODER_CONFIGURATION_RECORD_H_

#include <cstdint>
#include <vector>

#include <absl/log/check.h>
#include <absl/log/log.h>

#include <packager/macros/classes.h>
#include <packager/media/codecs/nalu_reader.h>

namespace shaka {
namespace media {

// Defines a base class for decoder configuration record.
class DecoderConfigurationRecord {
 public:
  virtual ~DecoderConfigurationRecord();

  /// Parses input to extract decoder configuration record.  This will make and
  /// store a copy of the data for Nalu access.
  /// @return false if there are parsing errors.
  bool Parse(const std::vector<uint8_t>& data) {
    return Parse(data.data(), data.size());
  }

  /// Parses input to extract decoder configuration record.  This will make and
  /// store a copy of the data for Nalu access.
  /// @return false if there are parsing errors.
  bool Parse(const uint8_t* data, size_t data_size);

  /// @return The size of the NAL unit length field.
  uint8_t nalu_length_size() const { return nalu_length_size_; }

  /// @return The number of Nalu in the configuration.
  size_t nalu_count() const { return nalu_.size(); }

  /// @return The nalu at the given index.  The Nalu is only valid for the
  ///         lifetime of this object, even if copied.
  const Nalu& nalu(size_t i) const { return nalu_[i]; }

  /// @return Transfer characteristics of the config.
  uint8_t transfer_characteristics() const { return transfer_characteristics_; }

  /// @return Colour Primaries of the config.
  uint8_t color_primaries() const { return color_primaries_; }

  /// @return Matrix Coeffs of the config.
  uint8_t matrix_coefficients() const { return matrix_coefficients_; }

 protected:
  DecoderConfigurationRecord();

  /// Adds the given Nalu to the configuration.
  void AddNalu(const Nalu& nalu);

  /// @return a pointer to the copy of the data.
  const uint8_t* data() const { return data_.data(); }

  /// @return the size of the copy of the data.
  size_t data_size() const { return data_.size(); }

  /// Sets the size of the NAL unit length field.
  void set_nalu_length_size(uint8_t nalu_length_size) {
    DCHECK(nalu_length_size <= 2 || nalu_length_size == 4);
    nalu_length_size_ = nalu_length_size;
  }

  /// Sets the transfer characteristics.
  void set_transfer_characteristics(uint8_t transfer_characteristics) {
    transfer_characteristics_ = transfer_characteristics;
  }

  /// Sets the colour primaries.
  void set_color_primaries(uint8_t color_primaries) {
    color_primaries_ = color_primaries;
  }
  /// Sets the matrix coeffs.
  void set_matrix_coefficients(uint8_t matrix_coefficients) {
    matrix_coefficients_ = matrix_coefficients;
  }

 private:
  // Performs the actual parsing of the data.
  virtual bool ParseInternal() = 0;

  // Contains a copy of the data.  This manages the pointer lifetime so the
  // extracted Nalu can accessed.
  std::vector<uint8_t> data_;
  std::vector<Nalu> nalu_;
  uint8_t nalu_length_size_ = 0;

  // Indicates the opto-electronic transfer characteristics of the source
  // picture, which can be used to determine whether the video is HDR or SDR.
  // The parameter is extracted from SPS.
  uint8_t transfer_characteristics_ = 0;

  uint8_t color_primaries_ = 0;
  uint8_t matrix_coefficients_ = 0;

  DISALLOW_COPY_AND_ASSIGN(DecoderConfigurationRecord);
};

}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_CODECS_DECODER_CONFIGURATION_RECORD_H_
