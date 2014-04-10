// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/mp4/box_definitions.h"

#include "base/logging.h"
#include "media/base/bit_reader.h"
#include "media/formats/mp4/box_buffer.h"
#include "media/formats/mp4/rcheck.h"

namespace {
const uint32 kFourCCSize = 4;
// Additional 32-bit size. We don't support 64-bit size.
const uint32 kBoxSize = kFourCCSize + sizeof(uint32);
// Additional 1-byte version and 3-byte flags.
const uint32 kFullBoxSize = kBoxSize + 4;

// 9 uint32 in big endian formatted array.
const uint8 kUnityMatrix[] = {0, 1, 0, 0, 0, 0, 0, 0, 0,    0, 0, 0,
                              0, 0, 0, 0, 0, 1, 0, 0, 0,    0, 0, 0,
                              0, 0, 0, 0, 0, 0, 0, 0, 0x40, 0, 0, 0};

// Default entries for HandlerReference box.
const char kVideoHandlerName[] = "VideoHandler";
const char kAudioHandlerName[] = "SoundHandler";

// Default values for VideoSampleEntry box.
const uint32 kVideoResolution = 0x00480000;  // 72 dpi.
const uint16 kVideoFrameCount = 1;
const uint16 kVideoDepth = 0x0018;

bool IsFitIn32Bits(uint64 a) { return a <= kuint32max; }
bool IsFitIn32Bits(int64 a) { return a <= kint32max && a >= kint32min; }
bool IsFitIn32Bits(uint64 a, uint64 b) {
  return IsFitIn32Bits(a) && IsFitIn32Bits(b);
}
bool IsFitIn32Bits(uint64 a, int64 b) {
  return IsFitIn32Bits(a) && IsFitIn32Bits(b);
}
bool IsFitIn32Bits(uint64 a, uint64 b, uint64 c) {
  return IsFitIn32Bits(a) && IsFitIn32Bits(b) && IsFitIn32Bits(c);
}

}  // namespace

