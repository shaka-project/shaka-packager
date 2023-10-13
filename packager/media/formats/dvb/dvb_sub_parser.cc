// Copyright 2020 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/media/formats/dvb/dvb_sub_parser.h>

#include <algorithm>

#include <absl/log/check.h>
#include <absl/log/log.h>

#include <packager/media/formats/mp2t/mp2t_common.h>

namespace shaka {
namespace media {

namespace {

RgbaColor ConvertYuv(uint8_t Y, uint8_t Cr, uint8_t Cb, uint8_t T) {
  // Converts based on ITU-R BT.601.
  // See https://en.wikipedia.org/wiki/YCbCr
  //
  // Note that the T value should be interpolated based on a full transparency
  // being 256.  This means that T=255 should not be fully transparent.  Y=0 is
  // used to signal full transparency.
  // Values for Y<16 (except Y=0) are invalid, so clamp to 16.
  RgbaColor color;
  const double y_transform = 255.0 / 219 * (std::max<uint8_t>(Y, 16) - 16);
  const double cb_transform = 255.0 / 244 * 1.772 * (Cb - 128);
  const double cr_transform = 255.0 / 244 * 1.402 * (Cr - 128);
  const double f1 = 0.114 / 0.587;
  const double f2 = 0.299 / 0.587;
  color.r = static_cast<uint8_t>(y_transform + cr_transform);
  color.g =
      static_cast<uint8_t>(y_transform - cb_transform * f1 - cr_transform * f2);
  color.b = static_cast<uint8_t>(y_transform + cb_transform);
  color.a = Y == 0 ? 0 : (T == 0 ? 255 : 256 - T);
  return color;
}

}  // namespace

DvbSubParser::DvbSubParser() : last_pts_(0), timeout_(0) {}

DvbSubParser::~DvbSubParser() {}

bool DvbSubParser::Parse(DvbSubSegmentType segment_type,
                         int64_t pts,
                         const uint8_t* payload,
                         size_t size,
                         std::vector<std::shared_ptr<TextSample>>* samples) {
  switch (segment_type) {
    case DvbSubSegmentType::kPageComposition:
      return ParsePageComposition(pts, payload, size, samples);
    case DvbSubSegmentType::kRegionComposition:
      return ParseRegionComposition(payload, size);
    case DvbSubSegmentType::kClutDefinition:
      return ParseClutDefinition(payload, size);
    case DvbSubSegmentType::kObjectData:
      return ParseObjectData(pts, payload, size);
    case DvbSubSegmentType::kDisplayDefinition:
      return ParseDisplayDefinition(payload, size);
    case DvbSubSegmentType::kEndOfDisplay:
      // This signals all the current objects are available.  But we need to
      // know the end time, so we do nothing for now.
      return true;
    default:
      LOG(WARNING) << "Unknown DVB-sub segment_type=0x" << std::hex
                   << static_cast<uint32_t>(segment_type);
      return true;
  }
}

bool DvbSubParser::Flush(std::vector<std::shared_ptr<TextSample>>* samples) {
  RCHECK(composer_.GetSamples(last_pts_, last_pts_ + timeout_ * kMpeg2Timescale,
                              samples));
  composer_.ClearObjects();
  return true;
}

const DvbImageColorSpace* DvbSubParser::GetColorSpace(uint8_t clut_id) {
  return composer_.GetColorSpace(clut_id);
}

const DvbImageBuilder* DvbSubParser::GetImageForObject(uint16_t object_id) {
  return composer_.GetObjectImage(object_id);
}

bool DvbSubParser::ParsePageComposition(
    int64_t pts,
    const uint8_t* data,
    size_t size,
    std::vector<std::shared_ptr<TextSample>>* samples) {
  // See ETSI EN 300 743 Section 7.2.2.
  BitReader reader(data, size);

  uint8_t page_state;
  RCHECK(reader.ReadBits(8, &timeout_));
  RCHECK(reader.SkipBits(4));  // page_version_number
  RCHECK(reader.ReadBits(2, &page_state));
  RCHECK(reader.SkipBits(2));  // reserved
  if (page_state == 0x1 || page_state == 0x2) {
    // If this is a "acquisition point" or a "mode change", then this is a new
    // page and we should clear the old data.
    RCHECK(composer_.GetSamples(last_pts_, pts, samples));
    composer_.ClearObjects();
    last_pts_ = pts;
  }

  while (reader.bits_available() > 0u) {
    uint8_t region_id;
    uint16_t x, y;
    RCHECK(reader.ReadBits(8, &region_id));
    RCHECK(reader.SkipBits(8));  // reserved
    RCHECK(reader.ReadBits(16, &x));
    RCHECK(reader.ReadBits(16, &y));

    RCHECK(composer_.SetRegionPosition(region_id, x, y));
  }

  return true;
}

bool DvbSubParser::ParseRegionComposition(const uint8_t* data, size_t size) {
  // See ETSI EN 300 743 Section 7.2.3.
  BitReader reader(data, size);

  uint8_t region_id, clut_id;
  uint16_t region_width, region_height;
  bool region_fill_flag;
  int background_pixel_code;
  RCHECK(reader.ReadBits(8, &region_id));
  RCHECK(reader.SkipBits(4));  // region_version_number
  RCHECK(reader.ReadBits(1, &region_fill_flag));
  RCHECK(reader.SkipBits(3));  // reserved
  RCHECK(reader.ReadBits(16, &region_width));
  RCHECK(reader.ReadBits(16, &region_height));
  RCHECK(reader.SkipBits(3));  // region_level_of_compatibility
  RCHECK(reader.SkipBits(3));  // region_depth
  RCHECK(reader.SkipBits(2));  // reserved
  RCHECK(reader.ReadBits(8, &clut_id));
  RCHECK(reader.ReadBits(8, &background_pixel_code));
  RCHECK(reader.SkipBits(4));  // region_4-bit_pixel_code
  RCHECK(reader.SkipBits(2));  // region_2-bit_pixel_code
  RCHECK(reader.SkipBits(2));  // reserved
  RCHECK(
      composer_.SetRegionInfo(region_id, clut_id, region_width, region_height));
  if (!region_fill_flag)
    background_pixel_code = -1;

  while (reader.bits_available() > 0) {
    uint16_t object_id, x, y;
    uint8_t object_type;
    RCHECK(reader.ReadBits(16, &object_id));
    RCHECK(reader.ReadBits(2, &object_type));
    RCHECK(reader.SkipBits(2));  // object_provider_flag
    RCHECK(reader.ReadBits(12, &x));
    RCHECK(reader.SkipBits(4));  // reserved
    RCHECK(reader.ReadBits(12, &y));

    if (object_type == 0x01 || object_type == 0x02) {
      RCHECK(reader.SkipBits(8));  // foreground_pixel_code
      RCHECK(reader.SkipBits(8));  // background_pixel_code
    }
    RCHECK(composer_.SetObjectInfo(object_id, region_id, x, y,
                                   background_pixel_code));
  }

  return true;
}

bool DvbSubParser::ParseClutDefinition(const uint8_t* data, size_t size) {
  // See ETSI EN 300 743 Section 7.2.4.
  BitReader reader(data, size);

  uint8_t clut_id;
  RCHECK(reader.ReadBits(8, &clut_id));
  auto* color_space = composer_.GetColorSpace(clut_id);
  RCHECK(reader.SkipBits(4));  // CLUT_version_number
  RCHECK(reader.SkipBits(4));  // reserved
  while (reader.bits_available() > 0) {
    uint8_t clut_entry_id;
    uint8_t has_2_bit;
    uint8_t has_4_bit;
    uint8_t has_8_bit;
    uint8_t full_range_flag;
    RCHECK(reader.ReadBits(8, &clut_entry_id));
    RCHECK(reader.ReadBits(1, &has_2_bit));
    RCHECK(reader.ReadBits(1, &has_4_bit));
    RCHECK(reader.ReadBits(1, &has_8_bit));
    RCHECK(reader.SkipBits(4));  // reserved
    RCHECK(reader.ReadBits(1, &full_range_flag));

    if (has_2_bit + has_4_bit + has_8_bit != 1) {
      LOG(ERROR) << "Must specify exactly one bit depth in CLUT definition";
      return false;
    }
    const BitDepth bit_depth =
        has_2_bit ? BitDepth::k2Bit
                  : (has_4_bit ? BitDepth::k4Bit : BitDepth::k8Bit);

    uint8_t Y, Cr, Cb, T;
    if (full_range_flag) {
      RCHECK(reader.ReadBits(8, &Y));
      RCHECK(reader.ReadBits(8, &Cr));
      RCHECK(reader.ReadBits(8, &Cb));
      RCHECK(reader.ReadBits(8, &T));
    } else {
      // These store the most-significant bits, so shift them up.
      RCHECK(reader.ReadBits(6, &Y));
      Y <<= 2;
      RCHECK(reader.ReadBits(4, &Cr));
      Cr <<= 4;
      RCHECK(reader.ReadBits(4, &Cb));
      Cb <<= 4;
      RCHECK(reader.ReadBits(2, &T));
      T <<= 6;
    }
    color_space->SetColor(bit_depth, clut_entry_id, ConvertYuv(Y, Cr, Cb, T));
  }

  return true;
}

bool DvbSubParser::ParseObjectData(int64_t pts,
                                   const uint8_t* data,
                                   size_t size) {
  // See ETSI EN 300 743 Section 7.2.5 Table 17.
  BitReader reader(data, size);

  uint16_t object_id;
  uint8_t object_coding_method;
  RCHECK(reader.ReadBits(16, &object_id));
  RCHECK(reader.SkipBits(4));  // object_version_number
  RCHECK(reader.ReadBits(2, &object_coding_method));
  RCHECK(reader.SkipBits(1));  // non_modifying_colour_flag
  RCHECK(reader.SkipBits(1));  // reserved

  auto* image = composer_.GetObjectImage(object_id);
  auto* color_space = composer_.GetColorSpaceForObject(object_id);
  if (!image || !color_space)
    return false;

  if (object_coding_method == 0) {
    uint16_t top_field_length;
    uint16_t bottom_field_length;
    RCHECK(reader.ReadBits(16, &top_field_length));
    RCHECK(reader.ReadBits(16, &bottom_field_length));

    RCHECK(ParsePixelDataSubObject(top_field_length, true, &reader, color_space,
                                   image));
    RCHECK(ParsePixelDataSubObject(bottom_field_length, false, &reader,
                                   color_space, image));
    // Ignore 8_stuff_bits since we don't need to read to the end.

    if (bottom_field_length == 0) {
      // If there are no bottom rows, then the top rows are used instead.  See
      // beginning of section 7.2.5.1.
      image->MirrorToBottomRows();
    }
  } else {
    LOG(ERROR) << "Unsupported DVB-sub object coding method: "
               << static_cast<int>(object_coding_method);
    return false;
  }
  return true;
}

bool DvbSubParser::ParseDisplayDefinition(const uint8_t* data, size_t size) {
  // See ETSI EN 300 743 Section 7.2.1.
  BitReader reader(data, size);

  uint16_t width, height;
  RCHECK(reader.SkipBits(4));  // dds_version_number
  RCHECK(reader.SkipBits(1));  // display_window_flag
  RCHECK(reader.SkipBits(3));  // reserved
  RCHECK(reader.ReadBits(16, &width));
  RCHECK(reader.ReadBits(16, &height));
  // Size is stored as -1.
  composer_.SetDisplaySize(width + 1, height + 1);

  return true;
}

bool DvbSubParser::ParsePixelDataSubObject(size_t sub_object_length,
                                           bool is_top_fields,
                                           BitReader* reader,
                                           DvbImageColorSpace* color_space,
                                           DvbImageBuilder* image) {
  const size_t start = reader->bit_position() / 8;
  while (reader->bit_position() / 8 < start + sub_object_length) {
    // See ETSI EN 300 743 Section 7.2.5.1 Table 20
    uint8_t data_type;
    RCHECK(reader->ReadBits(8, &data_type));
    uint8_t temp[16];
    switch (data_type) {
      case 0x10:
        RCHECK(Parse2BitPixelData(is_top_fields, reader, image));
        reader->SkipToNextByte();
        break;
      case 0x11:
        RCHECK(Parse4BitPixelData(is_top_fields, reader, image));
        reader->SkipToNextByte();
        break;
      case 0x12:
        RCHECK(Parse8BitPixelData(is_top_fields, reader, image));
        break;
      case 0x20:
        for (int i = 0; i < 4; i++) {
          RCHECK(reader->ReadBits(4, &temp[i]));
        }
        color_space->Set2To4BitDepthMap(temp);
        break;
      case 0x21:
        for (int i = 0; i < 4; i++) {
          RCHECK(reader->ReadBits(8, &temp[i]));
        }
        color_space->Set2To8BitDepthMap(temp);
        break;
      case 0x22:
        for (int i = 0; i < 16; i++) {
          RCHECK(reader->ReadBits(8, &temp[i]));
        }
        color_space->Set4To8BitDepthMap(temp);
        break;
      case 0xf0:
        image->NewRow(is_top_fields);
        break;
      default:
        LOG(ERROR) << "Unsupported DVB-sub pixel data format: 0x" << std::hex
                   << static_cast<int>(data_type);
        return false;
    }
  }
  return true;
}

bool DvbSubParser::Parse2BitPixelData(bool is_top_fields,
                                      BitReader* reader,
                                      DvbImageBuilder* image) {
  // 2-bit/pixel code string, Section 7.2.5.2.1, Table 22.
  while (true) {
    uint8_t peek;
    RCHECK(reader->ReadBits(2, &peek));
    if (peek != 0) {
      RCHECK(image->AddPixel(BitDepth::k2Bit, peek, is_top_fields));
    } else {
      uint8_t switch_1;
      RCHECK(reader->ReadBits(1, &switch_1));
      if (switch_1 == 1) {
        uint8_t count_minus_3;
        RCHECK(reader->ReadBits(3, &count_minus_3));
        RCHECK(reader->ReadBits(2, &peek));
        for (uint8_t i = 0; i < count_minus_3 + 3; i++)
          RCHECK(image->AddPixel(BitDepth::k2Bit, peek, is_top_fields));
      } else {
        uint8_t switch_2;
        RCHECK(reader->ReadBits(1, &switch_2));
        if (switch_2 == 1) {
          RCHECK(image->AddPixel(BitDepth::k2Bit, 0, is_top_fields));
        } else {
          uint8_t switch_3;
          RCHECK(reader->ReadBits(2, &switch_3));
          if (switch_3 == 0) {
            break;
          } else if (switch_3 == 1) {
            RCHECK(image->AddPixel(BitDepth::k2Bit, 0, is_top_fields));
            RCHECK(image->AddPixel(BitDepth::k2Bit, 0, is_top_fields));
          } else if (switch_3 == 2) {
            uint8_t count_minus_12;
            RCHECK(reader->ReadBits(4, &count_minus_12));
            RCHECK(reader->ReadBits(2, &peek));
            for (uint8_t i = 0; i < count_minus_12 + 12; i++)
              RCHECK(image->AddPixel(BitDepth::k2Bit, peek, is_top_fields));
          } else if (switch_3 == 3) {
            uint8_t count_minus_29;
            RCHECK(reader->ReadBits(8, &count_minus_29));
            RCHECK(reader->ReadBits(2, &peek));
            for (uint8_t i = 0; i < count_minus_29 + 29; i++)
              RCHECK(image->AddPixel(BitDepth::k2Bit, peek, is_top_fields));
          }
        }
      }
    }
  }

  return true;
}

bool DvbSubParser::Parse4BitPixelData(bool is_top_fields,
                                      BitReader* reader,
                                      DvbImageBuilder* image) {
  // 4-bit/pixel code string, Section 7.2.5.2.2, Table 24.
  DCHECK(reader->bits_available() % 8 == 0);
  while (true) {
    uint8_t peek;
    RCHECK(reader->ReadBits(4, &peek));
    if (peek != 0) {
      RCHECK(image->AddPixel(BitDepth::k4Bit, peek, is_top_fields));
    } else {
      uint8_t switch_1;
      RCHECK(reader->ReadBits(1, &switch_1));
      if (switch_1 == 0) {
        RCHECK(reader->ReadBits(3, &peek));
        if (peek != 0) {
          for (int i = 0; i < peek + 2; i++)
            RCHECK(image->AddPixel(BitDepth::k4Bit, 0, is_top_fields));
        } else {
          break;
        }
      } else {
        uint8_t switch_2;
        RCHECK(reader->ReadBits(1, &switch_2));
        if (switch_2 == 0) {
          RCHECK(reader->ReadBits(2, &peek));  // run_length_4-7
          uint8_t code;
          RCHECK(reader->ReadBits(4, &code));
          for (int i = 0; i < peek + 4; i++)
            RCHECK(image->AddPixel(BitDepth::k4Bit, code, is_top_fields));
        } else {
          uint8_t switch_3;
          RCHECK(reader->ReadBits(2, &switch_3));
          if (switch_3 == 0) {
            RCHECK(image->AddPixel(BitDepth::k4Bit, 0, is_top_fields));
          } else if (switch_3 == 1) {
            RCHECK(image->AddPixel(BitDepth::k4Bit, 0, is_top_fields));
            RCHECK(image->AddPixel(BitDepth::k4Bit, 0, is_top_fields));
          } else if (switch_3 == 2) {
            RCHECK(reader->ReadBits(4, &peek));  // run_length_9-24
            uint8_t code;
            RCHECK(reader->ReadBits(4, &code));
            for (int i = 0; i < peek + 9; i++)
              RCHECK(image->AddPixel(BitDepth::k4Bit, code, is_top_fields));
          } else {
            // switch_3 == 3
            RCHECK(reader->ReadBits(8, &peek));  // run_length_25-280
            uint8_t code;
            RCHECK(reader->ReadBits(4, &code));
            for (int i = 0; i < peek + 25; i++)
              RCHECK(image->AddPixel(BitDepth::k4Bit, code, is_top_fields));
          }
        }
      }
    }
  }
  return true;
}

bool DvbSubParser::Parse8BitPixelData(bool is_top_fields,
                                      BitReader* reader,
                                      DvbImageBuilder* image) {
  // 8-bit/pixel code string, Section 7.2.5.2.3, Table 26.
  while (true) {
    uint8_t peek;
    RCHECK(reader->ReadBits(8, &peek));
    if (peek != 0) {
      RCHECK(image->AddPixel(BitDepth::k8Bit, peek, is_top_fields));
    } else {
      uint8_t switch_1;
      RCHECK(reader->ReadBits(1, &switch_1));
      if (switch_1 == 0) {
        RCHECK(reader->ReadBits(7, &peek));
        if (peek != 0) {
          for (uint8_t i = 0; i < peek; i++)
            RCHECK(image->AddPixel(BitDepth::k8Bit, 0, is_top_fields));
        } else {
          break;
        }
      } else {
        uint8_t count;
        RCHECK(reader->ReadBits(7, &count));
        RCHECK(reader->ReadBits(8, &peek));
        for (uint8_t i = 0; i < count; i++)
          RCHECK(image->AddPixel(BitDepth::k8Bit, peek, is_top_fields));
      }
    }
  }

  return true;
}

}  // namespace media
}  // namespace shaka
