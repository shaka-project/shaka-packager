// Copyright (c) 2013 Google Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/buffer_writer.h"

#include "base/sys_byteorder.h"
#include "media/file/file.h"

namespace media {

BufferWriter::BufferWriter() {
  const size_t kDefaultReservedCapacity = 0x40000;  // 256KB.
  buf_.reserve(kDefaultReservedCapacity);
}
BufferWriter::BufferWriter(size_t reserved_size_in_bytes) {
  buf_.reserve(reserved_size_in_bytes);
}
BufferWriter::~BufferWriter() {}

void BufferWriter::AppendInt(uint8 v) { buf_.push_back(v); }
void BufferWriter::AppendInt(uint16 v) { AppendInternal(base::HostToNet16(v)); }
void BufferWriter::AppendInt(uint32 v) { AppendInternal(base::HostToNet32(v)); }
void BufferWriter::AppendInt(uint64 v) { AppendInternal(base::HostToNet64(v)); }
void BufferWriter::AppendInt(int16 v) { AppendInternal(base::HostToNet16(v)); }
void BufferWriter::AppendInt(int32 v) { AppendInternal(base::HostToNet32(v)); }
void BufferWriter::AppendInt(int64 v) { AppendInternal(base::HostToNet64(v)); }

void BufferWriter::AppendNBytes(uint64 v, size_t num_bytes) {
  DCHECK_GE(sizeof(v), num_bytes);
  v = base::HostToNet64(v);
  const uint8* data = reinterpret_cast<uint8*>(&v);
  AppendArray(&data[sizeof(v) - num_bytes], num_bytes);
}

void BufferWriter::AppendVector(const std::vector<uint8>& v) {
  buf_.insert(buf_.end(), v.begin(), v.end());
}

void BufferWriter::AppendArray(const uint8* buf, size_t size) {
  buf_.insert(buf_.end(), buf, buf + size);
}

void BufferWriter::AppendBuffer(const BufferWriter& buffer) {
  buf_.insert(buf_.end(), buffer.buf_.begin(), buffer.buf_.end());
}

Status BufferWriter::WriteToFile(File* file) {
  DCHECK(file);

  size_t remaining_size = buf_.size();
  const uint8* buf = &buf_[0];
  while (remaining_size > 0) {
    int64 size_written = file->Write(buf, remaining_size);
    if (size_written <= 0) {
      return Status(error::FILE_FAILURE,
                    "Fail to write to file in BufferWriter");
    }
    remaining_size -= size_written;
    buf += size_written;
  }
  buf_.clear();
  return Status::OK;
}

template <typename T>
void BufferWriter::AppendInternal(T v) {
  AppendArray(reinterpret_cast<uint8*>(&v), sizeof(T));
}

}  // namespace media
