// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "media/formats/mp4/box.h"

#include "base/logging.h"
#include "media/formats/mp4/box_buffer.h"

namespace media {
namespace mp4 {

Box::Box() : atom_size(0) {}
Box::~Box() {}

bool Box::Parse(BoxReader* reader) {
  DCHECK(reader != NULL);
  BoxBuffer buffer(reader);
  return ReadWrite(&buffer);
}

void Box::Write(BufferWriter* writer) {
  DCHECK(writer != NULL);
  uint32 size = ComputeSize();
  DCHECK_EQ(size, this->atom_size);

  size_t buffer_size_before_write = writer->Size();
  BoxBuffer buffer(writer);
  CHECK(ReadWrite(&buffer));
  DCHECK_EQ(this->atom_size, writer->Size() - buffer_size_before_write);
}

bool Box::ReadWrite(BoxBuffer* buffer) {
  if (buffer->Reading()) {
    // Skip for read mode, which is handled already in BoxReader.
  } else {
    CHECK(buffer->ReadWriteUInt32(&this->atom_size));
    FourCC fourcc = BoxType();
    CHECK(buffer->ReadWriteFourCC(&fourcc));
  }
  return true;
}

FullBox::FullBox() : version(0), flags(0) {}
FullBox::~FullBox() {}

bool FullBox::ReadWrite(BoxBuffer* buffer) {
  RCHECK(Box::ReadWrite(buffer));

  uint32 vflags;
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
