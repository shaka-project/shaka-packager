// Copyright 2014 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_FORMATS_MP4_BOX_BUFFER_H_
#define PACKAGER_MEDIA_FORMATS_MP4_BOX_BUFFER_H_

#include <string>

#include <absl/log/check.h>

#include <packager/macros/classes.h>
#include <packager/media/base/buffer_writer.h>
#include <packager/media/formats/mp4/box.h>
#include <packager/media/formats/mp4/box_reader.h>

namespace shaka {
namespace media {
namespace mp4 {

/// Class for MP4 box I/O. Box I/O is symmetric and exclusive, so we can define
/// a single method to do either reading or writing box objects.
/// BoxBuffer wraps either BoxReader for reading or BufferWriter for writing.
/// Thus it is capable of doing either reading or writing, but not both.
class BoxBuffer {
 public:
  /// Create a reader version of the BoxBuffer.
  /// @param reader should not be NULL.
  explicit BoxBuffer(BoxReader* reader) : reader_(reader), writer_(NULL) {
    DCHECK(reader);
  }
  /// Create a writer version of the BoxBuffer.
  /// @param writer should not be NULL.
  explicit BoxBuffer(BufferWriter* writer) : reader_(NULL), writer_(writer) {
    DCHECK(writer);
  }
  ~BoxBuffer() {}

  /// @return true for reader, false for writer.
  bool Reading() const { return reader_ != NULL; }

  /// @return Current read/write position. In read mode, this is the current
  ///         read position. In write mode, it is the same as Size().
  size_t Pos() const {
    if (reader_)
      return reader_->pos();
    return writer_->Size();
  }

  /// @return Total buffer size. In read mode, it includes data that has already
  ///         been read or skipped, and will not change. In write mode, it
  ///         includes all data that has been written, and will change as more
  ///         data is written.
  size_t Size() const {
    if (reader_)
      return reader_->size();
    return writer_->Size();
  }

  /// @return In read mode, return the number of bytes left in the box.
  ///         In write mode, return 0.
  size_t BytesLeft() const {
    if (reader_)
      return reader_->size() - reader_->pos();
    return 0;
  }

  /// @name Read/write integers of various sizes and signedness.
  /// @{
  bool ReadWriteUInt8(uint8_t* v) {
    if (reader_)
      return reader_->Read1(v);
    writer_->AppendInt(*v);
    return true;
  }
  bool ReadWriteUInt16(uint16_t* v) {
    if (reader_)
      return reader_->Read2(v);
    writer_->AppendInt(*v);
    return true;
  }
  bool ReadWriteUInt32(uint32_t* v) {
    if (reader_)
      return reader_->Read4(v);
    writer_->AppendInt(*v);
    return true;
  }
  bool ReadWriteUInt64(uint64_t* v) {
    if (reader_)
      return reader_->Read8(v);
    writer_->AppendInt(*v);
    return true;
  }
  bool ReadWriteInt16(int16_t* v) {
    if (reader_)
      return reader_->Read2s(v);
    writer_->AppendInt(*v);
    return true;
  }
  bool ReadWriteInt32(int32_t* v) {
    if (reader_)
      return reader_->Read4s(v);
    writer_->AppendInt(*v);
    return true;
  }
  bool ReadWriteInt64(int64_t* v) {
    if (reader_)
      return reader_->Read8s(v);
    writer_->AppendInt(*v);
    return true;
  }
  /// @}

  /// Read/write the least significant |num_bytes| of |v| from/to the buffer.
  /// @param num_bytes should not be larger than sizeof(v), i.e. 8.
  /// @return true on success, false otherwise.
  bool ReadWriteUInt64NBytes(uint64_t* v, size_t num_bytes) {
    if (reader_)
      return reader_->ReadNBytesInto8(v, num_bytes);
    writer_->AppendNBytes(*v, num_bytes);
    return true;
  }
  bool ReadWriteInt64NBytes(int64_t* v, size_t num_bytes) {
    if (reader_)
      return reader_->ReadNBytesInto8s(v, num_bytes);
    writer_->AppendNBytes(*v, num_bytes);
    return true;
  }
  bool ReadWriteVector(std::vector<uint8_t>* vector, size_t count) {
    if (reader_)
      return reader_->ReadToVector(vector, count);
    DCHECK_EQ(vector->size(), count);
    writer_->AppendArray(vector->data(), count);
    return true;
  }

  /// Reads @a size characters from the buffer and sets it to str.
  /// Writes @a str to the buffer. Write mode ignores @a size.
  bool ReadWriteString(std::string* str, size_t size) {
    if (reader_)
      return reader_->ReadToString(str, size);
    DCHECK_EQ(str->size(), size);
    writer_->AppendArray(reinterpret_cast<const uint8_t*>(str->data()),
                         str->size());
    return true;
  }

  bool ReadWriteCString(std::string* str) {
    if (reader_)
      return reader_->ReadCString(str);
    // Cannot contain embedded nulls.
    DCHECK_EQ(str->find('\0'), std::string::npos);
    writer_->AppendString(*str);
    writer_->AppendInt(static_cast<uint8_t>('\0'));
    return true;
  }

  bool ReadWriteFourCC(FourCC* fourcc) {
    if (reader_)
      return reader_->ReadFourCC(fourcc);
    writer_->AppendInt(static_cast<uint32_t>(*fourcc));
    return true;
  }

  /// Prepare child boxes for reading/writing.
  /// @return true on success, false otherwise.
  bool PrepareChildren() {
    if (reader_)
      return reader_->ScanChildren();
    // NOP in write mode.
    return true;
  }

  /// Read/write child box.
  /// @return true on success, false otherwise.
  bool ReadWriteChild(Box* box) {
    if (reader_)
      return reader_->ReadChild(box);
    // The box is mandatory, i.e. the box size should not be 0.
    DCHECK_NE(0u, box->box_size());
    CHECK(box->ReadWriteInternal(this));
    return true;
  }

  /// Read/write child box if exists.
  /// @return true on success, false otherwise.
  bool TryReadWriteChild(Box* box) {
    if (reader_)
      return reader_->TryReadChild(box);
    // The box is optional, i.e. it can be skipped if the box size is 0.
    if (box->box_size() != 0)
      CHECK(box->ReadWriteInternal(this));
    return true;
  }

  /// @param num_bytes specifies number of bytes to skip in read mode or number
  ///        of bytes to be padded with zero in write mode.
  /// @return true on success, false otherwise.
  bool IgnoreBytes(size_t num_bytes) {
    if (reader_)
      return reader_->SkipBytes(num_bytes);
    std::vector<uint8_t> vector(num_bytes, 0);
    writer_->AppendVector(vector);
    return true;
  }

  /// @return A pointer to the inner reader object.
  BoxReader* reader() { return reader_; }
  /// @return A pointer to the inner writer object.
  BufferWriter* writer() { return writer_; }

 private:
  BoxReader* reader_;
  BufferWriter* writer_;

  DISALLOW_COPY_AND_ASSIGN(BoxBuffer);
};

}  // namespace mp4
}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_FORMATS_MP4_BOX_BUFFER_H_