namespace media {
namespace mp4 {

FileType::FileType() : major_brand(FOURCC_NULL), minor_version(0) {}
FileType::~FileType() {}
FourCC FileType::BoxType() const { return FOURCC_FTYP; }

bool FileType::ReadWrite(BoxBuffer* buffer) {
  RCHECK(Box::ReadWrite(buffer) &&
         buffer->ReadWriteFourCC(&major_brand) &&
         buffer->ReadWriteUInt32(&minor_version));
  size_t num_brands;
  if (buffer->Reading()) {
    num_brands = (buffer->Size() - buffer->Pos()) / sizeof(FourCC);
    compatible_brands.resize(num_brands);
  } else {
    num_brands = compatible_brands.size();
  }
  for (size_t i = 0; i < num_brands; ++i)
    RCHECK(buffer->ReadWriteFourCC(&compatible_brands[i]));
  return true;
}

uint32 FileType::ComputeSize() {
  atom_size = kBoxSize + kFourCCSize + sizeof(minor_version) +
              kFourCCSize * compatible_brands.size();
  return atom_size;
}

SegmentType::SegmentType() {}
SegmentType::~SegmentType() {}
FourCC SegmentType::BoxType() const { return FOURCC_STYP; }

bool SegmentType::ReadWrite(BoxBuffer* buffer) {
  return FileType::ReadWrite(buffer);
}

uint32 SegmentType::ComputeSize() {
  return FileType::ComputeSize();
}

ProtectionSystemSpecificHeader::ProtectionSystemSpecificHeader() {}
ProtectionSystemSpecificHeader::~ProtectionSystemSpecificHeader() {}
FourCC ProtectionSystemSpecificHeader::BoxType() const { return FOURCC_PSSH; }

bool ProtectionSystemSpecificHeader::ReadWrite(BoxBuffer* buffer) {
  uint32 size = data.size();
  RCHECK(FullBox::ReadWrite(buffer) &&
         buffer->ReadWriteVector(&system_id, 16) &&
         buffer->ReadWriteUInt32(&size) &&
         buffer->ReadWriteVector(&data, size));

  if (buffer->Reading()) {
    // Copy the entire box, including the header, for passing to EME as
    // initData.
    DCHECK(raw_box.empty());
    BoxReader* reader = buffer->reader();
    DCHECK(reader);
    raw_box.assign(reader->data(), reader->data() + reader->size());
  }
  return true;
}

uint32 ProtectionSystemSpecificHeader::ComputeSize() {
  atom_size = kFullBoxSize + system_id.size() + sizeof(uint32) + data.size();
  return atom_size;
}

SampleAuxiliaryInformationOffset::SampleAuxiliaryInformationOffset() {}
SampleAuxiliaryInformationOffset::~SampleAuxiliaryInformationOffset() {}
FourCC SampleAuxiliaryInformationOffset::BoxType() const { return FOURCC_SAIO; }

bool SampleAuxiliaryInformationOffset::ReadWrite(BoxBuffer* buffer) {
  RCHECK(FullBox::ReadWrite(buffer));
  if (flags & 1)
    RCHECK(buffer->IgnoreBytes(8));  // aux_info_type and parameter.

  uint32 count = offsets.size();
  RCHECK(buffer->ReadWriteUInt32(&count));
  offsets.resize(count);

  size_t num_bytes = (version == 1) ? sizeof(uint64) : sizeof(uint32);
  for (uint32 i = 0; i < count; ++i)
    RCHECK(buffer->ReadWriteUInt64NBytes(&offsets[i], num_bytes));
  return true;
}

uint32 SampleAuxiliaryInformationOffset::ComputeSize() {
  // This box is optional. Skip it if it is empty.
  atom_size = 0;
  if (offsets.size() != 0) {
    size_t num_bytes = (version == 1) ? sizeof(uint64) : sizeof(uint32);
    atom_size = kFullBoxSize + sizeof(uint32) + num_bytes * offsets.size();
  }
  return atom_size;
}

SampleAuxiliaryInformationSize::SampleAuxiliaryInformationSize()
    : default_sample_info_size(0), sample_count(0) {}
SampleAuxiliaryInformationSize::~SampleAuxiliaryInformationSize() {}
FourCC SampleAuxiliaryInformationSize::BoxType() const { return FOURCC_SAIZ; }

bool SampleAuxiliaryInformationSize::ReadWrite(BoxBuffer* buffer) {
  RCHECK(FullBox::ReadWrite(buffer));
  if (flags & 1)
    RCHECK(buffer->IgnoreBytes(8));

  RCHECK(buffer->ReadWriteUInt8(&default_sample_info_size) &&
         buffer->ReadWriteUInt32(&sample_count));
  if (default_sample_info_size == 0)
    RCHECK(buffer->ReadWriteVector(&sample_info_sizes, sample_count));
  return true;
}

uint32 SampleAuxiliaryInformationSize::ComputeSize() {
  // This box is optional. Skip it if it is empty.
  atom_size = 0;
  if (sample_count != 0) {
    atom_size = kFullBoxSize + sizeof(default_sample_info_size) +
                sizeof(sample_count) +
                (default_sample_info_size == 0 ? sample_info_sizes.size() : 0);
  }
  return atom_size;
}

OriginalFormat::OriginalFormat() : format(FOURCC_NULL) {}
OriginalFormat::~OriginalFormat() {}
FourCC OriginalFormat::BoxType() const { return FOURCC_FRMA; }

bool OriginalFormat::ReadWrite(BoxBuffer* buffer) {
  return Box::ReadWrite(buffer) && buffer->ReadWriteFourCC(&format);
}

uint32 OriginalFormat::ComputeSize() {
  atom_size = kBoxSize + kFourCCSize;
  return atom_size;
}

SchemeType::SchemeType() : type(FOURCC_NULL), version(0) {}
SchemeType::~SchemeType() {}
FourCC SchemeType::BoxType() const { return FOURCC_SCHM; }

bool SchemeType::ReadWrite(BoxBuffer* buffer) {
  RCHECK(FullBox::ReadWrite(buffer) &&
         buffer->ReadWriteFourCC(&type) &&
         buffer->ReadWriteUInt32(&version));
  return true;
}

uint32 SchemeType::ComputeSize() {
  atom_size = kFullBoxSize + kFourCCSize + sizeof(version);
  return atom_size;
}

TrackEncryption::TrackEncryption()
    : is_encrypted(false), default_iv_size(0), default_kid(16, 0) {}
TrackEncryption::~TrackEncryption() {}
FourCC TrackEncryption::BoxType() const { return FOURCC_TENC; }

bool TrackEncryption::ReadWrite(BoxBuffer* buffer) {
  uint8 flag = is_encrypted ? 1 : 0;
  RCHECK(FullBox::ReadWrite(buffer) &&
         buffer->IgnoreBytes(2) &&  // reserved.
         buffer->ReadWriteUInt8(&flag) &&
         buffer->ReadWriteUInt8(&default_iv_size) &&
         buffer->ReadWriteVector(&default_kid, 16));
  if (buffer->Reading()) {
    is_encrypted = (flag != 0);
    if (is_encrypted) {
      RCHECK(default_iv_size == 8 || default_iv_size == 16);
    } else {
      RCHECK(default_iv_size == 0);
    }
  }
  return true;
}

uint32 TrackEncryption::ComputeSize() {
  atom_size = kFullBoxSize + sizeof(uint32) + default_kid.size();
  return atom_size;
}

SchemeInfo::SchemeInfo() {}
SchemeInfo::~SchemeInfo() {}
FourCC SchemeInfo::BoxType() const { return FOURCC_SCHI; }

bool SchemeInfo::ReadWrite(BoxBuffer* buffer) {
  RCHECK(Box::ReadWrite(buffer) && buffer->PrepareChildren() &&
         buffer->ReadWriteChild(&track_encryption));
  return true;
}

uint32 SchemeInfo::ComputeSize() {
  atom_size = kBoxSize + track_encryption.ComputeSize();
  return atom_size;
}

ProtectionSchemeInfo::ProtectionSchemeInfo() {}
ProtectionSchemeInfo::~ProtectionSchemeInfo() {}
FourCC ProtectionSchemeInfo::BoxType() const { return FOURCC_SINF; }

bool ProtectionSchemeInfo::ReadWrite(BoxBuffer* buffer) {
  RCHECK(Box::ReadWrite(buffer) &&
         buffer->PrepareChildren() &&
         buffer->ReadWriteChild(&format) &&
         buffer->ReadWriteChild(&type));
  if (type.type == FOURCC_CENC)
    RCHECK(buffer->ReadWriteChild(&info));
  // Other protection schemes are silently ignored. Since the protection scheme
  // type can't be determined until this box is opened, we return 'true' for
  // non-CENC protection scheme types. It is the parent box's responsibility to
  // ensure that this scheme type is a supported one.
  return true;
}

uint32 ProtectionSchemeInfo::ComputeSize() {
  // Skip sinf box if it is not initialized.
  atom_size = 0;
  if (format.format != FOURCC_NULL) {
    atom_size = kBoxSize + format.ComputeSize() + type.ComputeSize() +
                info.ComputeSize();
  }
  return atom_size;
}

MovieHeader::MovieHeader()
    : creation_time(0),
      modification_time(0),
      timescale(0),
      duration(0),
      rate(1 << 16),
      volume(1 << 8),
      next_track_id(0) {}
MovieHeader::~MovieHeader() {}
FourCC MovieHeader::BoxType() const { return FOURCC_MVHD; }

bool MovieHeader::ReadWrite(BoxBuffer* buffer) {
  RCHECK(FullBox::ReadWrite(buffer));

  size_t num_bytes = (version == 1) ? sizeof(uint64) : sizeof(uint32);
  RCHECK(buffer->ReadWriteUInt64NBytes(&creation_time, num_bytes) &&
         buffer->ReadWriteUInt64NBytes(&modification_time, num_bytes) &&
         buffer->ReadWriteUInt32(&timescale) &&
         buffer->ReadWriteUInt64NBytes(&duration, num_bytes));

  std::vector<uint8> matrix(kUnityMatrix,
                            kUnityMatrix + arraysize(kUnityMatrix));
  RCHECK(buffer->ReadWriteInt32(&rate) &&
         buffer->ReadWriteInt16(&volume) &&
         buffer->IgnoreBytes(10) &&  // reserved
         buffer->ReadWriteVector(&matrix, matrix.size()) &&
         buffer->IgnoreBytes(24) &&  // predefined zero
         buffer->ReadWriteUInt32(&next_track_id));
  return true;
}

uint32 MovieHeader::ComputeSize() {
  version = IsFitIn32Bits(creation_time, modification_time, duration) ? 0 : 1;
  atom_size = kFullBoxSize + sizeof(uint32) * (1 + version) * 3 +
              sizeof(timescale) + sizeof(rate) + sizeof(volume) +
              sizeof(next_track_id) + sizeof(kUnityMatrix) + 10 +
              24;  // 10 bytes reserved, 24 bytes predefined.
  return atom_size;
}

TrackHeader::TrackHeader()
    : creation_time(0),
      modification_time(0),
      track_id(0),
      duration(0),
      layer(0),
      alternate_group(0),
      volume(-1),
      width(0),
      height(0) {
  flags = kTrackEnabled | kTrackInMovie;
}
TrackHeader::~TrackHeader() {}
FourCC TrackHeader::BoxType() const { return FOURCC_TKHD; }

bool TrackHeader::ReadWrite(BoxBuffer* buffer) {
  RCHECK(FullBox::ReadWrite(buffer));

  size_t num_bytes = (version == 1) ? sizeof(uint64) : sizeof(uint32);
  RCHECK(buffer->ReadWriteUInt64NBytes(&creation_time, num_bytes) &&
         buffer->ReadWriteUInt64NBytes(&modification_time, num_bytes) &&
         buffer->ReadWriteUInt32(&track_id) &&
         buffer->IgnoreBytes(4) &&  // reserved
         buffer->ReadWriteUInt64NBytes(&duration, num_bytes));

  if (!buffer->Reading()) {
    // Set default value for volume, if track is audio, 0x100 else 0.
    if (volume == -1)
      volume = (width != 0 && height != 0) ? 0 : 0x100;
    // Convert integer to 16.16 fix point.
    width <<= 16;
    height <<= 16;
  }
  std::vector<uint8> matrix(kUnityMatrix,
                            kUnityMatrix + arraysize(kUnityMatrix));
  RCHECK(buffer->IgnoreBytes(8) &&  // reserved
         buffer->ReadWriteInt16(&layer) &&
         buffer->ReadWriteInt16(&alternate_group) &&
         buffer->ReadWriteInt16(&volume) &&
         buffer->IgnoreBytes(2) &&  // reserved
         buffer->ReadWriteVector(&matrix, matrix.size()) &&
         buffer->ReadWriteUInt32(&width) &&
         buffer->ReadWriteUInt32(&height));
  // Convert 16.16 fixed point to integer.
  width >>= 16;
  height >>= 16;
  return true;
}

uint32 TrackHeader::ComputeSize() {
  version = IsFitIn32Bits(creation_time, modification_time, duration) ? 0 : 1;
  atom_size = kFullBoxSize + sizeof(track_id) +
              sizeof(uint32) * (1 + version) * 3 + sizeof(layer) +
              sizeof(alternate_group) + sizeof(volume) + sizeof(width) +
              sizeof(height) + sizeof(kUnityMatrix) + 14;  // 14 bytes reserved.
  return atom_size;
}

SampleDescription::SampleDescription() : type(kInvalid) {}
SampleDescription::~SampleDescription() {}
FourCC SampleDescription::BoxType() const { return FOURCC_STSD; }

bool SampleDescription::ReadWrite(BoxBuffer* buffer) {
  uint32 count = 0;
  if (type == kVideo)
    count = video_entries.size();
  else
    count = audio_entries.size();
  RCHECK(FullBox::ReadWrite(buffer) &&
         buffer->ReadWriteUInt32(&count));

  if (buffer->Reading()) {
    BoxReader* reader = buffer->reader();
    DCHECK(reader);
    video_entries.clear();
    audio_entries.clear();
    // Note: this value is preset before scanning begins. See comments in the
    // Parse(Media*) function.
    if (type == kVideo) {
      RCHECK(reader->ReadAllChildren(&video_entries));
      RCHECK(video_entries.size() == count);
    } else if (type == kAudio) {
      RCHECK(reader->ReadAllChildren(&audio_entries));
      RCHECK(audio_entries.size() == count);
    }
  } else {
    DCHECK_LT(0u, count);
    if (type == kVideo) {
      for (uint32 i = 0; i < count; ++i)
        RCHECK(video_entries[i].ReadWrite(buffer));
    } else if (type == kAudio) {
      for (uint32 i = 0; i < count; ++i)
        RCHECK(audio_entries[i].ReadWrite(buffer));
    } else {
      NOTIMPLEMENTED();
    }
  }
  return true;
}

uint32 SampleDescription::ComputeSize() {
  atom_size = kFullBoxSize + sizeof(uint32);
  if (type == kVideo) {
    for (uint32 i = 0; i < video_entries.size(); ++i)
      atom_size += video_entries[i].ComputeSize();
  } else if (type == kAudio) {
    for (uint32 i = 0; i < audio_entries.size(); ++i)
      atom_size += audio_entries[i].ComputeSize();
  }
  return atom_size;
}

DecodingTimeToSample::DecodingTimeToSample() {}
DecodingTimeToSample::~DecodingTimeToSample() {}
FourCC DecodingTimeToSample::BoxType() const { return FOURCC_STTS; }

bool DecodingTimeToSample::ReadWrite(BoxBuffer* buffer) {
  uint32 count = decoding_time.size();
  RCHECK(FullBox::ReadWrite(buffer) &&
         buffer->ReadWriteUInt32(&count));

  decoding_time.resize(count);
  for (uint32 i = 0; i < count; ++i) {
    RCHECK(buffer->ReadWriteUInt32(&decoding_time[i].sample_count) &&
           buffer->ReadWriteUInt32(&decoding_time[i].sample_delta));
  }
  return true;
}

uint32 DecodingTimeToSample::ComputeSize() {
  atom_size = kFullBoxSize + sizeof(uint32) +
              sizeof(DecodingTime) * decoding_time.size();
  return atom_size;
}

CompositionTimeToSample::CompositionTimeToSample() {}
CompositionTimeToSample::~CompositionTimeToSample() {}
FourCC CompositionTimeToSample::BoxType() const { return FOURCC_CTTS; }

bool CompositionTimeToSample::ReadWrite(BoxBuffer* buffer) {
  uint32 count = composition_offset.size();
  RCHECK(FullBox::ReadWrite(buffer) &&
         buffer->ReadWriteUInt32(&count));

  composition_offset.resize(count);
  for (uint32 i = 0; i < count; ++i) {
    RCHECK(buffer->ReadWriteUInt32(&composition_offset[i].sample_count) &&
           buffer->ReadWriteInt32(&composition_offset[i].sample_offset));
  }
  return true;
}

uint32 CompositionTimeToSample::ComputeSize() {
  // Version 1 to support signed offset.
  version = 1;
  // This box is optional. Skip it if it is empty.
  atom_size = 0;
  if (!composition_offset.empty()) {
    atom_size = kFullBoxSize + sizeof(uint32) +
                sizeof(CompositionOffset) * composition_offset.size();
  }
  return atom_size;
}

SampleToChunk::SampleToChunk() {}
SampleToChunk::~SampleToChunk() {}
FourCC SampleToChunk::BoxType() const { return FOURCC_STSC; }

bool SampleToChunk::ReadWrite(BoxBuffer* buffer) {
  uint32 count = chunk_info.size();
  RCHECK(FullBox::ReadWrite(buffer) &&
         buffer->ReadWriteUInt32(&count));

  chunk_info.resize(count);
  for (uint32 i = 0; i < count; ++i) {
    RCHECK(buffer->ReadWriteUInt32(&chunk_info[i].first_chunk) &&
           buffer->ReadWriteUInt32(&chunk_info[i].samples_per_chunk) &&
           buffer->ReadWriteUInt32(&chunk_info[i].sample_description_index));
    // first_chunk values are always increasing.
    RCHECK(i == 0 ? chunk_info[i].first_chunk == 1
                  : chunk_info[i].first_chunk > chunk_info[i - 1].first_chunk);
  }
  return true;
}

uint32 SampleToChunk::ComputeSize() {
  atom_size =
      kFullBoxSize + sizeof(uint32) + sizeof(ChunkInfo) * chunk_info.size();
  return atom_size;
}

SampleSize::SampleSize() : sample_size(0), sample_count(0) {}
SampleSize::~SampleSize() {}
FourCC SampleSize::BoxType() const { return FOURCC_STSZ; }

bool SampleSize::ReadWrite(BoxBuffer* buffer) {
  RCHECK(FullBox::ReadWrite(buffer) &&
         buffer->ReadWriteUInt32(&sample_size) &&
         buffer->ReadWriteUInt32(&sample_count));

  if (sample_size == 0) {
    if (buffer->Reading())
      sizes.resize(sample_count);
    else
      DCHECK(sample_count == sizes.size());
    for (uint32 i = 0; i < sample_count; ++i)
      RCHECK(buffer->ReadWriteUInt32(&sizes[i]));
  }
  return true;
}

uint32 SampleSize::ComputeSize() {
  atom_size = kFullBoxSize + sizeof(sample_size) + sizeof(sample_count) +
              (sample_size == 0 ? sizeof(uint32) * sizes.size() : 0);
  return atom_size;
}

CompactSampleSize::CompactSampleSize() : field_size(0) {}
CompactSampleSize::~CompactSampleSize() {}
FourCC CompactSampleSize::BoxType() const { return FOURCC_STZ2; }

bool CompactSampleSize::ReadWrite(BoxBuffer* buffer) {
  uint32 sample_count = sizes.size();
  RCHECK(FullBox::ReadWrite(buffer) &&
         buffer->IgnoreBytes(3) &&
         buffer->ReadWriteUInt8(&field_size) &&
         buffer->ReadWriteUInt32(&sample_count));

  // Reserve one more entry if field size is 4 bits.
  sizes.resize(sample_count + (field_size == 4 ? 1 : 0), 0);
  switch (field_size) {
    case 4:
      for (uint32 i = 0; i < sample_count; i += 2) {
        if (buffer->Reading()) {
          uint8 size = 0;
          RCHECK(buffer->ReadWriteUInt8(&size));
          sizes[i] = size >> 4;
          sizes[i + 1] = size & 0x0F;
        } else {
          DCHECK_LT(sizes[i], 16u);
          DCHECK_LT(sizes[i + 1], 16u);
          uint8 size = (sizes[i] << 4) | sizes[i + 1];
          RCHECK(buffer->ReadWriteUInt8(&size));
        }
      }
      break;
    case 8:
      for (uint32 i = 0; i < sample_count; ++i) {
        uint8 size = sizes[i];
        RCHECK(buffer->ReadWriteUInt8(&size));
        sizes[i] = size;
      }
      break;
    case 16:
      for (uint32 i = 0; i < sample_count; ++i) {
        uint16 size = sizes[i];
        RCHECK(buffer->ReadWriteUInt16(&size));
        sizes[i] = size;
      }
      break;
    default:
      RCHECK(false);
  }
  sizes.resize(sample_count);
  return true;
}

uint32 CompactSampleSize::ComputeSize() {
  atom_size = kFullBoxSize + sizeof(uint32) + sizeof(uint32) +
              (field_size * sizes.size() + 7) / 8;
  return atom_size;
}

ChunkOffset::ChunkOffset() {}
ChunkOffset::~ChunkOffset() {}
FourCC ChunkOffset::BoxType() const { return FOURCC_STCO; }

bool ChunkOffset::ReadWrite(BoxBuffer* buffer) {
  uint32 count = offsets.size();
  RCHECK(FullBox::ReadWrite(buffer) &&
         buffer->ReadWriteUInt32(&count));

  offsets.resize(count);
  for (uint32 i = 0; i < count; ++i)
    RCHECK(buffer->ReadWriteUInt64NBytes(&offsets[i], sizeof(uint32)));
  return true;
}

uint32 ChunkOffset::ComputeSize() {
  atom_size = kFullBoxSize + sizeof(uint32) + sizeof(uint32) * offsets.size();
  return atom_size;
}

ChunkLargeOffset::ChunkLargeOffset() {}
ChunkLargeOffset::~ChunkLargeOffset() {}
FourCC ChunkLargeOffset::BoxType() const { return FOURCC_CO64; }

bool ChunkLargeOffset::ReadWrite(BoxBuffer* buffer) {
  uint32 count = offsets.size();

  if (!buffer->Reading()) {
    // Switch to ChunkOffset box if it is able to fit in 32 bits offset.
    if (count == 0 || IsFitIn32Bits(offsets[count - 1])) {
      ChunkOffset stco;
      stco.offsets.swap(offsets);
      DCHECK(buffer->writer());
      stco.Write(buffer->writer());
      stco.offsets.swap(offsets);
      return true;
    }
  }

  RCHECK(FullBox::ReadWrite(buffer) &&
         buffer->ReadWriteUInt32(&count));

  offsets.resize(count);
  for (uint32 i = 0; i < count; ++i)
    RCHECK(buffer->ReadWriteUInt64(&offsets[i]));
  return true;
}

uint32 ChunkLargeOffset::ComputeSize() {
  uint32 count = offsets.size();
  int use_large_offset =
      (count > 0 && !IsFitIn32Bits(offsets[count - 1])) ? 1 : 0;
  atom_size = kFullBoxSize + sizeof(count) +
              sizeof(uint32) * (1 + use_large_offset) * offsets.size();
  return atom_size;
}

SyncSample::SyncSample() {}
SyncSample::~SyncSample() {}
FourCC SyncSample::BoxType() const { return FOURCC_STSS; }

bool SyncSample::ReadWrite(BoxBuffer* buffer) {
  uint32 count = sample_number.size();
  RCHECK(FullBox::ReadWrite(buffer) &&
         buffer->ReadWriteUInt32(&count));

  sample_number.resize(count);
  for (uint32 i = 0; i < count; ++i)
    RCHECK(buffer->ReadWriteUInt32(&sample_number[i]));
  return true;
}

uint32 SyncSample::ComputeSize() {
  // Sync sample box is optional. Skip it if it is empty.
  atom_size = 0;
  if (!sample_number.empty()) {
    atom_size =
        kFullBoxSize + sizeof(uint32) + sizeof(uint32) * sample_number.size();
  }
  return atom_size;
}

SampleTable::SampleTable() {}
SampleTable::~SampleTable() {}
FourCC SampleTable::BoxType() const { return FOURCC_STBL; }

bool SampleTable::ReadWrite(BoxBuffer* buffer) {
  RCHECK(Box::ReadWrite(buffer) &&
         buffer->PrepareChildren() &&
         buffer->ReadWriteChild(&description) &&
         buffer->ReadWriteChild(&decoding_time_to_sample) &&
         buffer->TryReadWriteChild(&composition_time_to_sample) &&
         buffer->ReadWriteChild(&sample_to_chunk));

  if (buffer->Reading()) {
    BoxReader* reader = buffer->reader();
    DCHECK(reader);

    // Either SampleSize or CompactSampleSize must present.
    if (reader->ChildExist(&sample_size)) {
      RCHECK(reader->ReadChild(&sample_size));
    } else {
      CompactSampleSize compact_sample_size;
      RCHECK(reader->ReadChild(&compact_sample_size));
      sample_size.sample_size = 0;
      sample_size.sample_count = compact_sample_size.sizes.size();
      sample_size.sizes.swap(compact_sample_size.sizes);
    }

    // Either ChunkOffset or ChunkLargeOffset must present.
    if (reader->ChildExist(&chunk_large_offset)) {
      RCHECK(reader->ReadChild(&chunk_large_offset));
    } else {
      ChunkOffset chunk_offset;
      RCHECK(reader->ReadChild(&chunk_offset));
      chunk_large_offset.offsets.swap(chunk_offset.offsets);
    }
  } else {
    RCHECK(sample_size.ReadWrite(buffer) &&
           chunk_large_offset.ReadWrite(buffer));
  }
  RCHECK(buffer->TryReadWriteChild(&sync_sample));
  return true;
}

uint32 SampleTable::ComputeSize() {
  atom_size = kBoxSize + description.ComputeSize() +
              decoding_time_to_sample.ComputeSize() +
              composition_time_to_sample.ComputeSize() +
              sample_to_chunk.ComputeSize() + sample_size.ComputeSize() +
              chunk_large_offset.ComputeSize() + sync_sample.ComputeSize();
  return atom_size;
}

EditList::EditList() {}
EditList::~EditList() {}
FourCC EditList::BoxType() const { return FOURCC_ELST; }

bool EditList::ReadWrite(BoxBuffer* buffer) {
  uint32 count = edits.size();
  RCHECK(FullBox::ReadWrite(buffer) && buffer->ReadWriteUInt32(&count));
  edits.resize(count);

  size_t num_bytes = (version == 1) ? sizeof(uint64) : sizeof(uint32);
  for (uint32 i = 0; i < count; ++i) {
    RCHECK(
        buffer->ReadWriteUInt64NBytes(&edits[i].segment_duration, num_bytes) &&
        buffer->ReadWriteInt64NBytes(&edits[i].media_time, num_bytes) &&
        buffer->ReadWriteInt16(&edits[i].media_rate_integer) &&
        buffer->ReadWriteInt16(&edits[i].media_rate_fraction));
  }
  return true;
}

uint32 EditList::ComputeSize() {
  // EditList box is optional. Skip it if it is empty.
  atom_size = 0;
  if (edits.empty())
    return 0;

  version = 0;
  for (uint32 i = 0; i < edits.size(); ++i) {
    if (!IsFitIn32Bits(edits[i].segment_duration, edits[i].media_time)) {
      version = 1;
      break;
    }
  }
  atom_size =
      kFullBoxSize + sizeof(uint32) +
      (sizeof(uint32) * (1 + version) * 2 + sizeof(int16) * 2) * edits.size();
  return atom_size;
}

Edit::Edit() {}
Edit::~Edit() {}
FourCC Edit::BoxType() const { return FOURCC_EDTS; }

bool Edit::ReadWrite(BoxBuffer* buffer) {
  return Box::ReadWrite(buffer) &&
         buffer->PrepareChildren() &&
         buffer->ReadWriteChild(&list);
}

uint32 Edit::ComputeSize() {
  // Edit box is optional. Skip it if it is empty.
  atom_size = 0;
  if (!list.edits.empty())
    atom_size = kBoxSize + list.ComputeSize();
  return atom_size;
}

HandlerReference::HandlerReference() : type(kInvalid) {}
HandlerReference::~HandlerReference() {}
FourCC HandlerReference::BoxType() const { return FOURCC_HDLR; }

bool HandlerReference::ReadWrite(BoxBuffer* buffer) {
  FourCC hdlr_type = FOURCC_NULL;
  std::vector<uint8> handler_name;
  if (!buffer->Reading()) {
    if (type == kVideo) {
      hdlr_type = FOURCC_VIDE;
      handler_name.assign(kVideoHandlerName,
                          kVideoHandlerName + arraysize(kVideoHandlerName));
    } else if (type == kAudio) {
      hdlr_type = FOURCC_SOUN;
      handler_name.assign(kAudioHandlerName,
                          kAudioHandlerName + arraysize(kAudioHandlerName));
    } else {
      NOTIMPLEMENTED();
      return false;
    }
  }
  RCHECK(FullBox::ReadWrite(buffer) &&
         buffer->IgnoreBytes(4) &&  // predefined.
         buffer->ReadWriteFourCC(&hdlr_type));
  if (buffer->Reading()) {
    // Note: for reading, remaining fields in box ignored.
    if (hdlr_type == FOURCC_VIDE) {
      type = kVideo;
    } else if (hdlr_type == FOURCC_SOUN) {
      type = kAudio;
    } else {
      type = kInvalid;
    }
  } else {
    RCHECK(buffer->IgnoreBytes(12) &&  // reserved.
           buffer->ReadWriteVector(&handler_name, handler_name.size()));
  }
  return true;
}

uint32 HandlerReference::ComputeSize() {
  atom_size =
      kFullBoxSize + kFourCCSize + 16 +  // 16 bytes Reserved
      (type == kVideo ? sizeof(kVideoHandlerName) : sizeof(kAudioHandlerName));
  return atom_size;
}

AVCDecoderConfigurationRecord::AVCDecoderConfigurationRecord()
    : version(0),
      profile_indication(0),
      profile_compatibility(0),
      avc_level(0),
      length_size(0) {}

AVCDecoderConfigurationRecord::~AVCDecoderConfigurationRecord() {}
FourCC AVCDecoderConfigurationRecord::BoxType() const { return FOURCC_AVCC; }

bool AVCDecoderConfigurationRecord::ReadWrite(BoxBuffer* buffer) {
  RCHECK(Box::ReadWrite(buffer));
  if (buffer->Reading()) {
    RCHECK(buffer->ReadWriteVector(&data, buffer->Size() - buffer->Pos()));
    BufferReader buffer_reader(&data[0], data.size());
    return ParseData(&buffer_reader);
  } else {
    RCHECK(buffer->ReadWriteVector(&data, data.size()));
  }
  return true;
}

bool AVCDecoderConfigurationRecord::ParseData(BufferReader* reader) {
  RCHECK(reader->Read1(&version) && version == 1 &&
         reader->Read1(&profile_indication) &&
         reader->Read1(&profile_compatibility) &&
         reader->Read1(&avc_level));

  uint8 length_size_minus_one;
  RCHECK(reader->Read1(&length_size_minus_one) &&
         (length_size_minus_one & 0xfc) == 0xfc);
  length_size = (length_size_minus_one & 0x3) + 1;

  uint8 num_sps;
  RCHECK(reader->Read1(&num_sps) && (num_sps & 0xe0) == 0xe0);
  num_sps &= 0x1f;

  sps_list.resize(num_sps);
  for (int i = 0; i < num_sps; i++) {
    uint16 sps_length;
    RCHECK(reader->Read2(&sps_length) &&
           reader->ReadToVector(&sps_list[i], sps_length));
  }

  uint8 num_pps;
  RCHECK(reader->Read1(&num_pps));

  pps_list.resize(num_pps);
  for (int i = 0; i < num_pps; i++) {
    uint16 pps_length;
    RCHECK(reader->Read2(&pps_length) &&
           reader->ReadToVector(&pps_list[i], pps_length));
  }

  return true;
}

uint32 AVCDecoderConfigurationRecord::ComputeSize() {
  atom_size = 0;
  if (!data.empty())
    atom_size = kBoxSize + data.size();
  return atom_size;
}

PixelAspectRatioBox::PixelAspectRatioBox() : h_spacing(0), v_spacing(0) {}
PixelAspectRatioBox::~PixelAspectRatioBox() {}
FourCC PixelAspectRatioBox::BoxType() const { return FOURCC_PASP; }

bool PixelAspectRatioBox::ReadWrite(BoxBuffer* buffer) {
  RCHECK(Box::ReadWrite(buffer) &&
         buffer->ReadWriteUInt32(&h_spacing) &&
         buffer->ReadWriteUInt32(&v_spacing));
  return true;
}

uint32 PixelAspectRatioBox::ComputeSize() {
  // This box is optional. Skip it if it is not initialized.
  atom_size = 0;
  if (h_spacing != 0 || v_spacing != 0) {
    // Both values must be positive.
    DCHECK(h_spacing != 0 && v_spacing != 0);
    atom_size = kBoxSize + sizeof(h_spacing) + sizeof(v_spacing);
  }
  return atom_size;
}

VideoSampleEntry::VideoSampleEntry()
    : format(FOURCC_NULL), data_reference_index(1), width(0), height(0) {}

VideoSampleEntry::~VideoSampleEntry() {}
FourCC VideoSampleEntry::BoxType() const {
  LOG(ERROR) << "VideoSampleEntry should be parsed according to the "
             << "handler type recovered in its Media ancestor.";
  return FOURCC_NULL;
}

bool VideoSampleEntry::ReadWrite(BoxBuffer* buffer) {
  if (buffer->Reading()) {
    DCHECK(buffer->reader());
    format = buffer->reader()->type();
  } else {
    RCHECK(buffer->ReadWriteUInt32(&atom_size) &&
           buffer->ReadWriteFourCC(&format));
  }

  uint32 video_resolution = kVideoResolution;
  uint16 video_frame_count = kVideoFrameCount;
  uint16 video_depth = kVideoDepth;
  int16 predefined = -1;
  RCHECK(buffer->IgnoreBytes(6) &&  // reserved.
         buffer->ReadWriteUInt16(&data_reference_index) &&
         buffer->IgnoreBytes(16) &&  // predefined 0.
         buffer->ReadWriteUInt16(&width) &&
         buffer->ReadWriteUInt16(&height) &&
         buffer->ReadWriteUInt32(&video_resolution) &&
         buffer->ReadWriteUInt32(&video_resolution) &&
         buffer->IgnoreBytes(4) &&  // reserved.
         buffer->ReadWriteUInt16(&video_frame_count) &&
         buffer->IgnoreBytes(32) &&  // comparessor_name.
         buffer->ReadWriteUInt16(&video_depth) &&
         buffer->ReadWriteInt16(&predefined));

  RCHECK(buffer->PrepareChildren() &&
         buffer->TryReadWriteChild(&pixel_aspect));

  if (format == FOURCC_ENCV) {
    if (buffer->Reading()) {
      // Continue scanning until a recognized protection scheme is found,
      // or until we run out of protection schemes.
      while (sinf.type.type != FOURCC_CENC) {
        if (!buffer->ReadWriteChild(&sinf))
          return false;
      }
    } else {
      RCHECK(buffer->ReadWriteChild(&sinf));
    }
  }

  if (format == FOURCC_AVC1 ||
      (format == FOURCC_ENCV && sinf.format.format == FOURCC_AVC1)) {
    RCHECK(buffer->ReadWriteChild(&avcc));
  }
  return true;
}

uint32 VideoSampleEntry::ComputeSize() {
  atom_size = kBoxSize + sizeof(data_reference_index) + sizeof(width) +
              sizeof(height) + sizeof(kVideoResolution) * 2 +
              sizeof(kVideoFrameCount) + sizeof(kVideoDepth) +
              pixel_aspect.ComputeSize() + sinf.ComputeSize() +
              avcc.ComputeSize() + 32 +  // 32 bytes comparessor_name.
              6 + 4 + 16 + 2;  // 6 + 4 bytes reserved, 16 + 2 bytes predefined.
  return atom_size;
}

ElementaryStreamDescriptor::ElementaryStreamDescriptor() {}
ElementaryStreamDescriptor::~ElementaryStreamDescriptor() {}
FourCC ElementaryStreamDescriptor::BoxType() const { return FOURCC_ESDS; }

bool ElementaryStreamDescriptor::ReadWrite(BoxBuffer* buffer) {
  RCHECK(FullBox::ReadWrite(buffer));
  if (buffer->Reading()) {
    std::vector<uint8> data;
    RCHECK(buffer->ReadWriteVector(&data, buffer->Size() - buffer->Pos()));
    RCHECK(es_descriptor.Parse(data));
    if (es_descriptor.IsAAC()) {
      RCHECK(aac_audio_specific_config.Parse(
          es_descriptor.decoder_specific_info()));
    }
  } else {
    DCHECK(buffer->writer());
    es_descriptor.Write(buffer->writer());
  }
  return true;
}

uint32 ElementaryStreamDescriptor::ComputeSize() {
  // This box is optional. Skip it if not initialized.
  atom_size = 0;
  if (es_descriptor.object_type() != kForbidden)
    atom_size = kFullBoxSize + es_descriptor.ComputeSize();
  return atom_size;
}

AudioSampleEntry::AudioSampleEntry()
    : format(FOURCC_NULL),
      data_reference_index(1),
      channelcount(2),
      samplesize(16),
      samplerate(0) {}

AudioSampleEntry::~AudioSampleEntry() {}

FourCC AudioSampleEntry::BoxType() const {
  LOG(ERROR) << "AudioSampleEntry should be parsed according to the "
             << "handler type recovered in its Media ancestor.";
  return FOURCC_NULL;
}

bool AudioSampleEntry::ReadWrite(BoxBuffer* buffer) {
  if (buffer->Reading()) {
    DCHECK(buffer->reader());
    format = buffer->reader()->type();
  } else {
    RCHECK(buffer->ReadWriteUInt32(&atom_size) &&
           buffer->ReadWriteFourCC(&format));
  }

  // Convert from integer to 16.16 fixed point for writing.
  samplerate <<= 16;
  RCHECK(buffer->IgnoreBytes(6) &&  // reserved.
         buffer->ReadWriteUInt16(&data_reference_index) &&
         buffer->IgnoreBytes(8) &&  // reserved.
         buffer->ReadWriteUInt16(&channelcount) &&
         buffer->ReadWriteUInt16(&samplesize) &&
         buffer->IgnoreBytes(4) &&  // predefined.
         buffer->ReadWriteUInt32(&samplerate));
  // Convert from 16.16 fixed point to integer.
  samplerate >>= 16;

  RCHECK(buffer->PrepareChildren());
  if (format == FOURCC_ENCA) {
    if (buffer->Reading()) {
      // Continue scanning until a recognized protection scheme is found,
      // or until we run out of protection schemes.
      while (sinf.type.type != FOURCC_CENC) {
        if (!buffer->ReadWriteChild(&sinf))
          return false;
      }
    } else {
      RCHECK(buffer->ReadWriteChild(&sinf));
    }
  }

  // ESDS is not valid in case of EAC3.
  RCHECK(buffer->TryReadWriteChild(&esds));
  return true;
}

uint32 AudioSampleEntry::ComputeSize() {
  atom_size = kBoxSize + sizeof(data_reference_index) + sizeof(channelcount) +
              sizeof(samplesize) + sizeof(samplerate) + sinf.ComputeSize() +
              esds.ComputeSize() + 6 + 8 +  // 6 + 8 bytes reserved.
              4;                            // 4 bytes predefined.
  return atom_size;
}

MediaHeader::MediaHeader()
    : creation_time(0), modification_time(0), timescale(0), duration(0) {
  language[0] = 0;
}
MediaHeader::~MediaHeader() {}
FourCC MediaHeader::BoxType() const { return FOURCC_MDHD; }

bool MediaHeader::ReadWrite(BoxBuffer* buffer) {
  RCHECK(FullBox::ReadWrite(buffer));

  uint8 num_bytes = (version == 1) ? sizeof(uint64) : sizeof(uint32);
  RCHECK(buffer->ReadWriteUInt64NBytes(&creation_time, num_bytes) &&
         buffer->ReadWriteUInt64NBytes(&modification_time, num_bytes) &&
         buffer->ReadWriteUInt32(&timescale) &&
         buffer->ReadWriteUInt64NBytes(&duration, num_bytes));

  if (buffer->Reading()) {
    // Read language codes into temp first then use BitReader to read the
    // values. ISO-639-2/T language code: unsigned int(5)[3] language (2 bytes).
    std::vector<uint8> temp;
    RCHECK(buffer->ReadWriteVector(&temp, 2));

    BitReader bit_reader(&temp[0], 2);
    bit_reader.SkipBits(1);
    for (int i = 0; i < 3; ++i) {
      CHECK(bit_reader.ReadBits(5, &language[i]));
      language[i] += 0x60;
    }
    language[3] = '\0';
  } else {
    // Set up default language if it is not set.
    const char kUndefinedLanguage[] = "und";
    if (language[0] == 0)
      strcpy(language, kUndefinedLanguage);

    // Lang format: bit(1) pad, unsigned int(5)[3] language.
    uint16 lang = 0;
    for (int i = 0; i < 3; ++i)
      lang |= (language[i] - 0x60) << ((2 - i) * 5);
    RCHECK(buffer->ReadWriteUInt16(&lang));
  }

  RCHECK(buffer->IgnoreBytes(2));  // predefined.
  return true;
}

uint32 MediaHeader::ComputeSize() {
  version = IsFitIn32Bits(creation_time, modification_time, duration) ? 0 : 1;
  atom_size = kFullBoxSize + sizeof(timescale) +
              sizeof(uint32) * (1 + version) * 3 + 2 +  // 2 bytes language.
              2;                                        // 2 bytes predefined.
  return atom_size;
}

VideoMediaHeader::VideoMediaHeader()
    : graphicsmode(0), opcolor_red(0), opcolor_green(0), opcolor_blue(0) {
  const uint32 kVideoMediaHeaderFlags = 1;
  flags = kVideoMediaHeaderFlags;
}
VideoMediaHeader::~VideoMediaHeader() {}
FourCC VideoMediaHeader::BoxType() const { return FOURCC_VMHD; }
bool VideoMediaHeader::ReadWrite(BoxBuffer* buffer) {
  RCHECK(FullBox::ReadWrite(buffer) &&
         buffer->ReadWriteUInt16(&graphicsmode) &&
         buffer->ReadWriteUInt16(&opcolor_red) &&
         buffer->ReadWriteUInt16(&opcolor_green) &&
         buffer->ReadWriteUInt16(&opcolor_blue));
  return true;
}

uint32 VideoMediaHeader::ComputeSize() {
  atom_size = kFullBoxSize + sizeof(graphicsmode) + sizeof(opcolor_red) +
              sizeof(opcolor_green) + sizeof(opcolor_blue);
  return atom_size;
}

SoundMediaHeader::SoundMediaHeader() : balance(0) {}
SoundMediaHeader::~SoundMediaHeader() {}
FourCC SoundMediaHeader::BoxType() const { return FOURCC_SMHD; }
bool SoundMediaHeader::ReadWrite(BoxBuffer* buffer) {
  RCHECK(FullBox::ReadWrite(buffer) &&
         buffer->ReadWriteUInt16(&balance) &&
         buffer->IgnoreBytes(2));  // reserved.
  return true;
}

uint32 SoundMediaHeader::ComputeSize() {
  atom_size = kFullBoxSize + sizeof(balance) + sizeof(uint16);
  return atom_size;
}

DataEntryUrl::DataEntryUrl() {
  const uint32 kDataEntryUrlFlags = 1;
  flags = kDataEntryUrlFlags;
}
DataEntryUrl::~DataEntryUrl() {}
FourCC DataEntryUrl::BoxType() const { return FOURCC_URL; }
bool DataEntryUrl::ReadWrite(BoxBuffer* buffer) {
  RCHECK(FullBox::ReadWrite(buffer));
  if (buffer->Reading()) {
    RCHECK(buffer->ReadWriteVector(&location, buffer->Size() - buffer->Pos()));
  } else {
    RCHECK(buffer->ReadWriteVector(&location, location.size()));
  }
  return true;
}

uint32 DataEntryUrl::ComputeSize() {
  atom_size = kBoxSize + sizeof(flags) + location.size();
  return atom_size;
}

DataReference::DataReference() {
  // Default 1 entry.
  data_entry.resize(1);
}
DataReference::~DataReference() {}
FourCC DataReference::BoxType() const { return FOURCC_DREF; }
bool DataReference::ReadWrite(BoxBuffer* buffer) {
  uint32 entry_count = data_entry.size();
  RCHECK(FullBox::ReadWrite(buffer) &&
         buffer->ReadWriteUInt32(&entry_count));
  data_entry.resize(entry_count);
  RCHECK(buffer->PrepareChildren());
  for (uint32 i = 0; i < entry_count; ++i)
    RCHECK(buffer->ReadWriteChild(&data_entry[i]));
  return true;
}

uint32 DataReference::ComputeSize() {
  uint32 count = data_entry.size();
  atom_size = kFullBoxSize + sizeof(count);
  for (uint32 i = 0; i < count; ++i)
    atom_size += data_entry[i].ComputeSize();
  return atom_size;
}

DataInformation::DataInformation() {}
DataInformation::~DataInformation() {}
FourCC DataInformation::BoxType() const { return FOURCC_DINF; }

bool DataInformation::ReadWrite(BoxBuffer* buffer) {
  return Box::ReadWrite(buffer) &&
         buffer->PrepareChildren() &&
         buffer->ReadWriteChild(&dref);
}

uint32 DataInformation::ComputeSize() {
  atom_size = kBoxSize + dref.ComputeSize();
  return atom_size;
}

MediaInformation::MediaInformation() {}
MediaInformation::~MediaInformation() {}
FourCC MediaInformation::BoxType() const { return FOURCC_MINF; }

bool MediaInformation::ReadWrite(BoxBuffer* buffer) {
  RCHECK(Box::ReadWrite(buffer) &&
         buffer->PrepareChildren() &&
         buffer->ReadWriteChild(&dinf) &&
         buffer->ReadWriteChild(&sample_table));
  if (sample_table.description.type == kVideo)
    RCHECK(buffer->ReadWriteChild(&vmhd));
  else if (sample_table.description.type == kAudio)
    RCHECK(buffer->ReadWriteChild(&smhd));
  else
    NOTIMPLEMENTED();
  // Hint is not supported for now.
  return true;
}

uint32 MediaInformation::ComputeSize() {
  atom_size = kBoxSize + dinf.ComputeSize() + sample_table.ComputeSize();
  if (sample_table.description.type == kVideo)
    atom_size += vmhd.ComputeSize();
  else if (sample_table.description.type == kAudio)
    atom_size += smhd.ComputeSize();
  return atom_size;
}

Media::Media() {}
Media::~Media() {}
FourCC Media::BoxType() const { return FOURCC_MDIA; }

bool Media::ReadWrite(BoxBuffer* buffer) {
  RCHECK(Box::ReadWrite(buffer) &&
         buffer->PrepareChildren() &&
         buffer->ReadWriteChild(&header) &&
         buffer->ReadWriteChild(&handler));
  if (buffer->Reading()) {
    // Maddeningly, the HandlerReference box specifies how to parse the
    // SampleDescription box, making the latter the only box (of those that we
    // support) which cannot be parsed correctly on its own (or even with
    // information from its strict ancestor tree). We thus copy the handler type
    // to the sample description box *before* parsing it to provide this
    // information while parsing.
    information.sample_table.description.type = handler.type;
  } else {
    DCHECK_EQ(information.sample_table.description.type, handler.type);
  }
  RCHECK(buffer->ReadWriteChild(&information));
  return true;
}

uint32 Media::ComputeSize() {
  atom_size = kBoxSize + header.ComputeSize() + handler.ComputeSize() +
              information.ComputeSize();
  return atom_size;
}

Track::Track() {}
Track::~Track() {}
FourCC Track::BoxType() const { return FOURCC_TRAK; }

bool Track::ReadWrite(BoxBuffer* buffer) {
  RCHECK(Box::ReadWrite(buffer) &&
         buffer->PrepareChildren() &&
         buffer->ReadWriteChild(&header) &&
         buffer->ReadWriteChild(&media) &&
         buffer->TryReadWriteChild(&edit));
  return true;
}

uint32 Track::ComputeSize() {
  atom_size = kBoxSize + header.ComputeSize() + media.ComputeSize() +
              edit.ComputeSize();
  return atom_size;
}

MovieExtendsHeader::MovieExtendsHeader() : fragment_duration(0) {}
MovieExtendsHeader::~MovieExtendsHeader() {}
FourCC MovieExtendsHeader::BoxType() const { return FOURCC_MEHD; }

bool MovieExtendsHeader::ReadWrite(BoxBuffer* buffer) {
  RCHECK(FullBox::ReadWrite(buffer));
  size_t num_bytes = (version == 1) ? sizeof(uint64) : sizeof(uint32);
  RCHECK(buffer->ReadWriteUInt64NBytes(&fragment_duration, num_bytes));
  return true;
}

uint32 MovieExtendsHeader::ComputeSize() {
  atom_size = 0;
  // This box is optional. Skip it if it is not used.
  if (fragment_duration != 0) {
    version = IsFitIn32Bits(fragment_duration) ? 0 : 1;
    atom_size = kFullBoxSize + sizeof(uint32) * (1 + version);
  }
  return atom_size;
}

TrackExtends::TrackExtends()
    : track_id(0),
      default_sample_description_index(0),
      default_sample_duration(0),
      default_sample_size(0),
      default_sample_flags(0) {}
TrackExtends::~TrackExtends() {}
FourCC TrackExtends::BoxType() const { return FOURCC_TREX; }

bool TrackExtends::ReadWrite(BoxBuffer* buffer) {
  RCHECK(FullBox::ReadWrite(buffer) &&
         buffer->ReadWriteUInt32(&track_id) &&
         buffer->ReadWriteUInt32(&default_sample_description_index) &&
         buffer->ReadWriteUInt32(&default_sample_duration) &&
         buffer->ReadWriteUInt32(&default_sample_size) &&
         buffer->ReadWriteUInt32(&default_sample_flags));
  return true;
}

uint32 TrackExtends::ComputeSize() {
  atom_size = kFullBoxSize + sizeof(track_id) +
              sizeof(default_sample_description_index) +
              sizeof(default_sample_duration) + sizeof(default_sample_size) +
              sizeof(default_sample_flags);
  return atom_size;
}

MovieExtends::MovieExtends() {}
MovieExtends::~MovieExtends() {}
FourCC MovieExtends::BoxType() const { return FOURCC_MVEX; }

bool MovieExtends::ReadWrite(BoxBuffer* buffer) {
  RCHECK(Box::ReadWrite(buffer) &&
         buffer->PrepareChildren() &&
         buffer->TryReadWriteChild(&header));
  if (buffer->Reading()) {
    DCHECK(buffer->reader());
    RCHECK(buffer->reader()->ReadChildren(&tracks));
  } else {
    for (uint32 i = 0; i < tracks.size(); ++i)
      RCHECK(tracks[i].ReadWrite(buffer));
  }
  return true;
}

uint32 MovieExtends::ComputeSize() {
  // This box is optional. Skip it if it does not contain any track.
  atom_size = 0;
  if (tracks.size() != 0) {
    atom_size = kBoxSize + header.ComputeSize();
    for (uint32 i = 0; i < tracks.size(); ++i)
      atom_size += tracks[i].ComputeSize();
  }
  return atom_size;
}

Movie::Movie() {}
Movie::~Movie() {}
FourCC Movie::BoxType() const { return FOURCC_MOOV; }

bool Movie::ReadWrite(BoxBuffer* buffer) {
  RCHECK(Box::ReadWrite(buffer) &&
         buffer->PrepareChildren() &&
         buffer->ReadWriteChild(&header) &&
         buffer->TryReadWriteChild(&extends));
  if (buffer->Reading()) {
    BoxReader* reader = buffer->reader();
    DCHECK(reader);
    RCHECK(reader->ReadChildren(&tracks) &&
           reader->TryReadChildren(&pssh));
  } else {
    for (uint32 i = 0; i < tracks.size(); ++i)
      RCHECK(tracks[i].ReadWrite(buffer));
    for (uint32 i = 0; i < pssh.size(); ++i)
      RCHECK(pssh[i].ReadWrite(buffer));
  }
  return true;
}

uint32 Movie::ComputeSize() {
  atom_size = kBoxSize + header.ComputeSize() + extends.ComputeSize();
  for (uint32 i = 0; i < tracks.size(); ++i)
    atom_size += tracks[i].ComputeSize();
  for (uint32 i = 0; i < pssh.size(); ++i)
    atom_size += pssh[i].ComputeSize();
  return atom_size;
}

TrackFragmentDecodeTime::TrackFragmentDecodeTime() : decode_time(0) {}
TrackFragmentDecodeTime::~TrackFragmentDecodeTime() {}
FourCC TrackFragmentDecodeTime::BoxType() const { return FOURCC_TFDT; }

bool TrackFragmentDecodeTime::ReadWrite(BoxBuffer* buffer) {
  RCHECK(FullBox::ReadWrite(buffer));
  size_t num_bytes = (version == 1) ? sizeof(uint64) : sizeof(uint32);
  RCHECK(buffer->ReadWriteUInt64NBytes(&decode_time, num_bytes));
  return true;
}

uint32 TrackFragmentDecodeTime::ComputeSize() {
  version = IsFitIn32Bits(decode_time) ? 0 : 1;
  atom_size = kFullBoxSize + sizeof(uint32) * (1 + version);
  return atom_size;
}

MovieFragmentHeader::MovieFragmentHeader() : sequence_number(0) {}
MovieFragmentHeader::~MovieFragmentHeader() {}
FourCC MovieFragmentHeader::BoxType() const { return FOURCC_MFHD; }

bool MovieFragmentHeader::ReadWrite(BoxBuffer* buffer) {
  return FullBox::ReadWrite(buffer) &&
         buffer->ReadWriteUInt32(&sequence_number);
}

uint32 MovieFragmentHeader::ComputeSize() {
  atom_size = kFullBoxSize + sizeof(sequence_number);
  return atom_size;
}

TrackFragmentHeader::TrackFragmentHeader()
    : track_id(0),
      sample_description_index(0),
      default_sample_duration(0),
      default_sample_size(0),
      default_sample_flags(0) {}

TrackFragmentHeader::~TrackFragmentHeader() {}
FourCC TrackFragmentHeader::BoxType() const { return FOURCC_TFHD; }

bool TrackFragmentHeader::ReadWrite(BoxBuffer* buffer) {
  RCHECK(FullBox::ReadWrite(buffer) &&
         buffer->ReadWriteUInt32(&track_id));

  // Media Source specific: reject tracks that set 'base-data-offset-present'.
  // Although the Media Source requires that 'default-base-is-moof' (14496-12
  // Amendment 2) be set, we omit this check as many otherwise-valid files in
  // the wild don't set it.
  //
  //  RCHECK((flags & kDefaultBaseIsMoofMask) &&
  //         !(flags & kBaseDataOffsetPresentMask));
  if (flags & kDataOffsetPresentMask) {
    NOTIMPLEMENTED() << " base-data-offset-present is not supported.";
    return false;
  }

  if (flags & kSampleDescriptionIndexPresentMask) {
    RCHECK(buffer->ReadWriteUInt32(&sample_description_index));
  } else if (buffer->Reading()) {
    sample_description_index = 0;
  }

  if (flags & kDefaultSampleDurationPresentMask) {
    RCHECK(buffer->ReadWriteUInt32(&default_sample_duration));
  } else if (buffer->Reading()) {
    default_sample_duration = 0;
  }

  if (flags & kDefaultSampleSizePresentMask) {
    RCHECK(buffer->ReadWriteUInt32(&default_sample_size));
  } else if (buffer->Reading()) {
    default_sample_size = 0;
  }

  if (flags & kDefaultSampleFlagsPresentMask)
    RCHECK(buffer->ReadWriteUInt32(&default_sample_flags));
  return true;
}

uint32 TrackFragmentHeader::ComputeSize() {
  atom_size = kFullBoxSize + sizeof(track_id);
  if (flags & kSampleDescriptionIndexPresentMask)
    atom_size += sizeof(sample_description_index);
  if (flags & kDefaultSampleDurationPresentMask)
    atom_size += sizeof(default_sample_duration);
  if (flags & kDefaultSampleSizePresentMask)
    atom_size += sizeof(default_sample_size);
  if (flags & kDefaultSampleFlagsPresentMask)
    atom_size += sizeof(default_sample_flags);
  return atom_size;
}

TrackFragmentRun::TrackFragmentRun() : sample_count(0), data_offset(0) {}
TrackFragmentRun::~TrackFragmentRun() {}
FourCC TrackFragmentRun::BoxType() const { return FOURCC_TRUN; }

bool TrackFragmentRun::ReadWrite(BoxBuffer* buffer) {
  RCHECK(FullBox::ReadWrite(buffer) &&
         buffer->ReadWriteUInt32(&sample_count));

  bool data_offset_present = (flags & kDataOffsetPresentMask) != 0;
  bool first_sample_flags_present = (flags & kFirstSampleFlagsPresentMask) != 0;
  bool sample_duration_present = (flags & kSampleDurationPresentMask) != 0;
  bool sample_size_present = (flags & kSampleSizePresentMask) != 0;
  bool sample_flags_present = (flags & kSampleFlagsPresentMask) != 0;
  bool sample_composition_time_offsets_present =
      (flags & kSampleCompTimeOffsetsPresentMask) != 0;

  if (data_offset_present) {
    RCHECK(buffer->ReadWriteUInt32(&data_offset));
  } else {
    // NOTE: If the data-offset is not present, then the data for this run
    // starts immediately after the data of the previous run, or at the
    // base-data-offset defined by the track fragment header if this is the
    // first run in a track fragment. If the data-offset is present, it is
    // relative to the base-data-offset established in the track fragment
    // header.
    NOTIMPLEMENTED();
  }

  uint32 first_sample_flags;

  if (buffer->Reading()) {
    if (first_sample_flags_present)
      RCHECK(buffer->ReadWriteUInt32(&first_sample_flags));

    if (sample_duration_present)
      sample_durations.resize(sample_count);
    if (sample_size_present)
      sample_sizes.resize(sample_count);
    if (sample_flags_present)
      sample_flags.resize(sample_count);
    if (sample_composition_time_offsets_present)
      sample_composition_time_offsets.resize(sample_count);
  } else {
    if (first_sample_flags_present) {
      first_sample_flags = sample_flags[0];
      DCHECK(sample_flags.size() == 1);
      RCHECK(buffer->ReadWriteUInt32(&first_sample_flags));
    }

    if (sample_duration_present)
      DCHECK(sample_durations.size() == sample_count);
    if (sample_size_present)
      DCHECK(sample_sizes.size() == sample_count);
    if (sample_flags_present)
      DCHECK(sample_flags.size() == sample_count);
    if (sample_composition_time_offsets_present)
      DCHECK(sample_composition_time_offsets.size() == sample_count);
  }

  for (uint32 i = 0; i < sample_count; ++i) {
    if (sample_duration_present)
      RCHECK(buffer->ReadWriteUInt32(&sample_durations[i]));
    if (sample_size_present)
      RCHECK(buffer->ReadWriteUInt32(&sample_sizes[i]));
    if (sample_flags_present)
      RCHECK(buffer->ReadWriteUInt32(&sample_flags[i]));
    if (sample_composition_time_offsets_present)
      RCHECK(buffer->ReadWriteInt32(&sample_composition_time_offsets[i]));
  }

  if (buffer->Reading()) {
    if (first_sample_flags_present) {
      if (sample_flags.size() == 0) {
        sample_flags.push_back(first_sample_flags);
      } else {
        sample_flags[0] = first_sample_flags;
      }
    }
  }
  return true;
}

uint32 TrackFragmentRun::ComputeSize() {
  version = 1;  // Version 1 to support signed offset.
  atom_size = kFullBoxSize + sizeof(sample_count);
  if (flags & kDataOffsetPresentMask)
    atom_size += sizeof(data_offset);
  if (flags & kFirstSampleFlagsPresentMask)
    atom_size += sizeof(uint32);
  uint32 fields = (flags & kSampleDurationPresentMask ? 1 : 0) +
                  (flags & kSampleSizePresentMask ? 1 : 0) +
                  (flags & kSampleFlagsPresentMask ? 1 : 0) +
                  (flags & kSampleCompTimeOffsetsPresentMask ? 1 : 0);
  atom_size += fields * sizeof(uint32) * sample_count;
  return atom_size;
}

SampleToGroup::SampleToGroup() : grouping_type(0), grouping_type_parameter(0) {}
SampleToGroup::~SampleToGroup() {}
FourCC SampleToGroup::BoxType() const { return FOURCC_SBGP; }

bool SampleToGroup::ReadWrite(BoxBuffer* buffer) {
  RCHECK(FullBox::ReadWrite(buffer) &&
         buffer->ReadWriteUInt32(&grouping_type));
  if (version == 1)
    RCHECK(buffer->ReadWriteUInt32(&grouping_type_parameter));

  if (grouping_type != FOURCC_SEIG) {
    DCHECK(buffer->Reading());
    DLOG(WARNING) << "Sample group '" << grouping_type << "' is not supported.";
    return true;
  }

  uint32 count = entries.size();
  RCHECK(buffer->ReadWriteUInt32(&count));
  entries.resize(count);
  for (uint32 i = 0; i < count; ++i) {
    RCHECK(buffer->ReadWriteUInt32(&entries[i].sample_count) &&
           buffer->ReadWriteUInt32(&entries[i].group_description_index));
  }
  return true;
}

uint32 SampleToGroup::ComputeSize() {
  // This box is optional. Skip it if it is not used.
  atom_size = 0;
  if (!entries.empty()) {
    atom_size = kFullBoxSize + sizeof(grouping_type) +
                (version == 1 ? sizeof(grouping_type_parameter) : 0) +
                sizeof(uint32) + entries.size() * sizeof(entries[0]);
  }
  return atom_size;
}

CencSampleEncryptionInfoEntry::CencSampleEncryptionInfoEntry()
    : is_encrypted(false), iv_size(0) {
}
CencSampleEncryptionInfoEntry::~CencSampleEncryptionInfoEntry() {};

SampleGroupDescription::SampleGroupDescription() : grouping_type(0) {}
SampleGroupDescription::~SampleGroupDescription() {}
FourCC SampleGroupDescription::BoxType() const { return FOURCC_SGPD; }

bool SampleGroupDescription::ReadWrite(BoxBuffer* buffer) {
  RCHECK(FullBox::ReadWrite(buffer) &&
         buffer->ReadWriteUInt32(&grouping_type));

  if (grouping_type != FOURCC_SEIG) {
    DCHECK(buffer->Reading());
    DLOG(WARNING) << "Sample group '" << grouping_type << "' is not supported.";
    return true;
  }

  const size_t kKeyIdSize = 16;
  const size_t kEntrySize = sizeof(uint32) + kKeyIdSize;
  uint32 default_length = 0;
  if (version == 1) {
    if (buffer->Reading()) {
      RCHECK(buffer->ReadWriteUInt32(&default_length));
      RCHECK(default_length == 0 || default_length == kEntrySize);
    } else {
      default_length = kEntrySize;
      RCHECK(buffer->ReadWriteUInt32(&default_length));
    }
  }

  uint32 count = entries.size();
  RCHECK(buffer->ReadWriteUInt32(&count));
  entries.resize(count);
  for (uint32 i = 0; i < count; ++i) {
    if (version == 1) {
      if (buffer->Reading() && default_length == 0) {
        uint32 description_length = 0;
        RCHECK(buffer->ReadWriteUInt32(&description_length));
        RCHECK(description_length == kEntrySize);
      }
    }

    if (!buffer->Reading())
      RCHECK(entries[i].key_id.size() == kKeyIdSize);

    uint8 flag = entries[i].is_encrypted ? 1 : 0;
    RCHECK(buffer->IgnoreBytes(2) &&  // reserved.
           buffer->ReadWriteUInt8(&flag) &&
           buffer->ReadWriteUInt8(&entries[i].iv_size) &&
           buffer->ReadWriteVector(&entries[i].key_id, kKeyIdSize));

    if (buffer->Reading()) {
      entries[i].is_encrypted = (flag != 0);
      if (entries[i].is_encrypted) {
        RCHECK(entries[i].iv_size == 8 || entries[i].iv_size == 16);
      } else {
        RCHECK(entries[i].iv_size == 0);
      }
    }
  }
  return true;
}

uint32 SampleGroupDescription::ComputeSize() {
  // This box is optional. Skip it if it is not used.
  atom_size = 0;
  if (!entries.empty()) {
    const size_t kKeyIdSize = 16;
    const size_t kEntrySize = sizeof(uint32) + kKeyIdSize;
    atom_size = kFullBoxSize + sizeof(grouping_type) +
                (version == 1 ? sizeof(uint32) : 0) +
                sizeof(uint32) + entries.size() * kEntrySize;
  }
  return atom_size;
}

TrackFragment::TrackFragment() {}
TrackFragment::~TrackFragment() {}
FourCC TrackFragment::BoxType() const { return FOURCC_TRAF; }

bool TrackFragment::ReadWrite(BoxBuffer* buffer) {
  RCHECK(Box::ReadWrite(buffer) &&
         buffer->PrepareChildren() &&
         buffer->ReadWriteChild(&header) &&
         // Media Source specific: 'tfdt' required
         buffer->ReadWriteChild(&decode_time));
  if (buffer->Reading()) {
    DCHECK(buffer->reader());
    RCHECK(buffer->reader()->TryReadChildren(&runs));

    while (sample_to_group.grouping_type != FOURCC_SEIG &&
           buffer->reader()->ChildExist(&sample_to_group)) {
      RCHECK(buffer->reader()->ReadChild(&sample_to_group));
    }
    while (sample_group_description.grouping_type != FOURCC_SEIG &&
           buffer->reader()->ChildExist(&sample_group_description)) {
      RCHECK(buffer->reader()->ReadChild(&sample_group_description));
    }
    if (sample_to_group.grouping_type == FOURCC_SEIG) {
      // SampleGroupDescription box can appear in either 'moov...stbl' or
      // 'moov.traf'. The first case is not supported for now, so we require
      // a companion SampleGroupDescription box to coexist with the
      // SampleToGroup box.
      if (sample_group_description.grouping_type != FOURCC_SEIG) {
        NOTIMPLEMENTED()
            << "SampleGroupDescription box in 'moov' is not supported.";
        return false;
      }
      for (std::vector<SampleToGroupEntry>::iterator it =
               sample_to_group.entries.begin();
           it != sample_to_group.entries.end();
           ++it) {
        if ((it->group_description_index & 0x10000) == 0) {
          NOTIMPLEMENTED()
              << "SampleGroupDescription box in 'moov' is not supported.";
          return false;
        }
        it->group_description_index &= 0x0FFFF;
        RCHECK(it->group_description_index <=
               sample_group_description.entries.size());
      }
    } else {
      RCHECK(sample_group_description.grouping_type != FOURCC_SEIG);
    }
  } else {
    for (uint32 i = 0; i < runs.size(); ++i)
      RCHECK(runs[i].ReadWrite(buffer));
    RCHECK(buffer->TryReadWriteChild(&sample_to_group) &&
           buffer->TryReadWriteChild(&sample_group_description));
  }
  return buffer->TryReadWriteChild(&auxiliary_size) &&
         buffer->TryReadWriteChild(&auxiliary_offset);
}

uint32 TrackFragment::ComputeSize() {
  atom_size = kBoxSize + header.ComputeSize() + decode_time.ComputeSize() +
              sample_to_group.ComputeSize() +
              sample_group_description.ComputeSize() +
              auxiliary_size.ComputeSize() + auxiliary_offset.ComputeSize();
  for (uint32 i = 0; i < runs.size(); ++i)
    atom_size += runs[i].ComputeSize();
  return atom_size;
}

MovieFragment::MovieFragment() {}
MovieFragment::~MovieFragment() {}
FourCC MovieFragment::BoxType() const { return FOURCC_MOOF; }

bool MovieFragment::ReadWrite(BoxBuffer* buffer) {
  RCHECK(Box::ReadWrite(buffer) &&
         buffer->PrepareChildren() &&
         buffer->ReadWriteChild(&header));
  if (buffer->Reading()) {
    BoxReader* reader = buffer->reader();
    DCHECK(reader);
    RCHECK(reader->ReadChildren(&tracks) &&
           reader->TryReadChildren(&pssh));
  } else {
    for (uint32 i = 0; i < tracks.size(); ++i)
      RCHECK(tracks[i].ReadWrite(buffer));
    for (uint32 i = 0; i < pssh.size(); ++i)
      RCHECK(pssh[i].ReadWrite(buffer));
  }
  return true;
}

uint32 MovieFragment::ComputeSize() {
  atom_size = kBoxSize + header.ComputeSize();
  for (uint32 i = 0; i < tracks.size(); ++i)
    atom_size += tracks[i].ComputeSize();
  for (uint32 i = 0; i < pssh.size(); ++i)
    atom_size += pssh[i].ComputeSize();
  return atom_size;
}

SegmentIndex::SegmentIndex()
    : reference_id(0),
      timescale(0),
      earliest_presentation_time(0),
      first_offset(0) {}
SegmentIndex::~SegmentIndex() {}
FourCC SegmentIndex::BoxType() const { return FOURCC_SIDX; }

bool SegmentIndex::ReadWrite(BoxBuffer* buffer) {
  RCHECK(FullBox::ReadWrite(buffer) &&
         buffer->ReadWriteUInt32(&reference_id) &&
         buffer->ReadWriteUInt32(&timescale));

  size_t num_bytes = (version == 1) ? sizeof(uint64) : sizeof(uint32);
  RCHECK(
      buffer->ReadWriteUInt64NBytes(&earliest_presentation_time, num_bytes) &&
      buffer->ReadWriteUInt64NBytes(&first_offset, num_bytes));

  uint16 reference_count = references.size();
  RCHECK(buffer->IgnoreBytes(2) &&  // reserved.
         buffer->ReadWriteUInt16(&reference_count));
  references.resize(reference_count);

  uint32 reference_type_size;
  uint32 sap;
  for (uint32 i = 0; i < reference_count; ++i) {
    if (!buffer->Reading()) {
      reference_type_size = references[i].referenced_size;
      if (references[i].reference_type)
        reference_type_size |= (1 << 31);
      sap = (references[i].sap_type << 28) | references[i].sap_delta_time;
      if (references[i].starts_with_sap)
        sap |= (1 << 31);
    }
    RCHECK(buffer->ReadWriteUInt32(&reference_type_size) &&
           buffer->ReadWriteUInt32(&references[i].subsegment_duration) &&
           buffer->ReadWriteUInt32(&sap));
    if (buffer->Reading()) {
      references[i].reference_type = (reference_type_size >> 31) ? true : false;
      references[i].referenced_size = reference_type_size & ~(1 << 31);
      references[i].starts_with_sap = (sap >> 31) ? true : false;
      references[i].sap_type =
          static_cast<SegmentReference::SAPType>((sap >> 28) & 0x07);
      references[i].sap_delta_time = sap & ~(0xF << 28);
    }
  }
  return true;
}

uint32 SegmentIndex::ComputeSize() {
  version = IsFitIn32Bits(earliest_presentation_time, first_offset) ? 0 : 1;
  atom_size = kFullBoxSize + sizeof(reference_id) + sizeof(timescale) +
              sizeof(uint32) * (1 + version) * 2 + 2 * sizeof(uint16) +
              3 * sizeof(uint32) * references.size();
  return atom_size;
}

MediaData::MediaData() : data_size(0) {}
MediaData::~MediaData() {}
FourCC MediaData::BoxType() const { return FOURCC_MDAT; }

void MediaData::Write(BufferWriter* buffer) {
  buffer->AppendInt(ComputeSize());
  buffer->AppendInt(static_cast<uint32>(BoxType()));
}

uint32 MediaData::ComputeSize() {
  return kBoxSize + data_size;
}

}  // namespace mp4
}  // namespace media
