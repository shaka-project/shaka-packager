// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_FORMATS_MP4_BOX_H_
#define PACKAGER_MEDIA_FORMATS_MP4_BOX_H_

#include <stdint.h>

#include "packager/media/base/fourccs.h"

namespace shaka {
namespace media {

class BufferWriter;

namespace mp4 {

class BoxBuffer;
class BoxReader;

/// Defines the base ISO BMFF box objects as defined in ISO 14496-12:2012
/// ISO BMFF section 4.2. All ISO BMFF compatible boxes inherit from either
/// Box or FullBox.
struct Box {
 public:
  Box();
  virtual ~Box();
  /// Parse the mp4 box.
  /// @param reader points to a BoxReader object which parses the box.
  bool Parse(BoxReader* reader);
  /// Write the box to buffer. This function calls ComputeSize internally to
  /// compute and update box size.
  /// @param writer points to a BufferWriter object which wraps the buffer for
  ///        writing.
  void Write(BufferWriter* writer);
  /// Write the box header to buffer. This function calls ComputeSize internally
  /// to compute and update box size.
  /// @param writer points to a BufferWriter object which wraps the buffer for
  ///        writing.
  void WriteHeader(BufferWriter* writer);
  /// Compute the size of this box. It will also update box size.
  /// @return The size of result box including child boxes. A value of 0 should
  ///         be returned if the box should not be written.
  uint32_t ComputeSize();
  /// @return box header size in bytes.
  virtual uint32_t HeaderSize() const;
  /// @return box type.
  virtual FourCC BoxType() const = 0;

  /// @return The size of result box including child boxes. Note that this
  //          function expects that ComputeSize has been invoked already.
  uint32_t box_size() { return box_size_; }

 protected:
  /// Read/write mp4 box header. Note that this function expects that
  /// ComputeSize has been invoked already.
  /// @return true on success, false otherwise.
  virtual bool ReadWriteHeaderInternal(BoxBuffer* buffer);

 private:
  friend class BoxBuffer;
  // Read/write the mp4 box from/to BoxBuffer. Note that this function expects
  // that ComputeSize has been invoked already.
  virtual bool ReadWriteInternal(BoxBuffer* buffer) = 0;
  // Compute the size of this box. A value of 0 should be returned if the box
  // should not be written. Note that this function won't update box size.
  virtual size_t ComputeSizeInternal() = 0;

  // We don't support 64-bit box sizes. 32-bit should be large enough for our
  // current needs.
  uint32_t box_size_;

  // Not using DISALLOW_COPY_AND_ASSIGN here intentionally to allow the compiler
  // generated copy constructor and assignment operator.
};

/// Defines FullBox, the other base ISO BMFF box objects as defined in
/// ISO 14496-12:2012 ISO BMFF section 4.2. All ISO BMFF compatible boxes
/// inherit from either Box or FullBox.
struct FullBox : Box {
 public:
  FullBox();
  ~FullBox() override;

  uint32_t HeaderSize() const final;

  uint8_t version = 0;
  uint32_t flags = 0;

 protected:
  bool ReadWriteHeaderInternal(BoxBuffer* buffer) final;

  // Not using DISALLOW_COPY_AND_ASSIGN here intentionally to allow the compiler
  // generated copy constructor and assignment operator.
};

}  // namespace mp4
}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_FORMATS_MP4_BOX_H_
