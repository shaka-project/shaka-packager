// Copyright 2015 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_CODECS_VP_CODEC_CONFIGURATION_RECORD_H_
#define PACKAGER_MEDIA_CODECS_VP_CODEC_CONFIGURATION_RECORD_H_

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <packager/macros/classes.h>
#include <packager/media/base/video_stream_info.h>

namespace shaka {
namespace media {

/// The below enums are from ffmpeg/libavutil/pixfmt.h.
/// Chromaticity coordinates of the source primaries.
enum AVColorPrimaries {
  AVCOL_PRI_RESERVED0 = 0,
  /// Also ITU-R BT1361 / IEC 61966-2-4 / SMPTE RP177 Annex B
  AVCOL_PRI_BT709 = 1,
  AVCOL_PRI_UNSPECIFIED = 2,
  AVCOL_PRI_RESERVED = 3,
  /// Also FCC Title 47 Code of Federal Regulations 73.682 (a)(20)
  AVCOL_PRI_BT470M = 4,
  /// Also ITU-R BT601-6 625 / ITU-R BT1358 625 / ITU-R BT1700 625 PAL & SECAM
  AVCOL_PRI_BT470BG = 5,
  /// Also ITU-R BT601-6 525 / ITU-R BT1358 525 / ITU-R BT1700 NTSC
  AVCOL_PRI_SMPTE170M = 6,
  /// Functionally identical to above
  AVCOL_PRI_SMPTE240M = 7,
  /// Colour filters using Illuminant C
  AVCOL_PRI_FILM = 8,
  /// ITU-R BT2020
  AVCOL_PRI_BT2020 = 9,
  /// SMPTE ST 428-1 (CIE 1931 XYZ)
  AVCOL_PRI_SMPTE428 = 10,
  AVCOL_PRI_SMPTEST428_1 = AVCOL_PRI_SMPTE428,
  /// SMPTE ST 431-2 (2011)
  AVCOL_PRI_SMPTE431 = 11,
  /// SMPTE ST 432-1 D65 (2010)
  AVCOL_PRI_SMPTE432 = 12,
  ///< Not part of ABI
  AVCOL_PRI_NB
};

/// Color Transfer Characteristic.
enum AVColorTransferCharacteristic {
  AVCOL_TRC_RESERVED0 = 0,
  /// Also ITU-R BT1361
  AVCOL_TRC_BT709 = 1,
  AVCOL_TRC_UNSPECIFIED = 2,
  AVCOL_TRC_RESERVED = 3,
  /// Also ITU-R BT470M / ITU-R BT1700 625 PAL & SECAM
  AVCOL_TRC_GAMMA22 = 4,
  /// Also ITU-R BT470BG
  AVCOL_TRC_GAMMA28 = 5,
  /// Also ITU-R BT601-6 525 or 625 / ITU-R BT1358 525 or 625 / ITU-R BT1700
  /// NTSC
  AVCOL_TRC_SMPTE170M = 6,
  AVCOL_TRC_SMPTE240M = 7,
  /// "Linear transfer characteristics"
  AVCOL_TRC_LINEAR = 8,
  /// "Logarithmic transfer characteristic (100:1 range)"
  AVCOL_TRC_LOG = 9,
  /// "Logarithmic transfer characteristic (100 * Sqrt(10) : 1 range)"
  AVCOL_TRC_LOG_SQRT = 10,
  /// IEC 61966-2-4
  AVCOL_TRC_IEC61966_2_4 = 11,
  /// ITU-R BT1361 Extended Colour Gamut
  AVCOL_TRC_BT1361_ECG = 12,
  /// IEC 61966-2-1 (sRGB or sYCC)
  AVCOL_TRC_IEC61966_2_1 = 13,
  /// ITU-R BT2020 for 10-bit system
  AVCOL_TRC_BT2020_10 = 14,
  /// ITU-R BT2020 for 12-bit system
  AVCOL_TRC_BT2020_12 = 15,
  /// SMPTE ST 2084 for 10-, 12-, 14- and 16-bit systems
  AVCOL_TRC_SMPTE2084 = 16,
  AVCOL_TRC_SMPTEST2084 = AVCOL_TRC_SMPTE2084,
  /// SMPTE ST 428-1
  AVCOL_TRC_SMPTE428 = 17,
  AVCOL_TRC_SMPTEST428_1 = AVCOL_TRC_SMPTE428,
  /// ARIB STD-B67, known as "Hybrid log-gamma"
  AVCOL_TRC_ARIB_STD_B67 = 18,
  /// Not part of ABI
  AVCOL_TRC_NB
};

/// YUV colorspace type (a.c.a matrix coefficients in 23001-8:2016).
enum AVColorSpace {
  /// Order of coefficients is actually GBR, also IEC 61966-2-1 (sRGB)
  AVCOL_SPC_RGB = 0,
  /// Also ITU-R BT1361 / IEC 61966-2-4 xvYCC709 / SMPTE RP177 Annex B
  AVCOL_SPC_BT709 = 1,
  AVCOL_SPC_UNSPECIFIED = 2,
  AVCOL_SPC_RESERVED = 3,
  /// FCC Title 47 Code of Federal Regulations 73.682 (a)(20)
  AVCOL_SPC_FCC = 4,
  /// Also ITU-R BT601-6 625 / ITU-R BT1358 625 / ITU-R BT1700 625 PAL & SECAM /
  /// IEC 61966-2-4 xvYCC601
  AVCOL_SPC_BT470BG = 5,
  /// Also ITU-R BT601-6 525 / ITU-R BT1358 525 / ITU-R BT1700 NTSC
  AVCOL_SPC_SMPTE170M = 6,
  /// Functionally identical to above
  AVCOL_SPC_SMPTE240M = 7,
  /// Used by Dirac / VC-2 and H.264 FRext, see ITU-T SG16
  AVCOL_SPC_YCOCG = 8,
  /// ITU-R BT2020 non-constant luminance system
  AVCOL_SPC_BT2020_NCL = 9,
  /// ITU-R BT2020 constant luminance system
  AVCOL_SPC_BT2020_CL = 10,
  /// SMPTE 2085, Y'D'zD'x
  AVCOL_SPC_SMPTE2085 = 11,
  /// Not part of ABI
  AVCOL_SPC_NB
};

///  Location of chroma samples.
///
///  Illustration showing the location of the first (top left) chroma sample of
///  the image, the left shows only luma, the right shows the location of the
///  chroma sample, the 2 could be imagined to overlay each other but are drawn
///  separately due to limitations of ASCII
///
///                 1st 2nd      1st 2nd horizontal luma sample positions
///                  v   v        v   v
///                  ______        ______
/// 1st luma line > |X   X ...   |3 4 X ...   X are luma samples,
///                 |            |1 2         1-6 are possible chroma positions
/// 2nd luma line > |X   X ...   |5 6 X ...   0 is undefined/unknown position
enum AVChromaLocation {
  AVCHROMA_LOC_UNSPECIFIED = 0,
  /// MPEG-2/4 4:2:0, H.264 default for 4:2:0
  AVCHROMA_LOC_LEFT = 1,
  /// MPEG-1 4:2:0, JPEG 4:2:0, H.263 4:2:0
  AVCHROMA_LOC_CENTER = 2,
  /// ITU-R 601, SMPTE 274M 296M S314M(DV 4:1:1), mpeg2 4:2:2
  AVCHROMA_LOC_TOPLEFT = 3,
  AVCHROMA_LOC_TOP = 4,
  AVCHROMA_LOC_BOTTOMLEFT = 5,
  AVCHROMA_LOC_BOTTOM = 6,
  /// Not part of ABI
  AVCHROMA_LOC_NB
};

/// Class for parsing or writing VP codec configuration record.
class VPCodecConfigurationRecord {
 public:
  enum ChromaSubsampling {
    CHROMA_420_VERTICAL = 0,
    CHROMA_420_COLLOCATED_WITH_LUMA = 1,
    CHROMA_422 = 2,
    CHROMA_444 = 3,
    CHROMA_440 = 4,
  };
  enum ChromaSitingValues {
    kUnspecified = 0,
    kLeftCollocated = 1,
    kTopCollocated = kLeftCollocated,
    kHalf = 2,
  };

