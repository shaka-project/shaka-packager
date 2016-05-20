// Copyright 2016 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/filters/decoder_configuration.h"

namespace shaka {
namespace media {

DecoderConfiguration::DecoderConfiguration() : nalu_length_size_(0) {}
DecoderConfiguration::~DecoderConfiguration() {}

bool DecoderConfiguration::Parse(const uint8_t* data, size_t data_size) {
  data_.assign(data, data + data_size);
  nalu_.clear();
  return ParseInternal();
}

void DecoderConfiguration::AddNalu(const Nalu& nalu) {
  nalu_.push_back(nalu);
}

}  // namespace media
}  // namespace shaka
