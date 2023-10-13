// Copyright 2014 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/media/formats/mp4/box.h>

#include <absl/log/check.h>
#include <absl/log/log.h>

#include <packager/media/formats/mp4/box_buffer.h>

namespace shaka {
namespace media {
namespace mp4 {

Box::Box() : box_size_(0) {}
Box::~Box() {}

bool Box::Parse(BoxReader* reader) {
  DCHECK(reader);
  BoxBuffer buffer(reader);
  return ReadWriteInternal(&buffer);
}

void Box::Write(BufferWriter* writer) {
  DCHECK(writer);
  // Compute and update box size.
  uint32_t size = ComputeSize();
  DCHECK_EQ(size, box_size_);

  size_t buffer_size_before_write = writer->Size();
  BoxBuffer buffer(writer);
  CHECK(ReadWriteInternal(&buffer));
  DCHECK_EQ(box_size_, writer->Size() - buffer_size_before_write)
      << FourCCToString(BoxType());
}

void Box::WriteHeader(BufferWriter* writer) {
  DCHECK(writer);
  // Compute and update box size.
  uint32_t size = ComputeSize();
  DCHECK_EQ(size, box_size_);

  size_t buffer_size_before_write = writer->Size();
  BoxBuffer buffer(writer);
  CHECK(ReadWriteHeaderInternal(&buffer));
  DCHECK_EQ(HeaderSize(), writer->Size() - buffer_size_before_write);
}

uint32_t Box::ComputeSize() {
  box_size_ = static_cast<uint32_t>(ComputeSizeInternal());
  return box_size_;
}

uint32_t Box::HeaderSize() const {
  const uint32_t kFourCCSize = 4;
  // We don't support 64-bit size.
  return kFourCCSize + sizeof(uint32_t);
}

bool Box::ReadWriteHeaderInternal(BoxBuffer* buffer) {
  if (buffer->Reading()) {
    // Skip for read mode, which is handled already in BoxReader.
  } else {
    CHECK(buffer->ReadWriteUInt32(&box_size_));
    FourCC fourcc = BoxType();
    CHECK(buffer->ReadWriteFourCC(&fourcc));
  }
  return true;
}

FullBox::FullBox() = default;
FullBox::~FullBox() = default;

uint32_t FullBox::HeaderSize() const {
  // Additional 1-byte version and 3-byte flags.
  return Box::HeaderSize() + 1 + 3;
}

bool FullBox::ReadWriteHeaderInternal(BoxBuffer* buffer) {
  RCHECK(Box::ReadWriteHeaderInternal(buffer));

  uint32_t vflags;
  if (buffer->Reading()) {
    RCHECK(buffer->ReadWriteUInt32(&vflags));
    this->version = vflags >> 24;
    this->flags = vflags & 0x00FFFFFF;
  } else {
    vflags = (this->version << 24) | this->flags;
    RCHECK(buffer->ReadWriteUInt32(&vflags));
  }
  return true;
}

}  // namespace mp4
}  // namespace media
}  // namespace shaka
