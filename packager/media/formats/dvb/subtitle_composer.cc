// Copyright 2020 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/media/formats/dvb/subtitle_composer.h>

#include <cstring>

#include <absl/log/check.h>
#include <absl/log/log.h>
#include <png.h>

#include <packager/macros/logging.h>

namespace shaka {
namespace media {

namespace {

const uint16_t kDefaultWidth = 720;
const uint16_t kDefaultHeight = 576;
const RgbaColor kTransparent{0, 0, 0, 0};

struct PngFreeHelper {
  PngFreeHelper(png_structp* png, png_infop* info) : png(png), info(info) {}
  ~PngFreeHelper() { png_destroy_write_struct(png, info); }

  png_structp* png;
  png_infop* info;
};

void PngWriteData(png_structp png, png_bytep data, png_size_t length) {
  auto* output = reinterpret_cast<std::vector<uint8_t>*>(png_get_io_ptr(png));
  output->insert(output->end(), data, data + length);
}

void PngFlushData(png_structp png) {}

bool IsTransparent(const RgbaColor* colors, uint16_t width, uint16_t height) {
  for (size_t i = 0; i < static_cast<size_t>(width) * height; i++) {
    if (colors[i].a != 0)
      return false;
  }
  return true;
}

bool GetImageData(const DvbImageBuilder* image,
                  std::vector<uint8_t>* data,
                  uint16_t* width,
                  uint16_t* height) {
  // CAREFUL in this method since this uses long-jumps.  A long-jump causes the
  // execution to jump to another point *without executing returns*.  This
  // causes C++ objects to not get destroyed.  This also causes the same code to
  // be executed twice, so C++ objects can be initialized twice.
  //
  // So long as we don't create C++ objects after the long-jump point,
  // everything should work fine.  If we early-return after the long-jump, the
  // destructors will still be called; if we long-jump, we won't call the
  // constructors since we're past that point.
  auto png =
      png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
  auto info = png_create_info_struct(png);
  PngFreeHelper helper(&png, &info);
  if (!png || !info) {
    LOG(ERROR) << "Error creating libpng struct";
    return false;
  }
  if (setjmp(png_jmpbuf(png))) {
    // If any png_* functions fail, the code will jump back to here.
    LOG(ERROR) << "Error writing PNG image";
    return false;
  }
  png_set_write_fn(png, data, &PngWriteData, &PngFlushData);

  const RgbaColor* pixels;
  if (!image->GetPixels(&pixels, width, height))
    return false;
  if (IsTransparent(pixels, *width, *height))
    return true;  // Skip empty/transparent images.
  png_set_IHDR(png, info, *width, *height, 8, PNG_COLOR_TYPE_RGBA,
               PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE,
               PNG_FILTER_TYPE_BASE);
  png_write_info(png, info);

  const uint8_t* in_data = reinterpret_cast<const uint8_t*>(pixels);
  for (size_t y = 0; y < *height; y++) {
    size_t offset = image->max_width() * y * sizeof(RgbaColor);
    png_write_row(png, in_data + offset);
  }
  png_write_end(png, nullptr);

  return true;
}

}  // namespace

SubtitleComposer::SubtitleComposer()
    : display_width_(kDefaultWidth), display_height_(kDefaultHeight) {}

SubtitleComposer::~SubtitleComposer() {}

void SubtitleComposer::SetDisplaySize(uint16_t width, uint16_t height) {
  display_width_ = width;
  display_height_ = height;
}

bool SubtitleComposer::SetRegionInfo(uint8_t region_id,
                                     uint8_t color_space_id,
                                     uint16_t width,
                                     uint16_t height) {
  auto* region = &regions_[region_id];
  if (region->x + width > display_width_ ||
      region->y + height > display_height_) {
    LOG(ERROR) << "DVB-sub region won't fit within display";
    return false;
  }
  if (width == 0 || height == 0) {
    LOG(ERROR) << "DVB-sub width/height cannot be 0";
    return false;
  }

  region->width = width;
  region->height = height;
  region->color_space = &color_spaces_[color_space_id];
  return true;
}

bool SubtitleComposer::SetRegionPosition(uint8_t region_id,
                                         uint16_t x,
                                         uint16_t y) {
  auto* region = &regions_[region_id];
  if (x + region->width > display_width_ ||
      y + region->height > display_height_) {
    LOG(ERROR) << "DVB-sub region won't fit within display";
    return false;
  }

  region->x = x;
  region->y = y;
  return true;
}

bool SubtitleComposer::SetObjectInfo(uint16_t object_id,
                                     uint8_t region_id,
                                     uint16_t x,
                                     uint16_t y,
                                     int default_color_code) {
  auto region = regions_.find(region_id);
  if (region == regions_.end()) {
    LOG(ERROR) << "Unknown DVB-sub region: " << (int)region_id
               << ", in object: " << object_id;
    return false;
  }
  if (x >= region->second.width || y >= region->second.height) {
    LOG(ERROR) << "DVB-sub object is outside region: " << object_id;
    return false;
  }

  auto* object = &objects_[object_id];
  object->region = &region->second;
  object->default_color_code = default_color_code;
  object->x = x;
  object->y = y;
  return true;
}

DvbImageColorSpace* SubtitleComposer::GetColorSpace(uint8_t color_space_id) {
  return &color_spaces_[color_space_id];
}

DvbImageColorSpace* SubtitleComposer::GetColorSpaceForObject(
    uint16_t object_id) {
  auto info = objects_.find(object_id);
  if (info == objects_.end()) {
    LOG(ERROR) << "Unknown DVB-sub object: " << object_id;
    return nullptr;
  }
  return info->second.region->color_space;
}

DvbImageBuilder* SubtitleComposer::GetObjectImage(uint16_t object_id) {
  auto it = images_.find(object_id);
  if (it == images_.end()) {
    auto info = objects_.find(object_id);
    if (info == objects_.end()) {
      LOG(ERROR) << "Unknown DVB-sub object: " << object_id;
      return nullptr;
    }

    auto color = info->second.default_color_code < 0
                     ? kTransparent
                     : info->second.region->color_space->GetColor(
                           BitDepth::k8Bit, info->second.default_color_code);
    it = images_
             .emplace(std::piecewise_construct, std::make_tuple(object_id),
                      std::make_tuple(
                          info->second.region->color_space, color,
                          info->second.region->width - info->second.region->x,
                          info->second.region->height - info->second.region->y))
             .first;
  }
  return &it->second;
}

bool SubtitleComposer::GetSamples(
    int64_t start,
    int64_t end,
    std::vector<std::shared_ptr<TextSample>>* samples) const {
  for (const auto& pair : objects_) {
    auto it = images_.find(pair.first);
    if (it == images_.end()) {
      LOG(WARNING) << "DVB-sub object " << pair.first
                   << " doesn't include object data";
      continue;
    }

    uint16_t width, height;
    std::vector<uint8_t> image_data;
    if (!GetImageData(&it->second, &image_data, &width, &height))
      return false;
    if (image_data.empty()) {
      VLOG(1) << "Skipping transparent object";
      continue;
    }
    TextFragment body({}, image_data);
    DCHECK_LE(width, display_width_);
    DCHECK_LE(height, display_height_);

    TextSettings settings;
    settings.position.emplace(
        (pair.second.x + pair.second.region->x) * 100.0f / display_width_,
        TextUnitType::kPercent);
    settings.line.emplace(
        (pair.second.y + pair.second.region->y) * 100.0f / display_height_,
        TextUnitType::kPercent);
    settings.width.emplace(width * 100.0f / display_width_,
                           TextUnitType::kPercent);
    settings.height.emplace(height * 100.0f / display_height_,
                            TextUnitType::kPercent);

    samples->emplace_back(
        std::make_shared<TextSample>("", start, end, settings, body));
  }

  return true;
}

void SubtitleComposer::ClearObjects() {
  regions_.clear();
  objects_.clear();
  images_.clear();
}

}  // namespace media
}  // namespace shaka
