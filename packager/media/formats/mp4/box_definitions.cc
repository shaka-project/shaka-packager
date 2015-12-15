// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "packager/media/formats/mp4/box_definitions.h"

#include <limits>

#include "packager/base/logging.h"
#include "packager/media/base/bit_reader.h"
#include "packager/media/formats/mp4/box_buffer.h"
#include "packager/media/formats/mp4/rcheck.h"

namespace {
const uint32_t kFourCCSize = 4;

// Key Id size as defined in CENC spec.
const uint32_t kCencKeyIdSize = 16;

// 9 uint32_t in big endian formatted array.
const uint8_t kUnityMatrix[] = {0, 1, 0, 0, 0, 0, 0, 0, 0,    0, 0, 0,
                                0, 0, 0, 0, 0, 1, 0, 0, 0,    0, 0, 0,
                                0, 0, 0, 0, 0, 0, 0, 0, 0x40, 0, 0, 0};

// Default entries for HandlerReference box.
const char kVideoHandlerName[] = "VideoHandler";
const char kAudioHandlerName[] = "SoundHandler";

// Default values for VideoSampleEntry box.
const uint32_t kVideoResolution = 0x00480000;  // 72 dpi.
const uint16_t kVideoFrameCount = 1;
const uint16_t kVideoDepth = 0x0018;

const uint32_t kCompressorNameSize = 32u;
const char kAvcCompressorName[] = "\012AVC Coding";
const char kHevcCompressorName[] = "\013HEVC Coding";
const char kVpcCompressorName[] = "\012VPC Coding";

// Utility functions to check if the 64bit integers can fit in 32bit integer.
bool IsFitIn32Bits(uint64_t a) {
  return a <= std::numeric_limits<uint32_t>::max();
}

bool IsFitIn32Bits(int64_t a) {
  return a <= std::numeric_limits<int32_t>::max() &&
         a >= std::numeric_limits<int32_t>::min();
}

template <typename T1, typename T2>
bool IsFitIn32Bits(T1 a1, T2 a2) {
  return IsFitIn32Bits(a1) && IsFitIn32Bits(a2);
}

template <typename T1, typename T2, typename T3>
bool IsFitIn32Bits(T1 a1, T2 a2, T3 a3) {
  return IsFitIn32Bits(a1) && IsFitIn32Bits(a2) && IsFitIn32Bits(a3);
}

}  // namespace

