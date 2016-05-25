// Copyright 2016 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/codecs/decoder_configuration_record.h"

namespace shaka {
namespace media {

DecoderConfigurationRecord::DecoderConfigurationRecord()
    : nalu_length_size_(0) {}
DecoderConfigurationRecord::~DecoderConfigurationRecord() {}

bool DecoderConfigurationRecord::Parse(const uint8_t* data, size_t data_size) {
  data_.assign(data, data + data_size);
  nalu_.clear();
  return ParseInternal();
}

void DecoderConfigurationRecord::AddNalu(const Nalu& nalu) {
  nalu_.push_back(nalu);
}

}  // namespace media
}  // namespace shaka
