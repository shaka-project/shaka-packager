// Copyright 2020 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_DVB_DVB_IMAGE_H_
#define PACKAGER_MEDIA_DVB_DVB_IMAGE_H_

#include <cstdint>
#include <memory>
#include <type_traits>

namespace shaka {
namespace media {

struct RgbaColor {
  uint8_t r;
  uint8_t g;
  uint8_t b;
  uint8_t a;

  bool operator==(const RgbaColor& other) const {
    return r == other.r && g == other.g && b == other.b && a == other.a;
  }
  bool operator!=(const RgbaColor& other) const { return !(*this == other); }
};
// To avoid copying, we pass an RgbaColor array as a uint8_t* pointer to libpng
// for RGBA.
static_assert(std::is_pod<RgbaColor>::value, "RgbaColor must be POD");
static_assert(sizeof(RgbaColor) == 4, "RgbaColor not packed correctly");

enum class BitDepth : uint8_t {
  k2Bit,
  k4Bit,
  k8Bit,
};

/// Defines a color-space for DVB-sub images.  This maps to a single CLUT in the
/// spec.  This holds a map of the byte codes to the respective RGB colors.
/// This also handles getting the default colors when none are provided and
/// converting between bit-depths if applicable.
///
/// When handling bit-depths, this will attempt to use the bit-depth provided
/// before converting upward then downward.  Each color is only set for that
/// specific bit-depth; meaning different bit-depths can have different colors
/// mapped to the same byte-code.
class DvbImageColorSpace {
 public:
  DvbImageColorSpace();
  ~DvbImageColorSpace();

  DvbImageColorSpace(const DvbImageColorSpace&) = delete;
  DvbImageColorSpace& operator=(const DvbImageColorSpace&) = delete;

  RgbaColor GetColor(BitDepth bit_depth, uint8_t entry_id) const;

  void SetColor(BitDepth bit_depth, uint8_t entry_id, RgbaColor color);
  /// Must pass a 4-element array; elements are copied over.
  void Set2To4BitDepthMap(const uint8_t* map);
  /// Must pass a 4-element array; elements are copied over.
  void Set2To8BitDepthMap(const uint8_t* map);
  /// Must pass a 16-element array; elements are copied over.
  void Set4To8BitDepthMap(const uint8_t* map);

 private:
  RgbaColor GetColorRaw(BitDepth bit_depth, uint8_t entry_id) const;

  // These hold the colors for each entry ID.  Each value is initialized to the
  // special value kNoColor meaning there isn't a value present.
  RgbaColor color_map_2_[4];
  RgbaColor color_map_4_[16];
  RgbaColor color_map_8_[256];
  // See ETSI EN 300 743 Sections 10.4, 10.5, 10.6 for defaults.
  uint8_t bit_depth_2_to_4_[4] = {0x0, 0x7, 0x8, 0xf};
  uint8_t bit_depth_2_to_8_[4] = {0x0, 0x77, 0x88, 0xff};
  uint8_t bit_depth_4_to_8_[16] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55,
                                   0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb,
                                   0xcc, 0xdd, 0xee, 0xff};
};

/// Defines a builder that generates an image from a DVB-sub byte stream.  This
/// allocates a single buffer big enough to hold the max-sized image and fills
/// it in series.  The NewRow() method must be called to start a new line of the
/// image.
///
/// This adds pixels in an interlaced format.  Adding pixels and new rows on
/// top-rows doesn't affect the bottom-rows.  Top-rows refers to even-indexed
/// lines (e.g. 0,2,4).
class DvbImageBuilder {
 public:
  DvbImageBuilder(const DvbImageColorSpace* color_space,
                  const RgbaColor& default_color,
                  uint16_t max_width,
                  uint16_t max_height);
  ~DvbImageBuilder();

  DvbImageBuilder(const DvbImageBuilder&) = delete;
  DvbImageBuilder& operator=(const DvbImageBuilder&) = delete;

  uint16_t max_width() const { return max_width_; }
  uint16_t max_height() const { return max_height_; }

  bool AddPixel(BitDepth bit_depth, uint8_t byte_code, bool is_top_rows);
  void NewRow(bool is_top_rows);
  /// Copies the top-rows to the bottom rows.
  void MirrorToBottomRows();

  /// Gets the pixel buffer.  Each row is based on the max_width field, but
  /// the max filled row with will be given in `width`.  This assumes that
  /// NewRow was called recently and we are at the beginning of the rows.
  ///
  /// @param pixels A pointer to a RgbaColor* variable that will be set to the
  ///               pixel data pointer.
  /// @param width Will be filled with the max width of all rows.
  /// @param height Will be filled with the number of rows set.
  /// @return True on success, false on error.
  bool GetPixels(const RgbaColor** pixels,
                 uint16_t* width,
                 uint16_t* height) const;

 private:
  struct Position {
    uint16_t x, y;
  };

  const std::unique_ptr<RgbaColor[]> pixels_;
  const DvbImageColorSpace* const color_space_;
  Position top_pos_, bottom_pos_;
  const uint16_t max_width_;
  const uint16_t max_height_;
  uint16_t width_;
};

}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_DVB_DVB_IMAGE_H_