  VPCodecConfigurationRecord();
  VPCodecConfigurationRecord(
      uint8_t profile,
      uint8_t level,
      uint8_t bit_depth,
      uint8_t chroma_subsampling,
      bool video_full_range_flag,
      uint8_t color_primaries,
      uint8_t transfer_characteristics,
      uint8_t matrix_coefficients,
      const std::vector<uint8_t>& codec_initialization_data);
  ~VPCodecConfigurationRecord();

  /// Parses input (in MP4 format) to extract VP codec configuration record.
  /// @return false if there is parsing errors.
  bool ParseMP4(const std::vector<uint8_t>& data);

  /// Parses input (in WebM format) to extract VP codec configuration record.
  /// @return false if there is parsing errors.
  bool ParseWebM(const std::vector<uint8_t>& data);

  /// Compute and set VP9 Level based on the input attributes.
  void SetVP9Level(uint16_t width,
                   uint16_t height,
                   double sample_duration_seconds);

  /// @param data should not be null.
  /// Writes VP codec configuration record to buffer using MP4 format.
  void WriteMP4(std::vector<uint8_t>* data) const;

  /// @param data should not be null.
  /// Writes VP codec configuration record to buffer using WebM format.
  void WriteWebM(std::vector<uint8_t>* data) const;

  /// @return The codec string.
  std::string GetCodecString(Codec codec) const;

