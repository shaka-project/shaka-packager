// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/mp4/cenc.h"

#include <cstring>

#include "media/base/buffer_reader.h"
#include "media/base/buffer_writer.h"
#include "media/formats/mp4/rcheck.h"

namespace {
// According to ISO/IEC FDIS 23001-7: CENC spec, IV should be either
// 64-bit (8-byte) or 128-bit (16-byte).
bool IsIvSizeValid(size_t iv_size) { return iv_size == 8 || iv_size == 16; }

// 16-bit |clear_bytes| and 32-bit |cypher_bytes|.
const size_t kSubsampleEntrySize = sizeof(uint16) + sizeof(uint32);
}  // namespace

namespace media {
namespace mp4 {

FrameCENCInfo::FrameCENCInfo() {}
FrameCENCInfo::FrameCENCInfo(const std::vector<uint8>& iv) : iv_(iv) {}
FrameCENCInfo::~FrameCENCInfo() {}

bool FrameCENCInfo::Parse(uint8 iv_size, BufferReader* reader) {
  DCHECK(reader);
  // Mandated by CENC spec.
  RCHECK(IsIvSizeValid(iv_size));

  iv_.resize(iv_size);
  RCHECK(reader->ReadToVector(&iv_, iv_size));

  if (!reader->HasBytes(1))
    return true;

  uint16 subsample_count;
  RCHECK(reader->Read2(&subsample_count) &&
         reader->HasBytes(subsample_count * kSubsampleEntrySize));

  subsamples_.resize(subsample_count);
  for (uint16 i = 0; i < subsample_count; ++i) {
    uint16 clear_bytes;
    uint32 cypher_bytes;
    RCHECK(reader->Read2(&clear_bytes) &&
           reader->Read4(&cypher_bytes));
    subsamples_[i].clear_bytes = clear_bytes;
    subsamples_[i].cypher_bytes = cypher_bytes;
  }
  return true;
}

void FrameCENCInfo::Write(BufferWriter* writer) const {
  DCHECK(writer);
  DCHECK(IsIvSizeValid(iv_.size()));
  writer->AppendVector(iv_);

  uint16 subsample_count = subsamples_.size();
  if (subsample_count == 0)
    return;
  writer->AppendInt(subsample_count);

  for (uint16 i = 0; i < subsample_count; ++i) {
    writer->AppendInt(subsamples_[i].clear_bytes);
    writer->AppendInt(subsamples_[i].cypher_bytes);
  }
}

size_t FrameCENCInfo::ComputeSize() const {
  uint16 subsample_count = subsamples_.size();
  if (subsample_count == 0)
    return iv_.size();

  return iv_.size() + sizeof(subsample_count) +
         subsample_count * kSubsampleEntrySize;
}

size_t FrameCENCInfo::GetTotalSizeOfSubsamples() const {
  size_t size = 0;
  for (size_t i = 0; i < subsamples_.size(); ++i) {
    size += subsamples_[i].clear_bytes + subsamples_[i].cypher_bytes;
  }
  return size;
}

}  // namespace mp4
}  // namespace media
