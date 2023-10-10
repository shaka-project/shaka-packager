// Copyright 2020 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/media/formats/dvb/dvb_image.h>

#include <algorithm>
#include <cstring>
#include <tuple>

#include <absl/log/check.h>
#include <absl/log/log.h>

namespace shaka {
namespace media {

namespace {

// See ETSI EN 300 743 Section 9.1.
constexpr const uint8_t k4To2ReductionMap[] = {
    0x0, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1,
    0x2, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3,
};

// The only time when A==0 is when it is transparent.  This means we can use
// other values internally for special values.
constexpr const RgbaColor kNoColor{145, 92, 47, 0};

// DVB uses transparency, but libpng uses alpha, so we need to reverse the T
// value so we can pass the value to libpng.
#define COLOR(r, g, b, t)                           \
  RgbaColor {                                       \
    static_cast<uint8_t>(255 * (r) / 100),          \
        static_cast<uint8_t>(255 * (g) / 100),      \
        static_cast<uint8_t>(255 * (b) / 100),      \
        static_cast<uint8_t>(255 * (100 - t) / 100) \
  }
// Default color maps see ETSI EN 300 743 Section 10.
constexpr const RgbaColor k2BitDefaultColors[] = {
    COLOR(0, 0, 0, 100),      // 0 = 0b00
    COLOR(100, 100, 100, 0),  // 1 = 0b01
    COLOR(0, 0, 0, 0),        // 2 = 0b10
    COLOR(50, 50, 50, 0),     // 3 = 0b11
};
// Default color maps see ETSI EN 300 743 Section 10.
constexpr const RgbaColor k4BitDefaultColors[] = {
    COLOR(0, 0, 0, 100),      // 0 = 0b0000
    COLOR(100, 0, 0, 0),      // 1 = 0b0001
    COLOR(0, 100, 0, 0),      // 2 = 0b0010
    COLOR(100, 100, 0, 0),    // 3 = 0b0011
    COLOR(0, 0, 100, 0),      // 4 = 0b0100
    COLOR(100, 0, 100, 0),    // 5 = 0b0101
    COLOR(0, 100, 100, 0),    // 6 = 0b0110
    COLOR(100, 100, 100, 0),  // 7 = 0b0111

    COLOR(0, 0, 0, 0),     //  8 = 0b1000
    COLOR(50, 0, 0, 0),    //  9 = 0b1001
    COLOR(0, 50, 0, 0),    // 10 = 0b1010
    COLOR(50, 50, 0, 0),   // 11 = 0b1011
    COLOR(0, 0, 50, 0),    // 12 = 0b1100
    COLOR(50, 0, 50, 0),   // 13 = 0b1101
    COLOR(0, 50, 50, 0),   // 14 = 0b1110
    COLOR(50, 50, 50, 0),  // 15 = 0b1111
};

#define GET_BIT(n) ((entry_id >> (8 - (n))) & 0x1)
// Default color maps see ETSI EN 300 743 Section 10.
RgbaColor Get8BitDefaultColor(uint8_t entry_id) {
  uint8_t r, g, b, t;
  if (entry_id == 0) {
    return COLOR(0, 0, 0, 100);
  } else if ((entry_id & 0xf8) == 0) {
    r = 100 * GET_BIT(8);
    g = 100 * GET_BIT(7);
    b = 100 * GET_BIT(6);
    t = 75;
  } else if (!GET_BIT(1)) {
    r = (33 * GET_BIT(8)) + (67 * GET_BIT(4));
    g = (33 * GET_BIT(7)) + (67 * GET_BIT(3));
    b = (33 * GET_BIT(6)) + (67 * GET_BIT(2));
    t = GET_BIT(5) ? 50 : 0;
  } else {
    r = (17 * GET_BIT(8)) + (33 * GET_BIT(4)) + (GET_BIT(5) ? 0 : 50);
    g = (17 * GET_BIT(7)) + (33 * GET_BIT(3)) + (GET_BIT(5) ? 0 : 50);
    b = (17 * GET_BIT(6)) + (33 * GET_BIT(2)) + (GET_BIT(5) ? 0 : 50);
    t = 0;
  }
  return COLOR(r, g, b, t);
}
#undef GET_BIT
#undef COLOR

}  // namespace

DvbImageColorSpace::DvbImageColorSpace() {
  for (auto& item : color_map_2_)
    item = kNoColor;
  for (auto& item : color_map_4_)
    item = kNoColor;
  for (auto& item : color_map_8_)
    item = kNoColor;
}

DvbImageColorSpace::~DvbImageColorSpace() {}

RgbaColor DvbImageColorSpace::GetColor(BitDepth bit_depth,
                                       uint8_t entry_id) const {
  auto color = GetColorRaw(bit_depth, entry_id);
  if (color != kNoColor)
    return color;

  // If we don't have the exact bit-depth, try mapping to another bit-depth.
  // See ETSI EN 300 743 Section 9.
  RgbaColor default_color, alt1, alt2;
  switch (bit_depth) {
    case BitDepth::k2Bit:
      DCHECK_LT(entry_id, 4u);
      alt1 = GetColorRaw(BitDepth::k4Bit, bit_depth_2_to_4_[entry_id]);
      alt2 = GetColorRaw(BitDepth::k8Bit, bit_depth_2_to_8_[entry_id]);
      default_color = k2BitDefaultColors[entry_id];
      break;
    case BitDepth::k4Bit:
      DCHECK_LT(entry_id, 16u);
      alt1 = GetColorRaw(BitDepth::k8Bit, bit_depth_4_to_8_[entry_id]);
      alt2 = GetColorRaw(BitDepth::k2Bit, k4To2ReductionMap[entry_id]);
      default_color = k4BitDefaultColors[entry_id];
      break;
    case BitDepth::k8Bit:
      // 8-to-4-bit reduction is just take the low bits.
      alt1 = GetColorRaw(BitDepth::k4Bit, entry_id & 0xf);
      alt2 = GetColorRaw(BitDepth::k2Bit, k4To2ReductionMap[entry_id & 0xf]);
      default_color = Get8BitDefaultColor(entry_id);
      break;
    default:
      // Windows can't detect that all enums are handled and doesn't like
      // NOTIMPLEMENTED.
      return kNoColor;
  }

  if (alt1 != kNoColor)
    return alt1;
  if (alt2 != kNoColor)
    return alt2;
  return default_color;
}

void DvbImageColorSpace::SetColor(BitDepth bit_depth,
                                  uint8_t entry_id,
                                  RgbaColor color) {
  DCHECK(color != kNoColor);
  switch (bit_depth) {
    case BitDepth::k2Bit:
      DCHECK_LT(entry_id, 4u);
      color_map_2_[entry_id] = color;
      break;
    case BitDepth::k4Bit:
      DCHECK_LT(entry_id, 16u);
      color_map_4_[entry_id] = color;
      break;
    case BitDepth::k8Bit:
      color_map_8_[entry_id] = color;
      break;
  }
}

void DvbImageColorSpace::Set2To4BitDepthMap(const uint8_t* map) {
  memcpy(bit_depth_2_to_4_, map, sizeof(bit_depth_2_to_4_));
}

void DvbImageColorSpace::Set2To8BitDepthMap(const uint8_t* map) {
  memcpy(bit_depth_2_to_8_, map, sizeof(bit_depth_2_to_8_));
}

void DvbImageColorSpace::Set4To8BitDepthMap(const uint8_t* map) {
  memcpy(bit_depth_4_to_8_, map, sizeof(bit_depth_4_to_8_));
}

RgbaColor DvbImageColorSpace::GetColorRaw(BitDepth bit_depth,
                                          uint8_t entry_id) const {
  switch (bit_depth) {
    case BitDepth::k2Bit:
      return color_map_2_[entry_id];
    case BitDepth::k4Bit:
      return color_map_4_[entry_id];
    case BitDepth::k8Bit:
      return color_map_8_[entry_id];
  }
  // Not reached, but Windows doesn't like NOTIMPLEMENTED.
  return kNoColor;
}

DvbImageBuilder::DvbImageBuilder(const DvbImageColorSpace* color_space,
                                 const RgbaColor& default_color,
                                 uint16_t max_width,
                                 uint16_t max_height)
    : pixels_(new RgbaColor[max_width * max_height]),
      color_space_(color_space),
      top_pos_{0, 0},
      bottom_pos_{0, 1},  // Skip top row for bottom row.
      max_width_(max_width),
      max_height_(max_height),
      width_(0) {
  for (size_t i = 0; i < static_cast<size_t>(max_width) * max_height; i++)
    pixels_[i] = default_color;
}

DvbImageBuilder::~DvbImageBuilder() {}

bool DvbImageBuilder::AddPixel(BitDepth bit_depth,
                               uint8_t byte_code,
                               bool is_top_rows) {
  auto& pos = is_top_rows ? top_pos_ : bottom_pos_;
  if (pos.x >= max_width_ || pos.y >= max_height_) {
    LOG(ERROR) << "DVB-sub image cannot fit in region/window";
    return false;
  }

  pixels_[pos.y * max_width_ + pos.x++] =
      color_space_->GetColor(bit_depth, byte_code);
  if (pos.x > width_)
    width_ = pos.x;
  return true;
}

void DvbImageBuilder::NewRow(bool is_top_rows) {
  auto& pos = is_top_rows ? top_pos_ : bottom_pos_;
  pos.x = 0;
  pos.y += 2;  // Skip other row.
}

void DvbImageBuilder::MirrorToBottomRows() {
  for (size_t line = 0; line < max_height_ - 1u; line += 2) {
    std::memcpy(&pixels_[(line + 1) * max_width_], &pixels_[line * max_width_],
                max_width_ * sizeof(RgbaColor));
  }
  bottom_pos_ = top_pos_;
  if (max_height_ % 2 == 0)
    bottom_pos_.y++;
  else
    bottom_pos_.y--;  // Odd-height images don't end in odd-row, so move back.
}

bool DvbImageBuilder::GetPixels(const RgbaColor** pixels,
                                uint16_t* width,
                                uint16_t* height) const {
  size_t max_y, min_y;
  std::tie(min_y, max_y) = std::minmax(top_pos_.y, bottom_pos_.y);
  if (max_y == 1 || max_y != min_y + 1) {
    // 1. We should have at least one row.
    // 2. Both top-rows and bottom-rows should have the same number of rows.
    LOG(ERROR) << "Incomplete DVB-sub image";
    return false;
  }

  *width = width_;
  // We skipped the other row in NewRow, so rollback.
  *height = static_cast<uint16_t>(max_y - 1);
  *pixels = pixels_.get();
  if (*height > max_height_) {
    LOG(ERROR) << "DVB-sub image cannot fit in region/window";
    return false;
  }

  return true;
}

}  // namespace media
}  // namespace shaka
