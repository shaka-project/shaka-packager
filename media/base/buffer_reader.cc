// Copyright (c) 2013 Google Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/buffer_reader.h"

#include "base/logging.h"

namespace media {

bool BufferReader::Read1(uint8* v) {
  DCHECK(v != NULL);
  if (!HasBytes(1))
    return false;
  *v = buf_[pos_++];
  return true;
}

bool BufferReader::Read2(uint16* v) { return Read(v); }
bool BufferReader::Read2s(int16* v) { return Read(v); }
bool BufferReader::Read4(uint32* v) { return Read(v); }
bool BufferReader::Read4s(int32* v) { return Read(v); }
bool BufferReader::Read8(uint64* v) { return Read(v); }
bool BufferReader::Read8s(int64* v) { return Read(v); }
bool BufferReader::ReadNBytesInto8(uint64* v, size_t num_bytes) {
  return ReadNBytes(v, num_bytes);
}
bool BufferReader::ReadNBytesInto8s(int64* v, size_t num_bytes) {
  return ReadNBytes(v, num_bytes);
}

bool BufferReader::ReadToVector(std::vector<uint8>* vec, size_t count) {
  DCHECK(vec != NULL);
  if (!HasBytes(count))
    return false;
  vec->assign(buf_ + pos_, buf_ + pos_ + count);
  pos_ += count;
  return true;
}

bool BufferReader::SkipBytes(size_t num_bytes) {
  if (!HasBytes(num_bytes))
    return false;
  pos_ += num_bytes;
  return true;
}

template <typename T>
bool BufferReader::Read(T* v) {
  return ReadNBytes(v, sizeof(*v));
}

template <typename T>
bool BufferReader::ReadNBytes(T* v, size_t num_bytes) {
  DCHECK(v != NULL);
  DCHECK_LE(num_bytes, sizeof(*v));
  if (!HasBytes(num_bytes))
    return false;

  // Sign extension is required only if
  //     |num_bytes| is less than size of T, and
  //     T is a signed type.
  const bool sign_extension_required =
      num_bytes < sizeof(*v) && static_cast<T>(-1) < 0;
  // Perform sign extension by casting the byte value to int8, which will be
  // sign extended automatically when it is implicitly converted to T.
  T tmp =
      sign_extension_required ? static_cast<int8>(buf_[pos_++]) : buf_[pos_++];
  for (size_t i = 1; i < num_bytes; ++i) {
    tmp <<= 8;
    tmp |= buf_[pos_++];
  }
  *v = tmp;
  return true;
}

}  // namespace media
