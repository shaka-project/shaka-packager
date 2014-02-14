// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef MEDIA_MP4_BOX_BUFFER_H_
#define MEDIA_MP4_BOX_BUFFER_H_

#include "base/compiler_specific.h"
#include "media/base/buffer_writer.h"
#include "media/mp4/box.h"
#include "media/mp4/box_reader.h"

namespace media {
namespace mp4 {

// Defines a wrapper for mp4 box reading/writing, which is symmetric in most
// cases, i.e. we can use one single routine for the reading and writing.
// BoxBuffer wraps either BoxReader for reading or BufferWriter for writing.
// Thus it is capable of doing either reading or writing, but not both.
class BoxBuffer {
 public:
  // Creates a "reader" version of the BoxBuffer.
  // Caller retains |reader| ownership. |reader| should not be NULL.
  explicit BoxBuffer(BoxReader* reader) : reader_(reader), writer_(NULL) {
    DCHECK(reader);
  }
  // Creates a "writer" version of the BoxBuffer.
  // Caller retains |writer| ownership. |writer| should not be NULL.
  explicit BoxBuffer(BufferWriter* writer) : reader_(NULL), writer_(writer) {
    DCHECK(writer);
  }
  ~BoxBuffer() {}

  // Reading or writing?
  bool Reading() const { return reader_ != NULL; }

  // Returns current read/write position. In read mode, this is the current
  // read position. In write mode, it is the same as Size().
  size_t Pos() const {
    if (reader_)
      return reader_->pos();
    return writer_->Size();
  }

  // Returns total buffer size.In read mode, it includes data that has already
  // been read or skipped, and will not change. In write mode, it includes all
  // data that has been written, and will change as data is written.
  size_t Size() const {
    if (reader_)
      return reader_->size();
    return writer_->Size();
  }

  // Read/write integers of various size and unsigned/signed.
  bool ReadWriteUInt8(uint8* v) {
    if (reader_)
      return reader_->Read1(v);
    writer_->AppendInt(*v);
    return true;
  }
  bool ReadWriteUInt16(uint16* v) {
    if (reader_)
      return reader_->Read2(v);
    writer_->AppendInt(*v);
    return true;
  }
  bool ReadWriteUInt32(uint32* v) {
    if (reader_)
      return reader_->Read4(v);
    writer_->AppendInt(*v);
    return true;
  }
  bool ReadWriteUInt64(uint64* v) {
    if (reader_)
      return reader_->Read8(v);
    writer_->AppendInt(*v);
    return true;
  }
  bool ReadWriteInt16(int16* v) {
    if (reader_)
      return reader_->Read2s(v);
    writer_->AppendInt(*v);
    return true;
  }
  bool ReadWriteInt32(int32* v) {
    if (reader_)
      return reader_->Read4s(v);
    writer_->AppendInt(*v);
    return true;
  }
  bool ReadWriteInt64(int64* v) {
    if (reader_)
      return reader_->Read8s(v);
    writer_->AppendInt(*v);
    return true;
  }

  // Read/write the least significant |num_bytes| of |v| from/to buffer.
  // |num_bytes| should not be larger than sizeof(v), i.e. 8.
  bool ReadWriteUInt64NBytes(uint64* v, size_t num_bytes) {
    if (reader_)
      return reader_->ReadNBytesInto8(v, num_bytes);
    writer_->AppendNBytes(*v, num_bytes);
    return true;
  }
  bool ReadWriteInt64NBytes(int64* v, size_t num_bytes) {
    if (reader_)
      return reader_->ReadNBytesInto8s(v, num_bytes);
    writer_->AppendNBytes(*v, num_bytes);
    return true;
  }
  bool ReadWriteVector(std::vector<uint8>* vector, size_t count) {
    if (reader_)
      return reader_->ReadToVector(vector, count);
    DCHECK_EQ(vector->size(), count);
    writer_->AppendVector(*vector);
    return true;
  }
  bool ReadWriteFourCC(FourCC* fourcc) {
    if (reader_)
      return reader_->ReadFourCC(fourcc);
    writer_->AppendInt(static_cast<uint32>(*fourcc));
    return true;
  }

  // Prepare child boxes for read/write.
  bool PrepareChildren() {
    if (reader_)
      return reader_->ScanChildren();
    // NOP in write mode.
    return true;
  }

  // Read/write child box.
  bool ReadWriteChild(Box* box) {
    if (reader_)
      return reader_->ReadChild(box);
    // The box is mandatory, i.e. the box size should not be 0.
    DCHECK_NE(0, box->atom_size);
    CHECK(box->ReadWrite(this));
    return true;
  }

  // Read/write child box if exist.
  bool TryReadWriteChild(Box* box) {
    if (reader_)
      return reader_->TryReadChild(box);
    // The box is optional, i.e. it can be skipped if the box size is 0.
    if (box->atom_size != 0)
      CHECK(box->ReadWrite(this));
    return true;
  }

  // Skip |num_bytes| in read mode, otherwise fill with |num_bytes| of '\0'.
  bool IgnoreBytes(size_t num_bytes) {
    if (reader_)
      return reader_->SkipBytes(num_bytes);
    std::vector<uint8> vector(num_bytes, 0);
    writer_->AppendVector(vector);
    return true;
  }

  BoxReader* reader() { return reader_; }
  BufferWriter* writer() { return writer_; }

 private:
  BoxReader* reader_;
  BufferWriter* writer_;

  DISALLOW_COPY_AND_ASSIGN(BoxBuffer);
};

}  // namespace mp4
}  // namespace media

#endif  // MEDIA_MP4_BOX_BUFFER_H_
