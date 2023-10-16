// Copyright 2014 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/media/base/buffer_writer.h>

#include <absl/base/internal/endian.h>
#include <absl/log/check.h>
#include <absl/log/log.h>

#include <packager/file.h>

namespace shaka {
namespace media {

BufferWriter::BufferWriter() {
  const size_t kDefaultReservedCapacity = 0x40000;  // 256KB.
  buf_.reserve(kDefaultReservedCapacity);
}
BufferWriter::BufferWriter(size_t reserved_size_in_bytes) {
  buf_.reserve(reserved_size_in_bytes);
}
BufferWriter::~BufferWriter() {}

void BufferWriter::AppendInt(uint8_t v) {
  buf_.push_back(v);
}
void BufferWriter::AppendInt(uint16_t v) {
  AppendInternal(absl::big_endian::FromHost16(v));
}
void BufferWriter::AppendInt(uint32_t v) {
  AppendInternal(absl::big_endian::FromHost32(v));
}
void BufferWriter::AppendInt(uint64_t v) {
  AppendInternal(absl::big_endian::FromHost64(v));
}
void BufferWriter::AppendInt(int16_t v) {
  AppendInternal(absl::big_endian::FromHost16(v));
}
void BufferWriter::AppendInt(int32_t v) {
  AppendInternal(absl::big_endian::FromHost32(v));
}
void BufferWriter::AppendInt(int64_t v) {
  AppendInternal(absl::big_endian::FromHost64(v));
}

void BufferWriter::AppendNBytes(uint64_t v, size_t num_bytes) {
  DCHECK_GE(sizeof(v), num_bytes);
  v = absl::big_endian::FromHost64(v);
  const uint8_t* data = reinterpret_cast<uint8_t*>(&v);
  AppendArray(&data[sizeof(v) - num_bytes], num_bytes);
}

void BufferWriter::AppendVector(const std::vector<uint8_t>& v) {
  buf_.insert(buf_.end(), v.begin(), v.end());
}

void BufferWriter::AppendString(const std::string& s) {
  buf_.insert(buf_.end(), s.begin(), s.end());
}

void BufferWriter::AppendArray(const uint8_t* buf, size_t size) {
  buf_.insert(buf_.end(), buf, buf + size);
}

void BufferWriter::AppendBuffer(const BufferWriter& buffer) {
  buf_.insert(buf_.end(), buffer.buf_.begin(), buffer.buf_.end());
}

Status BufferWriter::WriteToFile(File* file) {
  DCHECK(file);
  DCHECK(!buf_.empty());

  size_t remaining_size = buf_.size();
  const uint8_t* buf = &buf_[0];
  while (remaining_size > 0) {
    int64_t size_written = file->Write(buf, remaining_size);
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
  AppendArray(reinterpret_cast<uint8_t*>(&v), sizeof(T));
}

}  // namespace media
}  // namespace shaka
