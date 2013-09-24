// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/pickle.h"

#include <stdlib.h>

#include <algorithm>  // for max()

//------------------------------------------------------------------------------

// static
const int Pickle::kPayloadUnit = 64;

static const size_t kCapacityReadOnly = static_cast<size_t>(-1);

PickleIterator::PickleIterator(const Pickle& pickle)
    : read_ptr_(pickle.payload()),
      read_end_ptr_(pickle.end_of_payload()) {
}

template <typename Type>
inline bool PickleIterator::ReadBuiltinType(Type* result) {
  const char* read_from = GetReadPointerAndAdvance<Type>();
  if (!read_from)
    return false;
  if (sizeof(Type) > sizeof(uint32))
    memcpy(result, read_from, sizeof(*result));
  else
    *result = *reinterpret_cast<const Type*>(read_from);
  return true;
}

template<typename Type>
inline const char* PickleIterator::GetReadPointerAndAdvance() {
  const char* current_read_ptr = read_ptr_;
  if (read_ptr_ + sizeof(Type) > read_end_ptr_)
    return NULL;
  if (sizeof(Type) < sizeof(uint32))
    read_ptr_ += AlignInt(sizeof(Type), sizeof(uint32));
  else
    read_ptr_ += sizeof(Type);
  return current_read_ptr;
}

const char* PickleIterator::GetReadPointerAndAdvance(int num_bytes) {
  if (num_bytes < 0 || read_end_ptr_ - read_ptr_ < num_bytes)
    return NULL;
  const char* current_read_ptr = read_ptr_;
  read_ptr_ += AlignInt(num_bytes, sizeof(uint32));
  return current_read_ptr;
}

inline const char* PickleIterator::GetReadPointerAndAdvance(int num_elements,
                                                          size_t size_element) {
  // Check for int32 overflow.
  int64 num_bytes = static_cast<int64>(num_elements) * size_element;
  int num_bytes32 = static_cast<int>(num_bytes);
  if (num_bytes != static_cast<int64>(num_bytes32))
    return NULL;
  return GetReadPointerAndAdvance(num_bytes32);
}

bool PickleIterator::ReadBool(bool* result) {
  return ReadBuiltinType(result);
}

bool PickleIterator::ReadInt(int* result) {
  return ReadBuiltinType(result);
}

bool PickleIterator::ReadLong(long* result) {
  return ReadBuiltinType(result);
}

bool PickleIterator::ReadUInt16(uint16* result) {
  return ReadBuiltinType(result);
}

bool PickleIterator::ReadUInt32(uint32* result) {
  return ReadBuiltinType(result);
}

bool PickleIterator::ReadInt64(int64* result) {
  return ReadBuiltinType(result);
}

bool PickleIterator::ReadUInt64(uint64* result) {
  return ReadBuiltinType(result);
}

bool PickleIterator::ReadFloat(float* result) {
  return ReadBuiltinType(result);
}

bool PickleIterator::ReadString(std::string* result) {
  int len;
  if (!ReadInt(&len))
    return false;
  const char* read_from = GetReadPointerAndAdvance(len);
  if (!read_from)
    return false;

  result->assign(read_from, len);
  return true;
}

bool PickleIterator::ReadWString(std::wstring* result) {
  int len;
  if (!ReadInt(&len))
    return false;
  const char* read_from = GetReadPointerAndAdvance(len, sizeof(wchar_t));
  if (!read_from)
    return false;

  result->assign(reinterpret_cast<const wchar_t*>(read_from), len);
  return true;
}

bool PickleIterator::ReadString16(string16* result) {
  int len;
  if (!ReadInt(&len))
    return false;
  const char* read_from = GetReadPointerAndAdvance(len, sizeof(char16));
  if (!read_from)
    return false;

  result->assign(reinterpret_cast<const char16*>(read_from), len);
  return true;
}

bool PickleIterator::ReadData(const char** data, int* length) {
  *length = 0;
  *data = 0;

  if (!ReadInt(length))
    return false;

  return ReadBytes(data, *length);
}

bool PickleIterator::ReadBytes(const char** data, int length) {
  const char* read_from = GetReadPointerAndAdvance(length);
  if (!read_from)
    return false;
  *data = read_from;
  return true;
}

// Payload is uint32 aligned.

Pickle::Pickle()
    : header_(NULL),
      header_size_(sizeof(Header)),
      capacity_(0),
      variable_buffer_offset_(0) {
  Resize(kPayloadUnit);
  header_->payload_size = 0;
}

Pickle::Pickle(int header_size)
    : header_(NULL),
      header_size_(AlignInt(header_size, sizeof(uint32))),
      capacity_(0),
      variable_buffer_offset_(0) {
  DCHECK_GE(static_cast<size_t>(header_size), sizeof(Header));
  DCHECK_LE(header_size, kPayloadUnit);
  Resize(kPayloadUnit);
  header_->payload_size = 0;
}

Pickle::Pickle(const char* data, int data_len)
    : header_(reinterpret_cast<Header*>(const_cast<char*>(data))),
      header_size_(0),
      capacity_(kCapacityReadOnly),
      variable_buffer_offset_(0) {
  if (data_len >= static_cast<int>(sizeof(Header)))
    header_size_ = data_len - header_->payload_size;

  if (header_size_ > static_cast<unsigned int>(data_len))
    header_size_ = 0;

  if (header_size_ != AlignInt(header_size_, sizeof(uint32)))
    header_size_ = 0;

  // If there is anything wrong with the data, we're not going to use it.
  if (!header_size_)
    header_ = NULL;
}

