// Copyright 2014 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/media/base/media_sample.h>

#include <cinttypes>

#include <absl/log/check.h>
#include <absl/log/log.h>
#include <absl/strings/str_format.h>

namespace shaka {
namespace media {

MediaSample::MediaSample(const uint8_t* data,
                         size_t data_size,
                         const uint8_t* side_data,
                         size_t side_data_size,
                         bool is_key_frame)
    : is_key_frame_(is_key_frame) {
  if (!data) {
    CHECK_EQ(data_size, 0u);
  }

  SetData(data, data_size);
  if (side_data) {
    std::shared_ptr<uint8_t> shared_side_data(new uint8_t[side_data_size],
                                              std::default_delete<uint8_t[]>());
    memcpy(shared_side_data.get(), side_data, side_data_size);
    side_data_ = std::move(shared_side_data);
    side_data_size_ = side_data_size;
  }
}

MediaSample::MediaSample() {}

MediaSample::~MediaSample() {}

// static
std::shared_ptr<MediaSample> MediaSample::CopyFrom(const uint8_t* data,
                                                   size_t data_size,
                                                   bool is_key_frame) {
  // If you hit this CHECK you likely have a bug in a demuxer. Go fix it.
  CHECK(data);
  return std::shared_ptr<MediaSample>(
      new MediaSample(data, data_size, nullptr, 0u, is_key_frame));
}

// static
std::shared_ptr<MediaSample> MediaSample::CopyFrom(const uint8_t* data,
                                                   size_t data_size,
                                                   const uint8_t* side_data,
                                                   size_t side_data_size,
                                                   bool is_key_frame) {
  // If you hit this CHECK you likely have a bug in a demuxer. Go fix it.
  CHECK(data);
  return std::shared_ptr<MediaSample>(new MediaSample(
      data, data_size, side_data, side_data_size, is_key_frame));
}

// static
std::shared_ptr<MediaSample> MediaSample::FromMetadata(const uint8_t* metadata,
                                                       size_t metadata_size) {
  return std::shared_ptr<MediaSample>(
      new MediaSample(nullptr, 0, metadata, metadata_size, false));
}

// static
std::shared_ptr<MediaSample> MediaSample::CreateEmptyMediaSample() {
  return std::shared_ptr<MediaSample>(new MediaSample);
}

// static
std::shared_ptr<MediaSample> MediaSample::CreateEOSBuffer() {
  return std::shared_ptr<MediaSample>(
      new MediaSample(nullptr, 0, nullptr, 0, false));
}

std::shared_ptr<MediaSample> MediaSample::Clone() const {
  std::shared_ptr<MediaSample> new_media_sample(new MediaSample);
  new_media_sample->dts_ = dts_;
  new_media_sample->pts_ = pts_;
  new_media_sample->duration_ = duration_;
  new_media_sample->is_key_frame_ = is_key_frame_;
  new_media_sample->is_encrypted_ = is_encrypted_;
  new_media_sample->data_ = data_;
  new_media_sample->data_size_ = data_size_;
  new_media_sample->side_data_ = side_data_;
  new_media_sample->side_data_size_ = side_data_size_;
  new_media_sample->config_id_ = config_id_;
  if (decrypt_config_) {
    new_media_sample->decrypt_config_.reset(new DecryptConfig(
        decrypt_config_->key_id(), decrypt_config_->iv(),
        decrypt_config_->subsamples(), decrypt_config_->protection_scheme(),
        decrypt_config_->crypt_byte_block(),
        decrypt_config_->skip_byte_block()));
  }
  return new_media_sample;
}

void MediaSample::TransferData(std::shared_ptr<uint8_t> data,
                               size_t data_size) {
  data_ = std::move(data);
  data_size_ = data_size;
}

void MediaSample::SetData(const uint8_t* data, size_t data_size) {
  std::shared_ptr<uint8_t> shared_data(new uint8_t[data_size],
                                       std::default_delete<uint8_t[]>());
  memcpy(shared_data.get(), data, data_size);
  TransferData(std::move(shared_data), data_size);
}

std::string MediaSample::ToString() const {
  if (end_of_stream())
    return "End of stream sample\n";
  return absl::StrFormat(
      "dts: %" PRId64 "\n pts: %" PRId64 "\n duration: %" PRId64
      "\n "
      "is_key_frame: %s\n size: %zu\n side_data_size: %zu\n",
      dts_, pts_, duration_, is_key_frame_ ? "true" : "false", data_size_,
      side_data_size_);
}

}  // namespace media
}  // namespace shaka
