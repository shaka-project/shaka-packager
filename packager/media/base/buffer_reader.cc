// Copyright 2014 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/media/base/buffer_reader.h>

#include <absl/log/check.h>
#include <absl/log/log.h>

namespace shaka {
namespace media {

bool BufferReader::Read1(uint8_t* v) {
  DCHECK(v != NULL);
  if (!HasBytes(1))
    return false;
  *v = buf_[pos_++];
  return true;
}

bool BufferReader::Read2(uint16_t* v) {
  return Read(v);
}
bool BufferReader::Read2s(int16_t* v) {
  return Read(v);
}
bool BufferReader::Read4(uint32_t* v) {
  return Read(v);
}
bool BufferReader::Read4s(int32_t* v) {
  return Read(v);
}
bool BufferReader::Read8(uint64_t* v) {
  return Read(v);
}
bool BufferReader::Read8s(int64_t* v) {
  return Read(v);
}
bool BufferReader::ReadNBytesInto8(uint64_t* v, size_t num_bytes) {
  return ReadNBytes(v, num_bytes);
}
bool BufferReader::ReadNBytesInto8s(int64_t* v, size_t num_bytes) {
  return ReadNBytes(v, num_bytes);
}

bool BufferReader::ReadToVector(std::vector<uint8_t>* vec, size_t count) {
  DCHECK(vec != NULL);
  if (!HasBytes(count))
    return false;
  vec->assign(buf_ + pos_, buf_ + pos_ + count);
  pos_ += count;
  return true;
}

bool BufferReader::ReadToString(std::string* str, size_t size) {
  DCHECK(str);
  if (!HasBytes(size))
    return false;
  str->assign(buf_ + pos_, buf_ + pos_ + size);
  pos_ += size;
  return true;
}

bool BufferReader::ReadCString(std::string* str) {
  DCHECK(str);
  for (size_t count = 0; pos_ + count < size_; count++) {
    if (buf_[pos_ + count] == 0) {
      str->assign(buf_ + pos_, buf_ + pos_ + count);
      pos_ += count + 1;
      return true;
    }
  }
  return false;  // EOF
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
  // Perform sign extension by casting the byte value to int8_t, which will be
  // sign extended automatically when it is implicitly converted to T.
  T tmp = sign_extension_required ? static_cast<int8_t>(buf_[pos_++])
                                  : buf_[pos_++];
  for (size_t i = 1; i < num_bytes; ++i) {
    tmp <<= 8;
    tmp |= buf_[pos_++];
  }
  *v = tmp;
  return true;
}

}  // namespace media
}  // namespace shaka