Pickle::Pickle(const Pickle& other)
    : header_(NULL),
      header_size_(other.header_size_),
      capacity_(0),
      variable_buffer_offset_(other.variable_buffer_offset_) {
  size_t payload_size = header_size_ + other.header_->payload_size;
  bool resized = Resize(payload_size);
  CHECK(resized);  // Realloc failed.
  memcpy(header_, other.header_, payload_size);
}

Pickle::~Pickle() {
  if (capacity_ != kCapacityReadOnly)
    free(header_);
}

Pickle& Pickle::operator=(const Pickle& other) {
  if (this == &other) {
    NOTREACHED();
    return *this;
  }
  if (capacity_ == kCapacityReadOnly) {
    header_ = NULL;
    capacity_ = 0;
  }
  if (header_size_ != other.header_size_) {
    free(header_);
    header_ = NULL;
    header_size_ = other.header_size_;
  }
  bool resized = Resize(other.header_size_ + other.header_->payload_size);
  CHECK(resized);  // Realloc failed.
  memcpy(header_, other.header_,
         other.header_size_ + other.header_->payload_size);
  variable_buffer_offset_ = other.variable_buffer_offset_;
  return *this;
}

bool Pickle::WriteString(const std::string& value) {
  if (!WriteInt(static_cast<int>(value.size())))
    return false;

  return WriteBytes(value.data(), static_cast<int>(value.size()));
}

bool Pickle::WriteWString(const std::wstring& value) {
  if (!WriteInt(static_cast<int>(value.size())))
    return false;

  return WriteBytes(value.data(),
                    static_cast<int>(value.size() * sizeof(wchar_t)));
}

bool Pickle::WriteString16(const string16& value) {
  if (!WriteInt(static_cast<int>(value.size())))
    return false;

  return WriteBytes(value.data(),
                    static_cast<int>(value.size()) * sizeof(char16));
}

bool Pickle::WriteData(const char* data, int length) {
  return length >= 0 && WriteInt(length) && WriteBytes(data, length);
}

bool Pickle::WriteBytes(const void* data, int data_len) {
  DCHECK_NE(kCapacityReadOnly, capacity_) << "oops: pickle is readonly";

  char* dest = BeginWrite(data_len);
  if (!dest)
    return false;

  memcpy(dest, data, data_len);

  EndWrite(dest, data_len);
  return true;
}

char* Pickle::BeginWriteData(int length) {
  DCHECK_EQ(variable_buffer_offset_, 0U) <<
    "There can only be one variable buffer in a Pickle";

  if (length < 0 || !WriteInt(length))
    return NULL;

  char *data_ptr = BeginWrite(length);
  if (!data_ptr)
    return NULL;

  variable_buffer_offset_ =
      data_ptr - reinterpret_cast<char*>(header_) - sizeof(int);

  // EndWrite doesn't necessarily have to be called after the write operation,
  // so we call it here to pad out what the caller will eventually write.
  EndWrite(data_ptr, length);
  return data_ptr;
}

void Pickle::TrimWriteData(int new_length) {
  DCHECK_NE(variable_buffer_offset_, 0U);

  // Fetch the the variable buffer size
  int* cur_length = reinterpret_cast<int*>(
      reinterpret_cast<char*>(header_) + variable_buffer_offset_);

  if (new_length < 0 || new_length > *cur_length) {
    NOTREACHED() << "Invalid length in TrimWriteData.";
    return;
  }

  // Update the payload size and variable buffer size
  header_->payload_size -= (*cur_length - new_length);
  *cur_length = new_length;
}

char* Pickle::BeginWrite(size_t length) {
  // write at a uint32-aligned offset from the beginning of the header
  size_t offset = AlignInt(header_->payload_size, sizeof(uint32));

  size_t new_size = offset + length;
  size_t needed_size = header_size_ + new_size;
  if (needed_size > capacity_ && !Resize(std::max(capacity_ * 2, needed_size)))
    return NULL;

#ifdef ARCH_CPU_64_BITS
  DCHECK_LE(length, kuint32max);
#endif

  header_->payload_size = static_cast<uint32>(new_size);
  return mutable_payload() + offset;
}

void Pickle::EndWrite(char* dest, int length) {
  // Zero-pad to keep tools like valgrind from complaining about uninitialized
  // memory.
  if (length % sizeof(uint32))
    memset(dest + length, 0, sizeof(uint32) - (length % sizeof(uint32)));
}

bool Pickle::Resize(size_t new_capacity) {
  new_capacity = AlignInt(new_capacity, kPayloadUnit);

  CHECK_NE(capacity_, kCapacityReadOnly);
  void* p = realloc(header_, new_capacity);
  if (!p)
    return false;

  header_ = reinterpret_cast<Header*>(p);
  capacity_ = new_capacity;
  return true;
}

// static
const char* Pickle::FindNext(size_t header_size,
                             const char* start,
                             const char* end) {
  DCHECK_EQ(header_size, AlignInt(header_size, sizeof(uint32)));
  DCHECK_LE(header_size, static_cast<size_t>(kPayloadUnit));

  if (static_cast<size_t>(end - start) < sizeof(Header))
    return NULL;

  const Header* hdr = reinterpret_cast<const Header*>(start);
  const char* payload_base = start + header_size;
  const char* payload_end = payload_base + hdr->payload_size;
  if (payload_end < payload_base)
    return NULL;

  return (payload_end > end) ? NULL : payload_end;
}