namespace edash_packager {
namespace media {
namespace mp4 {

FileType::FileType() : major_brand(FOURCC_NULL), minor_version(0) {}
FileType::~FileType() {}
FourCC FileType::BoxType() const { return FOURCC_FTYP; }

bool FileType::ReadWriteInternal(BoxBuffer* buffer) {
  RCHECK(ReadWriteHeaderInternal(buffer) &&
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

uint32_t FileType::ComputeSizeInternal() {
  return HeaderSize() + kFourCCSize + sizeof(minor_version) +
         kFourCCSize * compatible_brands.size();
}

FourCC SegmentType::BoxType() const { return FOURCC_STYP; }

ProtectionSystemSpecificHeader::ProtectionSystemSpecificHeader() {}
ProtectionSystemSpecificHeader::~ProtectionSystemSpecificHeader() {}
FourCC ProtectionSystemSpecificHeader::BoxType() const { return FOURCC_PSSH; }

bool ProtectionSystemSpecificHeader::ReadWriteInternal(BoxBuffer* buffer) {
  if (!buffer->Reading() && !raw_box.empty()) {
    // Write the raw box directly.
    buffer->writer()->AppendVector(raw_box);
    return true;
  }

  uint32_t size = data.size();
  RCHECK(ReadWriteHeaderInternal(buffer) &&
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

uint32_t ProtectionSystemSpecificHeader::ComputeSizeInternal() {
  if (!raw_box.empty()) {
    return raw_box.size();
  } else {
    return HeaderSize() + system_id.size() + sizeof(uint32_t) + data.size();
  }
}

SampleAuxiliaryInformationOffset::SampleAuxiliaryInformationOffset() {}
SampleAuxiliaryInformationOffset::~SampleAuxiliaryInformationOffset() {}
FourCC SampleAuxiliaryInformationOffset::BoxType() const { return FOURCC_SAIO; }

bool SampleAuxiliaryInformationOffset::ReadWriteInternal(BoxBuffer* buffer) {
  RCHECK(ReadWriteHeaderInternal(buffer));
  if (flags & 1)
    RCHECK(buffer->IgnoreBytes(8));  // aux_info_type and parameter.

  uint32_t count = offsets.size();
  RCHECK(buffer->ReadWriteUInt32(&count));
  offsets.resize(count);

  size_t num_bytes = (version == 1) ? sizeof(uint64_t) : sizeof(uint32_t);
  for (uint32_t i = 0; i < count; ++i)
    RCHECK(buffer->ReadWriteUInt64NBytes(&offsets[i], num_bytes));
  return true;
}

uint32_t SampleAuxiliaryInformationOffset::ComputeSizeInternal() {
  // This box is optional. Skip it if it is empty.
  if (offsets.size() == 0)
    return 0;
  size_t num_bytes = (version == 1) ? sizeof(uint64_t) : sizeof(uint32_t);
  return HeaderSize() + sizeof(uint32_t) + num_bytes * offsets.size();
}

SampleAuxiliaryInformationSize::SampleAuxiliaryInformationSize()
    : default_sample_info_size(0), sample_count(0) {}
SampleAuxiliaryInformationSize::~SampleAuxiliaryInformationSize() {}
FourCC SampleAuxiliaryInformationSize::BoxType() const { return FOURCC_SAIZ; }

bool SampleAuxiliaryInformationSize::ReadWriteInternal(BoxBuffer* buffer) {
  RCHECK(ReadWriteHeaderInternal(buffer));
  if (flags & 1)
    RCHECK(buffer->IgnoreBytes(8));

  RCHECK(buffer->ReadWriteUInt8(&default_sample_info_size) &&
         buffer->ReadWriteUInt32(&sample_count));
  if (default_sample_info_size == 0)
    RCHECK(buffer->ReadWriteVector(&sample_info_sizes, sample_count));
  return true;
}

uint32_t SampleAuxiliaryInformationSize::ComputeSizeInternal() {
  // This box is optional. Skip it if it is empty.
  if (sample_count == 0)
    return 0;
  return HeaderSize() + sizeof(default_sample_info_size) +
         sizeof(sample_count) +
         (default_sample_info_size == 0 ? sample_info_sizes.size() : 0);
}

OriginalFormat::OriginalFormat() : format(FOURCC_NULL) {}
OriginalFormat::~OriginalFormat() {}
FourCC OriginalFormat::BoxType() const { return FOURCC_FRMA; }

bool OriginalFormat::ReadWriteInternal(BoxBuffer* buffer) {
  return ReadWriteHeaderInternal(buffer) && buffer->ReadWriteFourCC(&format);
}

uint32_t OriginalFormat::ComputeSizeInternal() {
  return HeaderSize() + kFourCCSize;
}

SchemeType::SchemeType() : type(FOURCC_NULL), version(0) {}
SchemeType::~SchemeType() {}
FourCC SchemeType::BoxType() const { return FOURCC_SCHM; }

bool SchemeType::ReadWriteInternal(BoxBuffer* buffer) {
  RCHECK(ReadWriteHeaderInternal(buffer) &&
         buffer->ReadWriteFourCC(&type) &&
         buffer->ReadWriteUInt32(&version));
  return true;
}

uint32_t SchemeType::ComputeSizeInternal() {
  return HeaderSize() + kFourCCSize + sizeof(version);
}

TrackEncryption::TrackEncryption()
    : is_encrypted(false), default_iv_size(0), default_kid(16, 0) {}
TrackEncryption::~TrackEncryption() {}
FourCC TrackEncryption::BoxType() const { return FOURCC_TENC; }

bool TrackEncryption::ReadWriteInternal(BoxBuffer* buffer) {
  if (!buffer->Reading()) {
    if (default_kid.size() != kCencKeyIdSize) {
      LOG(WARNING) << "CENC defines key id length of " << kCencKeyIdSize
                   << " bytes; got " << default_kid.size()
                   << ". Resized accordingly.";
      default_kid.resize(kCencKeyIdSize);
    }
  }

  uint8_t flag = is_encrypted ? 1 : 0;
  RCHECK(ReadWriteHeaderInternal(buffer) &&
         buffer->IgnoreBytes(2) &&  // reserved.
         buffer->ReadWriteUInt8(&flag) &&
         buffer->ReadWriteUInt8(&default_iv_size) &&
         buffer->ReadWriteVector(&default_kid, kCencKeyIdSize));
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

uint32_t TrackEncryption::ComputeSizeInternal() {
  return HeaderSize() + sizeof(uint32_t) + kCencKeyIdSize;
}

SchemeInfo::SchemeInfo() {}
SchemeInfo::~SchemeInfo() {}
FourCC SchemeInfo::BoxType() const { return FOURCC_SCHI; }

bool SchemeInfo::ReadWriteInternal(BoxBuffer* buffer) {
  RCHECK(ReadWriteHeaderInternal(buffer) && buffer->PrepareChildren() &&
         buffer->ReadWriteChild(&track_encryption));
  return true;
}

uint32_t SchemeInfo::ComputeSizeInternal() {
  return HeaderSize() + track_encryption.ComputeSize();
}

ProtectionSchemeInfo::ProtectionSchemeInfo() {}
ProtectionSchemeInfo::~ProtectionSchemeInfo() {}
FourCC ProtectionSchemeInfo::BoxType() const { return FOURCC_SINF; }

bool ProtectionSchemeInfo::ReadWriteInternal(BoxBuffer* buffer) {
  RCHECK(ReadWriteHeaderInternal(buffer) &&
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

uint32_t ProtectionSchemeInfo::ComputeSizeInternal() {
  // Skip sinf box if it is not initialized.
  if (format.format == FOURCC_NULL)
    return 0;
  return HeaderSize() + format.ComputeSize() + type.ComputeSize() +
         info.ComputeSize();
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

bool MovieHeader::ReadWriteInternal(BoxBuffer* buffer) {
  RCHECK(ReadWriteHeaderInternal(buffer));

  size_t num_bytes = (version == 1) ? sizeof(uint64_t) : sizeof(uint32_t);
  RCHECK(buffer->ReadWriteUInt64NBytes(&creation_time, num_bytes) &&
         buffer->ReadWriteUInt64NBytes(&modification_time, num_bytes) &&
         buffer->ReadWriteUInt32(&timescale) &&
         buffer->ReadWriteUInt64NBytes(&duration, num_bytes));

  std::vector<uint8_t> matrix(kUnityMatrix,
                              kUnityMatrix + arraysize(kUnityMatrix));
  RCHECK(buffer->ReadWriteInt32(&rate) &&
         buffer->ReadWriteInt16(&volume) &&
         buffer->IgnoreBytes(10) &&  // reserved
         buffer->ReadWriteVector(&matrix, matrix.size()) &&
         buffer->IgnoreBytes(24) &&  // predefined zero
         buffer->ReadWriteUInt32(&next_track_id));
  return true;
}

uint32_t MovieHeader::ComputeSizeInternal() {
  version = IsFitIn32Bits(creation_time, modification_time, duration) ? 0 : 1;
  return HeaderSize() + sizeof(uint32_t) * (1 + version) * 3 +
         sizeof(timescale) + sizeof(rate) + sizeof(volume) +
         sizeof(next_track_id) + sizeof(kUnityMatrix) + 10 +
         24;  // 10 bytes reserved, 24 bytes predefined.
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

bool TrackHeader::ReadWriteInternal(BoxBuffer* buffer) {
  RCHECK(ReadWriteHeaderInternal(buffer));

  size_t num_bytes = (version == 1) ? sizeof(uint64_t) : sizeof(uint32_t);
  RCHECK(buffer->ReadWriteUInt64NBytes(&creation_time, num_bytes) &&
         buffer->ReadWriteUInt64NBytes(&modification_time, num_bytes) &&
         buffer->ReadWriteUInt32(&track_id) &&
         buffer->IgnoreBytes(4) &&  // reserved
         buffer->ReadWriteUInt64NBytes(&duration, num_bytes));

  if (!buffer->Reading()) {
    // Set default value for volume, if track is audio, 0x100 else 0.
    if (volume == -1)
      volume = (width != 0 && height != 0) ? 0 : 0x100;
  }
  std::vector<uint8_t> matrix(kUnityMatrix,
                              kUnityMatrix + arraysize(kUnityMatrix));
  RCHECK(buffer->IgnoreBytes(8) &&  // reserved
         buffer->ReadWriteInt16(&layer) &&
         buffer->ReadWriteInt16(&alternate_group) &&
         buffer->ReadWriteInt16(&volume) &&
         buffer->IgnoreBytes(2) &&  // reserved
         buffer->ReadWriteVector(&matrix, matrix.size()) &&
         buffer->ReadWriteUInt32(&width) &&
         buffer->ReadWriteUInt32(&height));
  return true;
}

uint32_t TrackHeader::ComputeSizeInternal() {
  version = IsFitIn32Bits(creation_time, modification_time, duration) ? 0 : 1;
  return HeaderSize() + sizeof(track_id) +
         sizeof(uint32_t) * (1 + version) * 3 + sizeof(layer) +
         sizeof(alternate_group) + sizeof(volume) + sizeof(width) +
         sizeof(height) + sizeof(kUnityMatrix) + 14;  // 14 bytes reserved.
}

SampleDescription::SampleDescription() : type(kInvalid) {}
SampleDescription::~SampleDescription() {}
FourCC SampleDescription::BoxType() const { return FOURCC_STSD; }

bool SampleDescription::ReadWriteInternal(BoxBuffer* buffer) {
  uint32_t count = 0;
  if (type == kVideo)
    count = video_entries.size();
  else
    count = audio_entries.size();
  RCHECK(ReadWriteHeaderInternal(buffer) &&
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
      for (uint32_t i = 0; i < count; ++i)
        RCHECK(buffer->ReadWriteChild(&video_entries[i]));
    } else if (type == kAudio) {
      for (uint32_t i = 0; i < count; ++i)
        RCHECK(buffer->ReadWriteChild(&audio_entries[i]));
    } else {
      NOTIMPLEMENTED();
    }
  }
  return true;
}

uint32_t SampleDescription::ComputeSizeInternal() {
  uint32_t box_size = HeaderSize() + sizeof(uint32_t);
  if (type == kVideo) {
    for (uint32_t i = 0; i < video_entries.size(); ++i)
      box_size += video_entries[i].ComputeSize();
  } else if (type == kAudio) {
    for (uint32_t i = 0; i < audio_entries.size(); ++i)
      box_size += audio_entries[i].ComputeSize();
  }
  return box_size;
}

DecodingTimeToSample::DecodingTimeToSample() {}
DecodingTimeToSample::~DecodingTimeToSample() {}
FourCC DecodingTimeToSample::BoxType() const { return FOURCC_STTS; }

bool DecodingTimeToSample::ReadWriteInternal(BoxBuffer* buffer) {
  uint32_t count = decoding_time.size();
  RCHECK(ReadWriteHeaderInternal(buffer) &&
         buffer->ReadWriteUInt32(&count));

  decoding_time.resize(count);
  for (uint32_t i = 0; i < count; ++i) {
    RCHECK(buffer->ReadWriteUInt32(&decoding_time[i].sample_count) &&
           buffer->ReadWriteUInt32(&decoding_time[i].sample_delta));
  }
  return true;
}

uint32_t DecodingTimeToSample::ComputeSizeInternal() {
  return HeaderSize() + sizeof(uint32_t) +
         sizeof(DecodingTime) * decoding_time.size();
}

CompositionTimeToSample::CompositionTimeToSample() {}
CompositionTimeToSample::~CompositionTimeToSample() {}
FourCC CompositionTimeToSample::BoxType() const { return FOURCC_CTTS; }

bool CompositionTimeToSample::ReadWriteInternal(BoxBuffer* buffer) {
  uint32_t count = composition_offset.size();
  if (!buffer->Reading()) {
    // Determine whether version 0 or version 1 should be used.
    // Use version 0 if possible, use version 1 if there is a negative
    // sample_offset value.
    version = 0;
    for (uint32_t i = 0; i < count; ++i) {
      if (composition_offset[i].sample_offset < 0) {
        version = 1;
        break;
      }
    }
  }

  RCHECK(ReadWriteHeaderInternal(buffer) &&
         buffer->ReadWriteUInt32(&count));

  composition_offset.resize(count);
  for (uint32_t i = 0; i < count; ++i) {
    RCHECK(buffer->ReadWriteUInt32(&composition_offset[i].sample_count));

    if (version == 0) {
      uint32_t sample_offset = composition_offset[i].sample_offset;
      RCHECK(buffer->ReadWriteUInt32(&sample_offset));
      composition_offset[i].sample_offset = sample_offset;
    } else {
      int32_t sample_offset = composition_offset[i].sample_offset;
      RCHECK(buffer->ReadWriteInt32(&sample_offset));
      composition_offset[i].sample_offset = sample_offset;
    }
  }
  return true;
}

uint32_t CompositionTimeToSample::ComputeSizeInternal() {
  // This box is optional. Skip it if it is empty.
  if (composition_offset.empty())
    return 0;
  // Structure CompositionOffset contains |sample_offset| (uint32_t) and
  // |sample_offset| (int64_t). The actual size of |sample_offset| is
  // 4 bytes (uint32_t for version 0 and int32_t for version 1).
  const uint32_t kCompositionOffsetSize = sizeof(uint32_t) * 2;
  return HeaderSize() + sizeof(uint32_t) +
         kCompositionOffsetSize * composition_offset.size();
}

SampleToChunk::SampleToChunk() {}
SampleToChunk::~SampleToChunk() {}
FourCC SampleToChunk::BoxType() const { return FOURCC_STSC; }

bool SampleToChunk::ReadWriteInternal(BoxBuffer* buffer) {
  uint32_t count = chunk_info.size();
  RCHECK(ReadWriteHeaderInternal(buffer) &&
         buffer->ReadWriteUInt32(&count));

  chunk_info.resize(count);
  for (uint32_t i = 0; i < count; ++i) {
    RCHECK(buffer->ReadWriteUInt32(&chunk_info[i].first_chunk) &&
           buffer->ReadWriteUInt32(&chunk_info[i].samples_per_chunk) &&
           buffer->ReadWriteUInt32(&chunk_info[i].sample_description_index));
    // first_chunk values are always increasing.
    RCHECK(i == 0 ? chunk_info[i].first_chunk == 1
                  : chunk_info[i].first_chunk > chunk_info[i - 1].first_chunk);
  }
  return true;
}

uint32_t SampleToChunk::ComputeSizeInternal() {
  return HeaderSize() + sizeof(uint32_t) +
         sizeof(ChunkInfo) * chunk_info.size();
}

SampleSize::SampleSize() : sample_size(0), sample_count(0) {}
SampleSize::~SampleSize() {}
FourCC SampleSize::BoxType() const { return FOURCC_STSZ; }

bool SampleSize::ReadWriteInternal(BoxBuffer* buffer) {
  RCHECK(ReadWriteHeaderInternal(buffer) &&
         buffer->ReadWriteUInt32(&sample_size) &&
         buffer->ReadWriteUInt32(&sample_count));

  if (sample_size == 0) {
    if (buffer->Reading())
      sizes.resize(sample_count);
    else
      DCHECK(sample_count == sizes.size());
    for (uint32_t i = 0; i < sample_count; ++i)
      RCHECK(buffer->ReadWriteUInt32(&sizes[i]));
  }
  return true;
}

uint32_t SampleSize::ComputeSizeInternal() {
  return HeaderSize() + sizeof(sample_size) + sizeof(sample_count) +
         (sample_size == 0 ? sizeof(uint32_t) * sizes.size() : 0);
}

CompactSampleSize::CompactSampleSize() : field_size(0) {}
CompactSampleSize::~CompactSampleSize() {}
FourCC CompactSampleSize::BoxType() const { return FOURCC_STZ2; }

bool CompactSampleSize::ReadWriteInternal(BoxBuffer* buffer) {
  uint32_t sample_count = sizes.size();
  RCHECK(ReadWriteHeaderInternal(buffer) &&
         buffer->IgnoreBytes(3) &&
         buffer->ReadWriteUInt8(&field_size) &&
         buffer->ReadWriteUInt32(&sample_count));

  // Reserve one more entry if field size is 4 bits.
  sizes.resize(sample_count + (field_size == 4 ? 1 : 0), 0);
  switch (field_size) {
    case 4:
      for (uint32_t i = 0; i < sample_count; i += 2) {
        if (buffer->Reading()) {
          uint8_t size = 0;
          RCHECK(buffer->ReadWriteUInt8(&size));
          sizes[i] = size >> 4;
          sizes[i + 1] = size & 0x0F;
        } else {
          DCHECK_LT(sizes[i], 16u);
          DCHECK_LT(sizes[i + 1], 16u);
          uint8_t size = (sizes[i] << 4) | sizes[i + 1];
          RCHECK(buffer->ReadWriteUInt8(&size));
        }
      }
      break;
    case 8:
      for (uint32_t i = 0; i < sample_count; ++i) {
        uint8_t size = sizes[i];
        RCHECK(buffer->ReadWriteUInt8(&size));
        sizes[i] = size;
      }
      break;
    case 16:
      for (uint32_t i = 0; i < sample_count; ++i) {
        uint16_t size = sizes[i];
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

uint32_t CompactSampleSize::ComputeSizeInternal() {
  return HeaderSize() + sizeof(uint32_t) + sizeof(uint32_t) +
         (field_size * sizes.size() + 7) / 8;
}

ChunkOffset::ChunkOffset() {}
ChunkOffset::~ChunkOffset() {}
FourCC ChunkOffset::BoxType() const { return FOURCC_STCO; }

bool ChunkOffset::ReadWriteInternal(BoxBuffer* buffer) {
  uint32_t count = offsets.size();
  RCHECK(ReadWriteHeaderInternal(buffer) &&
         buffer->ReadWriteUInt32(&count));

  offsets.resize(count);
  for (uint32_t i = 0; i < count; ++i)
    RCHECK(buffer->ReadWriteUInt64NBytes(&offsets[i], sizeof(uint32_t)));
  return true;
}

uint32_t ChunkOffset::ComputeSizeInternal() {
  return HeaderSize() + sizeof(uint32_t) + sizeof(uint32_t) * offsets.size();
}

ChunkLargeOffset::ChunkLargeOffset() {}
ChunkLargeOffset::~ChunkLargeOffset() {}
FourCC ChunkLargeOffset::BoxType() const { return FOURCC_CO64; }

bool ChunkLargeOffset::ReadWriteInternal(BoxBuffer* buffer) {
  uint32_t count = offsets.size();

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

  RCHECK(ReadWriteHeaderInternal(buffer) &&
         buffer->ReadWriteUInt32(&count));

  offsets.resize(count);
  for (uint32_t i = 0; i < count; ++i)
    RCHECK(buffer->ReadWriteUInt64(&offsets[i]));
  return true;
}

uint32_t ChunkLargeOffset::ComputeSizeInternal() {
  uint32_t count = offsets.size();
  int use_large_offset =
      (count > 0 && !IsFitIn32Bits(offsets[count - 1])) ? 1 : 0;
  return HeaderSize() + sizeof(count) +
         sizeof(uint32_t) * (1 + use_large_offset) * offsets.size();
}

SyncSample::SyncSample() {}
SyncSample::~SyncSample() {}
FourCC SyncSample::BoxType() const { return FOURCC_STSS; }

bool SyncSample::ReadWriteInternal(BoxBuffer* buffer) {
  uint32_t count = sample_number.size();
  RCHECK(ReadWriteHeaderInternal(buffer) &&
         buffer->ReadWriteUInt32(&count));

  sample_number.resize(count);
  for (uint32_t i = 0; i < count; ++i)
    RCHECK(buffer->ReadWriteUInt32(&sample_number[i]));
  return true;
}

uint32_t SyncSample::ComputeSizeInternal() {
  // Sync sample box is optional. Skip it if it is empty.
  if (sample_number.empty())
    return 0;
  return HeaderSize() + sizeof(uint32_t) +
         sizeof(uint32_t) * sample_number.size();
}

SampleTable::SampleTable() {}
SampleTable::~SampleTable() {}
FourCC SampleTable::BoxType() const { return FOURCC_STBL; }

bool SampleTable::ReadWriteInternal(BoxBuffer* buffer) {
  RCHECK(ReadWriteHeaderInternal(buffer) &&
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
    RCHECK(buffer->ReadWriteChild(&sample_size) &&
           buffer->ReadWriteChild(&chunk_large_offset));
  }
  RCHECK(buffer->TryReadWriteChild(&sync_sample));
  return true;
}

uint32_t SampleTable::ComputeSizeInternal() {
  return HeaderSize() + description.ComputeSize() +
         decoding_time_to_sample.ComputeSize() +
         composition_time_to_sample.ComputeSize() +
         sample_to_chunk.ComputeSize() + sample_size.ComputeSize() +
         chunk_large_offset.ComputeSize() + sync_sample.ComputeSize();
}

EditList::EditList() {}
EditList::~EditList() {}
FourCC EditList::BoxType() const { return FOURCC_ELST; }

bool EditList::ReadWriteInternal(BoxBuffer* buffer) {
  uint32_t count = edits.size();
  RCHECK(ReadWriteHeaderInternal(buffer) && buffer->ReadWriteUInt32(&count));
  edits.resize(count);

  size_t num_bytes = (version == 1) ? sizeof(uint64_t) : sizeof(uint32_t);
  for (uint32_t i = 0; i < count; ++i) {
    RCHECK(
        buffer->ReadWriteUInt64NBytes(&edits[i].segment_duration, num_bytes) &&
        buffer->ReadWriteInt64NBytes(&edits[i].media_time, num_bytes) &&
        buffer->ReadWriteInt16(&edits[i].media_rate_integer) &&
        buffer->ReadWriteInt16(&edits[i].media_rate_fraction));
  }
  return true;
}

uint32_t EditList::ComputeSizeInternal() {
  // EditList box is optional. Skip it if it is empty.
  if (edits.empty())
    return 0;

  version = 0;
  for (uint32_t i = 0; i < edits.size(); ++i) {
    if (!IsFitIn32Bits(edits[i].segment_duration, edits[i].media_time)) {
      version = 1;
      break;
    }
  }
  return HeaderSize() + sizeof(uint32_t) +
         (sizeof(uint32_t) * (1 + version) * 2 + sizeof(int16_t) * 2) *
             edits.size();
}

Edit::Edit() {}
Edit::~Edit() {}
FourCC Edit::BoxType() const { return FOURCC_EDTS; }

bool Edit::ReadWriteInternal(BoxBuffer* buffer) {
  return ReadWriteHeaderInternal(buffer) &&
         buffer->PrepareChildren() &&
         buffer->ReadWriteChild(&list);
}

uint32_t Edit::ComputeSizeInternal() {
  // Edit box is optional. Skip it if it is empty.
  if (list.edits.empty())
    return 0;
  return HeaderSize() + list.ComputeSize();
}

HandlerReference::HandlerReference() : type(kInvalid) {}
HandlerReference::~HandlerReference() {}
FourCC HandlerReference::BoxType() const { return FOURCC_HDLR; }

bool HandlerReference::ReadWriteInternal(BoxBuffer* buffer) {
  FourCC hdlr_type = FOURCC_NULL;
  std::vector<uint8_t> handler_name;
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
  RCHECK(ReadWriteHeaderInternal(buffer) &&
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

uint32_t HandlerReference::ComputeSizeInternal() {
  return HeaderSize() + kFourCCSize + 16 +  // 16 bytes Reserved
         (type == kVideo ? sizeof(kVideoHandlerName)
                         : sizeof(kAudioHandlerName));
}

CodecConfigurationRecord::CodecConfigurationRecord() : box_type(FOURCC_NULL) {}
CodecConfigurationRecord::~CodecConfigurationRecord() {}
FourCC CodecConfigurationRecord::BoxType() const {
  // CodecConfigurationRecord should be parsed according to format recovered in
  // VideoSampleEntry. |box_type| is determined dynamically there.
  return box_type;
}

bool CodecConfigurationRecord::ReadWriteInternal(BoxBuffer* buffer) {
  RCHECK(ReadWriteHeaderInternal(buffer));
  if (buffer->Reading()) {
    RCHECK(buffer->ReadWriteVector(&data, buffer->Size() - buffer->Pos()));
  } else {
    RCHECK(buffer->ReadWriteVector(&data, data.size()));
  }
  return true;
}

uint32_t CodecConfigurationRecord::ComputeSizeInternal() {
  if (data.empty())
    return 0;
  return HeaderSize() + data.size();
}

PixelAspectRatio::PixelAspectRatio() : h_spacing(0), v_spacing(0) {}
PixelAspectRatio::~PixelAspectRatio() {}
FourCC PixelAspectRatio::BoxType() const { return FOURCC_PASP; }

bool PixelAspectRatio::ReadWriteInternal(BoxBuffer* buffer) {
  RCHECK(ReadWriteHeaderInternal(buffer) &&
         buffer->ReadWriteUInt32(&h_spacing) &&
         buffer->ReadWriteUInt32(&v_spacing));
  return true;
}

uint32_t PixelAspectRatio::ComputeSizeInternal() {
  // This box is optional. Skip it if it is not initialized.
  if (h_spacing == 0 && v_spacing == 0)
    return 0;
  // Both values must be positive.
  DCHECK(h_spacing != 0 && v_spacing != 0);
  return HeaderSize() + sizeof(h_spacing) + sizeof(v_spacing);
}

VideoSampleEntry::VideoSampleEntry()
    : format(FOURCC_NULL), data_reference_index(1), width(0), height(0) {}

VideoSampleEntry::~VideoSampleEntry() {}
FourCC VideoSampleEntry::BoxType() const {
  if (format == FOURCC_NULL) {
    LOG(ERROR) << "VideoSampleEntry should be parsed according to the "
               << "handler type recovered in its Media ancestor.";
  }
  return format;
}

bool VideoSampleEntry::ReadWriteInternal(BoxBuffer* buffer) {
  std::vector<uint8_t> compressor_name;
  if (buffer->Reading()) {
    DCHECK(buffer->reader());
    format = buffer->reader()->type();
  } else {
    RCHECK(ReadWriteHeaderInternal(buffer));

    const FourCC actual_format = GetActualFormat();
    switch (actual_format) {
      case FOURCC_AVC1:
        compressor_name.assign(
            kAvcCompressorName,
            kAvcCompressorName + arraysize(kAvcCompressorName));
        break;
      case FOURCC_HEV1:
      case FOURCC_HVC1:
        compressor_name.assign(
            kHevcCompressorName,
            kHevcCompressorName + arraysize(kHevcCompressorName));
        break;
      case FOURCC_VP08:
      case FOURCC_VP09:
      case FOURCC_VP10:
        compressor_name.assign(
            kVpcCompressorName,
            kVpcCompressorName + arraysize(kVpcCompressorName));
        break;
      default:
        LOG(ERROR) << FourCCToString(actual_format) << " is not supported.";
        return false;
    }
    compressor_name.resize(kCompressorNameSize);
  }

  uint32_t video_resolution = kVideoResolution;
  uint16_t video_frame_count = kVideoFrameCount;
  uint16_t video_depth = kVideoDepth;
  int16_t predefined = -1;
  RCHECK(buffer->IgnoreBytes(6) &&  // reserved.
         buffer->ReadWriteUInt16(&data_reference_index) &&
         buffer->IgnoreBytes(16) &&  // predefined 0.
         buffer->ReadWriteUInt16(&width) &&
         buffer->ReadWriteUInt16(&height) &&
         buffer->ReadWriteUInt32(&video_resolution) &&
         buffer->ReadWriteUInt32(&video_resolution) &&
         buffer->IgnoreBytes(4) &&  // reserved.
         buffer->ReadWriteUInt16(&video_frame_count) &&
         buffer->ReadWriteVector(&compressor_name, kCompressorNameSize) &&
         buffer->ReadWriteUInt16(&video_depth) &&
         buffer->ReadWriteInt16(&predefined));

  RCHECK(buffer->PrepareChildren());

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

  const FourCC actual_format = GetActualFormat();
  switch (actual_format) {
    case FOURCC_AVC1:
      codec_config_record.box_type = FOURCC_AVCC;
      break;
    case FOURCC_HEV1:
    case FOURCC_HVC1:
      codec_config_record.box_type = FOURCC_HVCC;
      break;
    case FOURCC_VP08:
    case FOURCC_VP09:
    case FOURCC_VP10:
      codec_config_record.box_type = FOURCC_VPCC;
      break;
    default:
      LOG(ERROR) << FourCCToString(actual_format) << " is not supported.";
      return false;
  }
  RCHECK(buffer->ReadWriteChild(&codec_config_record));
  RCHECK(buffer->TryReadWriteChild(&pixel_aspect));
  return true;
}

uint32_t VideoSampleEntry::ComputeSizeInternal() {
  return HeaderSize() + sizeof(data_reference_index) + sizeof(width) +
         sizeof(height) + sizeof(kVideoResolution) * 2 +
         sizeof(kVideoFrameCount) + sizeof(kVideoDepth) +
         pixel_aspect.ComputeSize() + sinf.ComputeSize() +
         codec_config_record.ComputeSize() + kCompressorNameSize + 6 + 4 + 16 +
         2;  // 6 + 4 bytes reserved, 16 + 2 bytes predefined.
}

ElementaryStreamDescriptor::ElementaryStreamDescriptor() {}
ElementaryStreamDescriptor::~ElementaryStreamDescriptor() {}
FourCC ElementaryStreamDescriptor::BoxType() const { return FOURCC_ESDS; }

bool ElementaryStreamDescriptor::ReadWriteInternal(BoxBuffer* buffer) {
  RCHECK(ReadWriteHeaderInternal(buffer));
  if (buffer->Reading()) {
    std::vector<uint8_t> data;
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

uint32_t ElementaryStreamDescriptor::ComputeSizeInternal() {
  // This box is optional. Skip it if not initialized.
  if (es_descriptor.object_type() == kForbidden)
    return 0;
  return HeaderSize() + es_descriptor.ComputeSize();
}

DTSSpecific::DTSSpecific() {}
DTSSpecific::~DTSSpecific() {}
FourCC DTSSpecific::BoxType() const { return FOURCC_DDTS; }

bool DTSSpecific::ReadWriteInternal(BoxBuffer* buffer) {
  RCHECK(ReadWriteHeaderInternal(buffer));

  if (buffer->Reading()) {
    RCHECK(
        buffer->ReadWriteVector(&data, buffer->Size() - buffer->Pos()));
  } else {
    RCHECK(buffer->ReadWriteVector(&data, data.size()));
  }
  return true;
}

uint32_t DTSSpecific::ComputeSizeInternal() {
  // This box is optional. Skip it if not initialized.
  if (data.size() == 0)
    return 0;
  return HeaderSize() + data.size();
}

AudioSampleEntry::AudioSampleEntry()
    : format(FOURCC_NULL),
      data_reference_index(1),
      channelcount(2),
      samplesize(16),
      samplerate(0) {}

AudioSampleEntry::~AudioSampleEntry() {}

FourCC AudioSampleEntry::BoxType() const {
  if (format == FOURCC_NULL) {
    LOG(ERROR) << "AudioSampleEntry should be parsed according to the "
               << "handler type recovered in its Media ancestor.";
  }
  return format;
}

bool AudioSampleEntry::ReadWriteInternal(BoxBuffer* buffer) {
  if (buffer->Reading()) {
    DCHECK(buffer->reader());
    format = buffer->reader()->type();
  } else {
    RCHECK(ReadWriteHeaderInternal(buffer));
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

  RCHECK(buffer->TryReadWriteChild(&esds));
  RCHECK(buffer->TryReadWriteChild(&ddts));

  return true;
}

uint32_t AudioSampleEntry::ComputeSizeInternal() {
  return HeaderSize() + sizeof(data_reference_index) + sizeof(channelcount) +
         sizeof(samplesize) + sizeof(samplerate) + sinf.ComputeSize() +
         esds.ComputeSize() + ddts.ComputeSize() + 6 +
         8 +  // 6 + 8 bytes reserved.
         4;   // 4 bytes predefined.
}

MediaHeader::MediaHeader()
    : creation_time(0), modification_time(0), timescale(0), duration(0) {
  language[0] = 0;
}
MediaHeader::~MediaHeader() {}
FourCC MediaHeader::BoxType() const { return FOURCC_MDHD; }

bool MediaHeader::ReadWriteInternal(BoxBuffer* buffer) {
  RCHECK(ReadWriteHeaderInternal(buffer));

  uint8_t num_bytes = (version == 1) ? sizeof(uint64_t) : sizeof(uint32_t);
  RCHECK(buffer->ReadWriteUInt64NBytes(&creation_time, num_bytes) &&
         buffer->ReadWriteUInt64NBytes(&modification_time, num_bytes) &&
         buffer->ReadWriteUInt32(&timescale) &&
         buffer->ReadWriteUInt64NBytes(&duration, num_bytes));

  if (buffer->Reading()) {
    // Read language codes into temp first then use BitReader to read the
    // values. ISO-639-2/T language code: unsigned int(5)[3] language (2 bytes).
    std::vector<uint8_t> temp;
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
    uint16_t lang = 0;
    for (int i = 0; i < 3; ++i)
      lang |= (language[i] - 0x60) << ((2 - i) * 5);
    RCHECK(buffer->ReadWriteUInt16(&lang));
  }

  RCHECK(buffer->IgnoreBytes(2));  // predefined.
  return true;
}

uint32_t MediaHeader::ComputeSizeInternal() {
  version = IsFitIn32Bits(creation_time, modification_time, duration) ? 0 : 1;
  return HeaderSize() + sizeof(timescale) +
         sizeof(uint32_t) * (1 + version) * 3 + 2 +  // 2 bytes language.
         2;                                          // 2 bytes predefined.
}

VideoMediaHeader::VideoMediaHeader()
    : graphicsmode(0), opcolor_red(0), opcolor_green(0), opcolor_blue(0) {
  const uint32_t kVideoMediaHeaderFlags = 1;
  flags = kVideoMediaHeaderFlags;
}
VideoMediaHeader::~VideoMediaHeader() {}
FourCC VideoMediaHeader::BoxType() const { return FOURCC_VMHD; }
bool VideoMediaHeader::ReadWriteInternal(BoxBuffer* buffer) {
  RCHECK(ReadWriteHeaderInternal(buffer) &&
         buffer->ReadWriteUInt16(&graphicsmode) &&
         buffer->ReadWriteUInt16(&opcolor_red) &&
         buffer->ReadWriteUInt16(&opcolor_green) &&
         buffer->ReadWriteUInt16(&opcolor_blue));
  return true;
}

uint32_t VideoMediaHeader::ComputeSizeInternal() {
  return HeaderSize() + sizeof(graphicsmode) + sizeof(opcolor_red) +
         sizeof(opcolor_green) + sizeof(opcolor_blue);
}

SoundMediaHeader::SoundMediaHeader() : balance(0) {}
SoundMediaHeader::~SoundMediaHeader() {}
FourCC SoundMediaHeader::BoxType() const { return FOURCC_SMHD; }
bool SoundMediaHeader::ReadWriteInternal(BoxBuffer* buffer) {
  RCHECK(ReadWriteHeaderInternal(buffer) &&
         buffer->ReadWriteUInt16(&balance) &&
         buffer->IgnoreBytes(2));  // reserved.
  return true;
}

uint32_t SoundMediaHeader::ComputeSizeInternal() {
  return HeaderSize() + sizeof(balance) + sizeof(uint16_t);
}

DataEntryUrl::DataEntryUrl() {
  const uint32_t kDataEntryUrlFlags = 1;
  flags = kDataEntryUrlFlags;
}
DataEntryUrl::~DataEntryUrl() {}
FourCC DataEntryUrl::BoxType() const { return FOURCC_URL; }
bool DataEntryUrl::ReadWriteInternal(BoxBuffer* buffer) {
  RCHECK(ReadWriteHeaderInternal(buffer));
  if (buffer->Reading()) {
    RCHECK(buffer->ReadWriteVector(&location, buffer->Size() - buffer->Pos()));
  } else {
    RCHECK(buffer->ReadWriteVector(&location, location.size()));
  }
  return true;
}

uint32_t DataEntryUrl::ComputeSizeInternal() {
  return HeaderSize() + location.size();
}

DataReference::DataReference() {
  // Default 1 entry.
  data_entry.resize(1);
}
DataReference::~DataReference() {}
FourCC DataReference::BoxType() const { return FOURCC_DREF; }
bool DataReference::ReadWriteInternal(BoxBuffer* buffer) {
  uint32_t entry_count = data_entry.size();
  RCHECK(ReadWriteHeaderInternal(buffer) &&
         buffer->ReadWriteUInt32(&entry_count));
  data_entry.resize(entry_count);
  RCHECK(buffer->PrepareChildren());
  for (uint32_t i = 0; i < entry_count; ++i)
    RCHECK(buffer->ReadWriteChild(&data_entry[i]));
  return true;
}

uint32_t DataReference::ComputeSizeInternal() {
  uint32_t count = data_entry.size();
  uint32_t box_size = HeaderSize() + sizeof(count);
  for (uint32_t i = 0; i < count; ++i)
    box_size += data_entry[i].ComputeSize();
  return box_size;
}

DataInformation::DataInformation() {}
DataInformation::~DataInformation() {}
FourCC DataInformation::BoxType() const { return FOURCC_DINF; }

bool DataInformation::ReadWriteInternal(BoxBuffer* buffer) {
  return ReadWriteHeaderInternal(buffer) &&
         buffer->PrepareChildren() &&
         buffer->ReadWriteChild(&dref);
}

uint32_t DataInformation::ComputeSizeInternal() {
  return HeaderSize() + dref.ComputeSize();
}

MediaInformation::MediaInformation() {}
MediaInformation::~MediaInformation() {}
FourCC MediaInformation::BoxType() const { return FOURCC_MINF; }

bool MediaInformation::ReadWriteInternal(BoxBuffer* buffer) {
  RCHECK(ReadWriteHeaderInternal(buffer) &&
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

uint32_t MediaInformation::ComputeSizeInternal() {
  uint32_t box_size =
      HeaderSize() + dinf.ComputeSize() + sample_table.ComputeSize();
  if (sample_table.description.type == kVideo)
    box_size += vmhd.ComputeSize();
  else if (sample_table.description.type == kAudio)
    box_size += smhd.ComputeSize();
  return box_size;
}

Media::Media() {}
Media::~Media() {}
FourCC Media::BoxType() const { return FOURCC_MDIA; }

bool Media::ReadWriteInternal(BoxBuffer* buffer) {
  RCHECK(ReadWriteHeaderInternal(buffer) &&
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

uint32_t Media::ComputeSizeInternal() {
  return HeaderSize() + header.ComputeSize() + handler.ComputeSize() +
         information.ComputeSize();
}

Track::Track() {}
Track::~Track() {}
FourCC Track::BoxType() const { return FOURCC_TRAK; }

bool Track::ReadWriteInternal(BoxBuffer* buffer) {
  RCHECK(ReadWriteHeaderInternal(buffer) &&
         buffer->PrepareChildren() &&
         buffer->ReadWriteChild(&header) &&
         buffer->ReadWriteChild(&media) &&
         buffer->TryReadWriteChild(&edit));
  return true;
}

uint32_t Track::ComputeSizeInternal() {
  return HeaderSize() + header.ComputeSize() + media.ComputeSize() +
         edit.ComputeSize();
}

MovieExtendsHeader::MovieExtendsHeader() : fragment_duration(0) {}
MovieExtendsHeader::~MovieExtendsHeader() {}
FourCC MovieExtendsHeader::BoxType() const { return FOURCC_MEHD; }

bool MovieExtendsHeader::ReadWriteInternal(BoxBuffer* buffer) {
  RCHECK(ReadWriteHeaderInternal(buffer));
  size_t num_bytes = (version == 1) ? sizeof(uint64_t) : sizeof(uint32_t);
  RCHECK(buffer->ReadWriteUInt64NBytes(&fragment_duration, num_bytes));
  return true;
}

uint32_t MovieExtendsHeader::ComputeSizeInternal() {
  // This box is optional. Skip it if it is not used.
  if (fragment_duration == 0)
    return 0;
  version = IsFitIn32Bits(fragment_duration) ? 0 : 1;
  return HeaderSize() + sizeof(uint32_t) * (1 + version);
}

TrackExtends::TrackExtends()
    : track_id(0),
      default_sample_description_index(0),
      default_sample_duration(0),
      default_sample_size(0),
      default_sample_flags(0) {}
TrackExtends::~TrackExtends() {}
FourCC TrackExtends::BoxType() const { return FOURCC_TREX; }

bool TrackExtends::ReadWriteInternal(BoxBuffer* buffer) {
  RCHECK(ReadWriteHeaderInternal(buffer) &&
         buffer->ReadWriteUInt32(&track_id) &&
         buffer->ReadWriteUInt32(&default_sample_description_index) &&
         buffer->ReadWriteUInt32(&default_sample_duration) &&
         buffer->ReadWriteUInt32(&default_sample_size) &&
         buffer->ReadWriteUInt32(&default_sample_flags));
  return true;
}

uint32_t TrackExtends::ComputeSizeInternal() {
  return HeaderSize() + sizeof(track_id) +
         sizeof(default_sample_description_index) +
         sizeof(default_sample_duration) + sizeof(default_sample_size) +
         sizeof(default_sample_flags);
}

MovieExtends::MovieExtends() {}
MovieExtends::~MovieExtends() {}
FourCC MovieExtends::BoxType() const { return FOURCC_MVEX; }

bool MovieExtends::ReadWriteInternal(BoxBuffer* buffer) {
  RCHECK(ReadWriteHeaderInternal(buffer) &&
         buffer->PrepareChildren() &&
         buffer->TryReadWriteChild(&header));
  if (buffer->Reading()) {
    DCHECK(buffer->reader());
    RCHECK(buffer->reader()->ReadChildren(&tracks));
  } else {
    for (uint32_t i = 0; i < tracks.size(); ++i)
      RCHECK(buffer->ReadWriteChild(&tracks[i]));
  }
  return true;
}

uint32_t MovieExtends::ComputeSizeInternal() {
  // This box is optional. Skip it if it does not contain any track.
  if (tracks.size() == 0)
    return 0;
  uint32_t box_size = HeaderSize() + header.ComputeSize();
  for (uint32_t i = 0; i < tracks.size(); ++i)
    box_size += tracks[i].ComputeSize();
  return box_size;
}

Movie::Movie() {}
Movie::~Movie() {}
FourCC Movie::BoxType() const { return FOURCC_MOOV; }

bool Movie::ReadWriteInternal(BoxBuffer* buffer) {
  RCHECK(ReadWriteHeaderInternal(buffer) &&
         buffer->PrepareChildren() &&
         buffer->ReadWriteChild(&header) &&
         buffer->TryReadWriteChild(&extends));
  if (buffer->Reading()) {
    BoxReader* reader = buffer->reader();
    DCHECK(reader);
    RCHECK(reader->ReadChildren(&tracks) &&
           reader->TryReadChildren(&pssh));
  } else {
    for (uint32_t i = 0; i < tracks.size(); ++i)
      RCHECK(buffer->ReadWriteChild(&tracks[i]));
    for (uint32_t i = 0; i < pssh.size(); ++i)
      RCHECK(buffer->ReadWriteChild(&pssh[i]));
  }
  return true;
}

uint32_t Movie::ComputeSizeInternal() {
  uint32_t box_size =
      HeaderSize() + header.ComputeSize() + extends.ComputeSize();
  for (uint32_t i = 0; i < tracks.size(); ++i)
    box_size += tracks[i].ComputeSize();
  for (uint32_t i = 0; i < pssh.size(); ++i)
    box_size += pssh[i].ComputeSize();
  return box_size;
}

TrackFragmentDecodeTime::TrackFragmentDecodeTime() : decode_time(0) {}
TrackFragmentDecodeTime::~TrackFragmentDecodeTime() {}
FourCC TrackFragmentDecodeTime::BoxType() const { return FOURCC_TFDT; }

bool TrackFragmentDecodeTime::ReadWriteInternal(BoxBuffer* buffer) {
  RCHECK(ReadWriteHeaderInternal(buffer));
  size_t num_bytes = (version == 1) ? sizeof(uint64_t) : sizeof(uint32_t);
  RCHECK(buffer->ReadWriteUInt64NBytes(&decode_time, num_bytes));
  return true;
}

uint32_t TrackFragmentDecodeTime::ComputeSizeInternal() {
  version = IsFitIn32Bits(decode_time) ? 0 : 1;
  return HeaderSize() + sizeof(uint32_t) * (1 + version);
}

MovieFragmentHeader::MovieFragmentHeader() : sequence_number(0) {}
MovieFragmentHeader::~MovieFragmentHeader() {}
FourCC MovieFragmentHeader::BoxType() const { return FOURCC_MFHD; }

bool MovieFragmentHeader::ReadWriteInternal(BoxBuffer* buffer) {
  return ReadWriteHeaderInternal(buffer) &&
         buffer->ReadWriteUInt32(&sequence_number);
}

uint32_t MovieFragmentHeader::ComputeSizeInternal() {
  return HeaderSize() + sizeof(sequence_number);
}

TrackFragmentHeader::TrackFragmentHeader()
    : track_id(0),
      sample_description_index(0),
      default_sample_duration(0),
      default_sample_size(0),
      default_sample_flags(0) {}

TrackFragmentHeader::~TrackFragmentHeader() {}
FourCC TrackFragmentHeader::BoxType() const { return FOURCC_TFHD; }

bool TrackFragmentHeader::ReadWriteInternal(BoxBuffer* buffer) {
  RCHECK(ReadWriteHeaderInternal(buffer) &&
         buffer->ReadWriteUInt32(&track_id));

  if (flags & kBaseDataOffsetPresentMask) {
    // MSE requires 'default-base-is-moof' to be set and
    // 'base-data-offset-present' not to be set. We omit these checks as some
    // valid files in the wild don't follow these rules, though they use moof as
    // base.
    uint64_t base_data_offset;
    RCHECK(buffer->ReadWriteUInt64(&base_data_offset));
    DLOG(WARNING) << "base-data-offset-present is not expected. Assumes "
                     "default-base-is-moof.";
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

uint32_t TrackFragmentHeader::ComputeSizeInternal() {
  uint32_t box_size = HeaderSize() + sizeof(track_id);
  if (flags & kSampleDescriptionIndexPresentMask)
    box_size += sizeof(sample_description_index);
  if (flags & kDefaultSampleDurationPresentMask)
    box_size += sizeof(default_sample_duration);
  if (flags & kDefaultSampleSizePresentMask)
    box_size += sizeof(default_sample_size);
  if (flags & kDefaultSampleFlagsPresentMask)
    box_size += sizeof(default_sample_flags);
  return box_size;
}

TrackFragmentRun::TrackFragmentRun() : sample_count(0), data_offset(0) {}
TrackFragmentRun::~TrackFragmentRun() {}
FourCC TrackFragmentRun::BoxType() const { return FOURCC_TRUN; }

bool TrackFragmentRun::ReadWriteInternal(BoxBuffer* buffer) {
  if (!buffer->Reading()) {
    // Determine whether version 0 or version 1 should be used.
    // Use version 0 if possible, use version 1 if there is a negative
    // sample_offset value.
    version = 0;
    if (flags & kSampleCompTimeOffsetsPresentMask) {
      for (uint32_t i = 0; i < sample_count; ++i) {
        if (sample_composition_time_offsets[i] < 0) {
          version = 1;
          break;
        }
      }
    }
  }

  RCHECK(ReadWriteHeaderInternal(buffer) &&
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

  uint32_t first_sample_flags;

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

  for (uint32_t i = 0; i < sample_count; ++i) {
    if (sample_duration_present)
      RCHECK(buffer->ReadWriteUInt32(&sample_durations[i]));
    if (sample_size_present)
      RCHECK(buffer->ReadWriteUInt32(&sample_sizes[i]));
    if (sample_flags_present)
      RCHECK(buffer->ReadWriteUInt32(&sample_flags[i]));

    if (sample_composition_time_offsets_present) {
      if (version == 0) {
        uint32_t sample_offset = sample_composition_time_offsets[i];
        RCHECK(buffer->ReadWriteUInt32(&sample_offset));
        sample_composition_time_offsets[i] = sample_offset;
      } else {
        int32_t sample_offset = sample_composition_time_offsets[i];
        RCHECK(buffer->ReadWriteInt32(&sample_offset));
        sample_composition_time_offsets[i] = sample_offset;
      }
    }
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

uint32_t TrackFragmentRun::ComputeSizeInternal() {
  uint32_t box_size = HeaderSize() + sizeof(sample_count);
  if (flags & kDataOffsetPresentMask)
    box_size += sizeof(data_offset);
  if (flags & kFirstSampleFlagsPresentMask)
    box_size += sizeof(uint32_t);
  uint32_t fields = (flags & kSampleDurationPresentMask ? 1 : 0) +
                    (flags & kSampleSizePresentMask ? 1 : 0) +
                    (flags & kSampleFlagsPresentMask ? 1 : 0) +
                    (flags & kSampleCompTimeOffsetsPresentMask ? 1 : 0);
  box_size += fields * sizeof(uint32_t) * sample_count;
  return box_size;
}

SampleToGroup::SampleToGroup() : grouping_type(0), grouping_type_parameter(0) {}
SampleToGroup::~SampleToGroup() {}
FourCC SampleToGroup::BoxType() const { return FOURCC_SBGP; }

bool SampleToGroup::ReadWriteInternal(BoxBuffer* buffer) {
  RCHECK(ReadWriteHeaderInternal(buffer) &&
         buffer->ReadWriteUInt32(&grouping_type));
  if (version == 1)
    RCHECK(buffer->ReadWriteUInt32(&grouping_type_parameter));

  if (grouping_type != FOURCC_SEIG) {
    DCHECK(buffer->Reading());
    DLOG(WARNING) << "Sample group "
                  << FourCCToString(static_cast<FourCC>(grouping_type))
                  << " is not supported.";
    return true;
  }

  uint32_t count = entries.size();
  RCHECK(buffer->ReadWriteUInt32(&count));
  entries.resize(count);
  for (uint32_t i = 0; i < count; ++i) {
    RCHECK(buffer->ReadWriteUInt32(&entries[i].sample_count) &&
           buffer->ReadWriteUInt32(&entries[i].group_description_index));
  }
  return true;
}

uint32_t SampleToGroup::ComputeSizeInternal() {
  // This box is optional. Skip it if it is not used.
  if (entries.empty())
    return 0;
  return HeaderSize() + sizeof(grouping_type) +
         (version == 1 ? sizeof(grouping_type_parameter) : 0) +
         sizeof(uint32_t) + entries.size() * sizeof(entries[0]);
}

CencSampleEncryptionInfoEntry::CencSampleEncryptionInfoEntry()
    : is_encrypted(false), iv_size(0) {
}
CencSampleEncryptionInfoEntry::~CencSampleEncryptionInfoEntry() {};

SampleGroupDescription::SampleGroupDescription() : grouping_type(0) {}
SampleGroupDescription::~SampleGroupDescription() {}
FourCC SampleGroupDescription::BoxType() const { return FOURCC_SGPD; }

bool SampleGroupDescription::ReadWriteInternal(BoxBuffer* buffer) {
  RCHECK(ReadWriteHeaderInternal(buffer) &&
         buffer->ReadWriteUInt32(&grouping_type));

  if (grouping_type != FOURCC_SEIG) {
    DCHECK(buffer->Reading());
    DLOG(WARNING) << "Sample group '" << grouping_type << "' is not supported.";
    return true;
  }

  const size_t kEntrySize = sizeof(uint32_t) + kCencKeyIdSize;
  uint32_t default_length = 0;
  if (version == 1) {
    if (buffer->Reading()) {
      RCHECK(buffer->ReadWriteUInt32(&default_length));
      RCHECK(default_length == 0 || default_length >= kEntrySize);
    } else {
      default_length = kEntrySize;
      RCHECK(buffer->ReadWriteUInt32(&default_length));
    }
  }

  uint32_t count = entries.size();
  RCHECK(buffer->ReadWriteUInt32(&count));
  entries.resize(count);
  for (uint32_t i = 0; i < count; ++i) {
    if (version == 1) {
      if (buffer->Reading() && default_length == 0) {
        uint32_t description_length = 0;
        RCHECK(buffer->ReadWriteUInt32(&description_length));
        RCHECK(description_length >= kEntrySize);
      }
    }

    if (!buffer->Reading()) {
      if (entries[i].key_id.size() != kCencKeyIdSize) {
        LOG(WARNING) << "CENC defines key id length of " << kCencKeyIdSize
                     << " bytes; got " << entries[i].key_id.size()
                     << ". Resized accordingly.";
        entries[i].key_id.resize(kCencKeyIdSize);
      }
    }

    uint8_t flag = entries[i].is_encrypted ? 1 : 0;
    RCHECK(buffer->IgnoreBytes(2) &&  // reserved.
           buffer->ReadWriteUInt8(&flag) &&
           buffer->ReadWriteUInt8(&entries[i].iv_size) &&
           buffer->ReadWriteVector(&entries[i].key_id, kCencKeyIdSize));

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

uint32_t SampleGroupDescription::ComputeSizeInternal() {
  // Version 0 is obsoleted, so always generate version 1 box.
  version = 1;
  // This box is optional. Skip it if it is not used.
  if (entries.empty())
    return 0;
  const size_t kEntrySize = sizeof(uint32_t) + kCencKeyIdSize;
  return HeaderSize() + sizeof(grouping_type) +
         (version == 1 ? sizeof(uint32_t) : 0) + sizeof(uint32_t) +
         entries.size() * kEntrySize;
}

TrackFragment::TrackFragment() : decode_time_absent(false) {}
TrackFragment::~TrackFragment() {}
FourCC TrackFragment::BoxType() const { return FOURCC_TRAF; }

bool TrackFragment::ReadWriteInternal(BoxBuffer* buffer) {
  RCHECK(ReadWriteHeaderInternal(buffer) &&
         buffer->PrepareChildren() &&
         buffer->ReadWriteChild(&header));
  if (buffer->Reading()) {
    DCHECK(buffer->reader());
    decode_time_absent = !buffer->reader()->ChildExist(&decode_time);
    if (!decode_time_absent)
      RCHECK(buffer->ReadWriteChild(&decode_time));
    RCHECK(buffer->reader()->TryReadChildren(&runs));

    // There could be multiple SampleGroupDescription and SampleToGroup boxes
    // with different grouping types. For common encryption, the relevant
    // grouping type is 'seig'. Continue reading until 'seig' is found, or
    // until running out of child boxes.
    while (sample_to_group.grouping_type != FOURCC_SEIG &&
           buffer->reader()->ChildExist(&sample_to_group)) {
      RCHECK(buffer->reader()->ReadChild(&sample_to_group));
    }
    while (sample_group_description.grouping_type != FOURCC_SEIG &&
           buffer->reader()->ChildExist(&sample_group_description)) {
      RCHECK(buffer->reader()->ReadChild(&sample_group_description));
    }
  } else {
    if (!decode_time_absent)
      RCHECK(buffer->ReadWriteChild(&decode_time));
    for (uint32_t i = 0; i < runs.size(); ++i)
      RCHECK(buffer->ReadWriteChild(&runs[i]));
    RCHECK(buffer->TryReadWriteChild(&sample_to_group) &&
           buffer->TryReadWriteChild(&sample_group_description));
  }
  return buffer->TryReadWriteChild(&auxiliary_size) &&
         buffer->TryReadWriteChild(&auxiliary_offset);
}

uint32_t TrackFragment::ComputeSizeInternal() {
  uint32_t box_size =
      HeaderSize() + header.ComputeSize() + decode_time.ComputeSize() +
      sample_to_group.ComputeSize() + sample_group_description.ComputeSize() +
      auxiliary_size.ComputeSize() + auxiliary_offset.ComputeSize();
  for (uint32_t i = 0; i < runs.size(); ++i)
    box_size += runs[i].ComputeSize();
  return box_size;
}

MovieFragment::MovieFragment() {}
MovieFragment::~MovieFragment() {}
FourCC MovieFragment::BoxType() const { return FOURCC_MOOF; }

bool MovieFragment::ReadWriteInternal(BoxBuffer* buffer) {
  RCHECK(ReadWriteHeaderInternal(buffer) &&
         buffer->PrepareChildren() &&
         buffer->ReadWriteChild(&header));
  if (buffer->Reading()) {
    BoxReader* reader = buffer->reader();
    DCHECK(reader);
    RCHECK(reader->ReadChildren(&tracks) &&
           reader->TryReadChildren(&pssh));
  } else {
    for (uint32_t i = 0; i < tracks.size(); ++i)
      RCHECK(buffer->ReadWriteChild(&tracks[i]));
    for (uint32_t i = 0; i < pssh.size(); ++i)
      RCHECK(buffer->ReadWriteChild(&pssh[i]));
  }
  return true;
}

uint32_t MovieFragment::ComputeSizeInternal() {
  uint32_t box_size = HeaderSize() + header.ComputeSize();
  for (uint32_t i = 0; i < tracks.size(); ++i)
    box_size += tracks[i].ComputeSize();
  for (uint32_t i = 0; i < pssh.size(); ++i)
    box_size += pssh[i].ComputeSize();
  return box_size;
}

SegmentIndex::SegmentIndex()
    : reference_id(0),
      timescale(0),
      earliest_presentation_time(0),
      first_offset(0) {}
SegmentIndex::~SegmentIndex() {}
FourCC SegmentIndex::BoxType() const { return FOURCC_SIDX; }

bool SegmentIndex::ReadWriteInternal(BoxBuffer* buffer) {
  RCHECK(ReadWriteHeaderInternal(buffer) &&
         buffer->ReadWriteUInt32(&reference_id) &&
         buffer->ReadWriteUInt32(&timescale));

  size_t num_bytes = (version == 1) ? sizeof(uint64_t) : sizeof(uint32_t);
  RCHECK(
      buffer->ReadWriteUInt64NBytes(&earliest_presentation_time, num_bytes) &&
      buffer->ReadWriteUInt64NBytes(&first_offset, num_bytes));

  uint16_t reference_count = references.size();
  RCHECK(buffer->IgnoreBytes(2) &&  // reserved.
         buffer->ReadWriteUInt16(&reference_count));
  references.resize(reference_count);

  uint32_t reference_type_size;
  uint32_t sap;
  for (uint32_t i = 0; i < reference_count; ++i) {
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

uint32_t SegmentIndex::ComputeSizeInternal() {
  version = IsFitIn32Bits(earliest_presentation_time, first_offset) ? 0 : 1;
  return HeaderSize() + sizeof(reference_id) + sizeof(timescale) +
         sizeof(uint32_t) * (1 + version) * 2 + 2 * sizeof(uint16_t) +
         3 * sizeof(uint32_t) * references.size();
}

MediaData::MediaData() : data_size(0) {}
MediaData::~MediaData() {}
FourCC MediaData::BoxType() const { return FOURCC_MDAT; }

bool MediaData::ReadWriteInternal(BoxBuffer* buffer) {
  NOTIMPLEMENTED() << "Actual data is parsed and written separately.";
  return false;
}

uint32_t MediaData::ComputeSizeInternal() {
  return HeaderSize() + data_size;
}

}  // namespace mp4
}  // namespace media
}  // namespace edash_packager