  /// Merges the values from the given configuration.  If there are values in
  /// both |*this| and |other|, |*this| is not updated.
  void MergeFrom(const VPCodecConfigurationRecord& other);

  void SetChromaSubsampling(uint8_t subsampling_x, uint8_t subsampling_y);
  void SetChromaSubsampling(ChromaSubsampling chroma_subsampling);
  void SetChromaLocation(uint8_t chroma_siting_x, uint8_t chroma_siting_y);

  void set_profile(uint8_t profile) { profile_ = profile; }
  void set_level(uint8_t level) { level_ = level; }
  void set_bit_depth(uint8_t bit_depth) { bit_depth_ = bit_depth; }
  void set_video_full_range_flag(bool video_full_range_flag) {
    video_full_range_flag_ = video_full_range_flag;
  }
  void set_color_primaries(uint8_t color_primaries) {
    color_primaries_ = color_primaries;
  }
  void set_transfer_characteristics(uint8_t transfer_characteristics) {
    transfer_characteristics_ = transfer_characteristics;
  }
  void set_matrix_coefficients(uint8_t matrix_coefficients) {
    matrix_coefficients_ = matrix_coefficients;
  }

  bool is_profile_set() const { return static_cast<bool>(profile_); }
  bool is_level_set() const { return static_cast<bool>(level_); }
  bool is_bit_depth_set() const { return static_cast<bool>(bit_depth_); }
  bool is_chroma_subsampling_set() const {
    return static_cast<bool>(chroma_subsampling_);
  }
  bool is_video_full_range_flag_set() const {
    return static_cast<bool>(video_full_range_flag_);
  }
  bool is_color_primaries_set() const {
    return static_cast<bool>(color_primaries_);
  }
  bool is_transfer_characteristics_set() const {
    return static_cast<bool>(transfer_characteristics_);
  }
  bool is_matrix_coefficients_set() const {
    return static_cast<bool>(matrix_coefficients_);
  }
  bool is_chroma_location_set() const {
    return static_cast<bool>(chroma_location_);
  }

  uint8_t profile() const { return profile_.value_or(0); }
  uint8_t level() const { return level_.value_or(10); }
  uint8_t bit_depth() const { return bit_depth_.value_or(8); }
  uint8_t chroma_subsampling() const {
    return chroma_subsampling_.value_or(CHROMA_420_COLLOCATED_WITH_LUMA);
  }
  bool video_full_range_flag() const {
    return video_full_range_flag_.value_or(false);
  }
  uint8_t color_primaries() const {
    return color_primaries_.value_or(AVCOL_PRI_UNSPECIFIED);
  }
  uint8_t transfer_characteristics() const {
    return transfer_characteristics_.value_or(AVCOL_TRC_UNSPECIFIED);
  }
  uint8_t matrix_coefficients() const {
    return matrix_coefficients_.value_or(AVCOL_SPC_UNSPECIFIED);
  }
  uint8_t chroma_location() const {
    return chroma_location_.value_or(AVCHROMA_LOC_UNSPECIFIED);
  }

 private:
  void UpdateChromaSubsamplingIfNeeded();

  std::optional<uint8_t> profile_;
  std::optional<uint8_t> level_;
  std::optional<uint8_t> bit_depth_;
  std::optional<uint8_t> chroma_subsampling_;
  std::optional<bool> video_full_range_flag_;
  std::optional<uint8_t> color_primaries_;
  std::optional<uint8_t> transfer_characteristics_;
  std::optional<uint8_t> matrix_coefficients_;
  std::vector<uint8_t> codec_initialization_data_;

  // Not in the decoder config. It is there to help determine chroma subsampling
  // format.
  std::optional<uint8_t> chroma_location_;
  // Not using DISALLOW_COPY_AND_ASSIGN here intentionally to allow the compiler
  // generated copy constructor and assignment operator. Since the internal data
  // is small, the performance impact is minimal.
};

}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_CODECS_VP_CODEC_CONFIGURATION_RECORD_H_
