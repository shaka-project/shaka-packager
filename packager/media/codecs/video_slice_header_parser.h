// Copyright 2016 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_CODECS_VIDEO_SLICE_HEADER_PARSER_H_
#define PACKAGER_MEDIA_CODECS_VIDEO_SLICE_HEADER_PARSER_H_

#include <cstdint>
#include <vector>

#include <packager/macros/classes.h>
#include <packager/media/codecs/h264_parser.h>
#include <packager/media/codecs/h265_parser.h>
#include <packager/media/codecs/hevc_decoder_configuration_record.h>

namespace shaka {
namespace media {

class VideoSliceHeaderParser {
 public:
  VideoSliceHeaderParser() {}
  virtual ~VideoSliceHeaderParser() {}

  /// Adds decoder configuration from the given data.  This must be called
  /// once before any calls to GetHeaderSize.
  virtual bool Initialize(
      const std::vector<uint8_t>& decoder_configuration) = 0;

  /// Adds decoder configuration from the given data for the layered case;
  /// e.g: MV-HEVC.  This must be also called once before any calls to
  // GetHeaderSize.
  virtual bool InitializeLayered(
      const std::vector<uint8_t>& layered_decoder_configuration) = 0;

  /// Process NAL unit, in particular parameter set NAL units.  Non parameter
  /// set NAL unit is allowed but the function always returns true.
  /// Returns false if there is any problem processing the parameter set NAL
  /// unit.
  /// This function is needed to handle parameter set NAL units not in decoder
  /// configuration record, i.e. in the samples.
  virtual bool ProcessNalu(const Nalu& nalu) = 0;

  /// Gets the header size of the given NALU.  Returns < 0 on error.
  virtual int64_t GetHeaderSize(const Nalu& nalu) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(VideoSliceHeaderParser);
};

class H264VideoSliceHeaderParser : public VideoSliceHeaderParser {
 public:
  H264VideoSliceHeaderParser();
  ~H264VideoSliceHeaderParser() override;

  /// @name VideoSliceHeaderParser implementation overrides.
  /// @{
  bool Initialize(const std::vector<uint8_t>& decoder_configuration) override;
  bool InitializeLayered(
      const std::vector<uint8_t>& decoder_configuration) override;
  bool ProcessNalu(const Nalu& nalu) override;
  int64_t GetHeaderSize(const Nalu& nalu) override;
  /// @}

 private:
  H264Parser parser_;

  DISALLOW_COPY_AND_ASSIGN(H264VideoSliceHeaderParser);
};

class H265VideoSliceHeaderParser : public VideoSliceHeaderParser {
 public:
  H265VideoSliceHeaderParser();
  ~H265VideoSliceHeaderParser() override;

  /// @name VideoSliceHeaderParser implementation overrides.
  /// @{
  bool Initialize(const std::vector<uint8_t>& decoder_configuration) override;
  bool InitializeLayered(
      const std::vector<uint8_t>& decoder_configuration) override;
  bool ProcessNalu(const Nalu& nalu) override;
  int64_t GetHeaderSize(const Nalu& nalu) override;
  /// @}

 private:
  bool ParseParameterSets(const HEVCDecoderConfigurationRecord& config);

  H265Parser parser_;

  DISALLOW_COPY_AND_ASSIGN(H265VideoSliceHeaderParser);
};

}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_CODECS_VIDEO_SLICE_HEADER_PARSER_H_
