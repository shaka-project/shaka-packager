// Copyright (c) 2013 Google Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/media_sample.h"

#include "base/logging.h"
#include "media/base/decrypt_config.h"

namespace media {

MediaSample::MediaSample(const uint8* data,
                         int size,
                         const uint8* side_data,
                         int side_data_size,
                         bool is_key_frame)
    : dts_(0), pts_(0), duration_(0), is_key_frame_(is_key_frame) {
  if (!data) {
    CHECK_EQ(size, 0);
    CHECK(!side_data);
    return;
  }

  data_.assign(data, data + size);
  if (side_data)
    side_data_.assign(side_data, side_data + side_data_size);
}

MediaSample::~MediaSample() {}

// static
scoped_refptr<MediaSample> MediaSample::CopyFrom(const uint8* data,
                                                 int data_size,
                                                 bool is_key_frame) {
  // If you hit this CHECK you likely have a bug in a demuxer. Go fix it.
  CHECK(data);
  return make_scoped_refptr(
      new MediaSample(data, data_size, NULL, 0, is_key_frame));
}

// static
scoped_refptr<MediaSample> MediaSample::CopyFrom(const uint8* data,
                                                 int data_size,
                                                 const uint8* side_data,
                                                 int side_data_size,
                                                 bool is_key_frame) {
  // If you hit this CHECK you likely have a bug in a demuxer. Go fix it.
  CHECK(data);
  return make_scoped_refptr(new MediaSample(
      data, data_size, side_data, side_data_size, is_key_frame));
}

// static
scoped_refptr<MediaSample> MediaSample::CreateEOSBuffer() {
  return make_scoped_refptr(new MediaSample(NULL, 0, NULL, 0, false));
}

std::string MediaSample::ToString() const {
  if (end_of_stream()) {
    return "end of stream";
  }

  std::ostringstream s;
  s << "dts: " << dts_
    << " pts: " << pts_
    << " duration: " << duration_
    << " is_key_frame: " << is_key_frame_
    << " size: " << data_.size()
    << " side_data_size: " << side_data_.size()
    << " encrypted: " << (decrypt_config_ != NULL);
  return s.str();
}

}  // namespace media
