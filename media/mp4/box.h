// Copyright (c) 2013 Google Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MP4_BOX_H_
#define MEDIA_MP4_BOX_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "media/mp4/fourccs.h"

namespace media {

class BufferWriter;

namespace mp4 {

class BoxBuffer;
class BoxReader;

// Defines Box and FullBox, the two base ISO BMFF box objects as defined in
// ISO 14496-12:2012 ISO BMFF section 4.2. All ISO BMFF compatible boxes
// inherits either Box or FullBox.
struct Box {
 public:
  Box();
  virtual ~Box();
  bool Parse(BoxReader* reader);
  // Write the box to buffer.
  // The function calls ComputeSize internally to compute box size.
  void Write(BufferWriter* writer);
  // Computer box size.
  // The calculated size will be saved in |atom_size| for consumption later.
  virtual uint32 ComputeSize() = 0;
  virtual FourCC BoxType() const = 0;

 protected:
  friend class BoxBuffer;
  // Read or write the mp4 box through BoxBuffer.
  virtual bool ReadWrite(BoxBuffer* buffer);

  // We don't support 64-bit atom size. 32-bit should be large enough for our
  // current needs.
  uint32 atom_size;
};

struct FullBox : Box {
 public:
  FullBox();
  virtual ~FullBox();

  uint8 version;
  uint32 flags;

 protected:
  virtual bool ReadWrite(BoxBuffer* buffer) OVERRIDE;
};

}  // namespace mp4
}  // namespace media

#endif  // MEDIA_MP4_BOX_H_
