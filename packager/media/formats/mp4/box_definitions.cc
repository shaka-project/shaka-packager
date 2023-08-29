// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "packager/media/formats/mp4/box_definitions.h"

#include <gflags/gflags.h>
#include <algorithm>
#include <limits>

#include "packager/base/logging.h"
#include "packager/media/base/bit_reader.h"
#include "packager/media/base/macros.h"
#include "packager/media/base/rcheck.h"
#include "packager/media/formats/mp4/box_buffer.h"

DEFINE_bool(mvex_before_trak,
            false,
            "Android MediaExtractor requires mvex to be written before trak. "
            "Set the flag to true to comply with the requirement.");

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
const char kTextHandlerName[] = "TextHandler";
const char kSubtitleHandlerName[] = "SubtitleHandler";

// Default values for VideoSampleEntry box.
const uint32_t kVideoResolution = 0x00480000;  // 72 dpi.
const uint16_t kVideoFrameCount = 1;
const uint16_t kVideoDepth = 0x0018;

const uint32_t kCompressorNameSize = 32u;
const char kAv1CompressorName[] = "\012AOM Coding";
const char kAvcCompressorName[] = "\012AVC Coding";
const char kDolbyVisionCompressorName[] = "\013DOVI Coding";
const char kHevcCompressorName[] = "\013HEVC Coding";
const char kVpcCompressorName[] = "\012VPC Coding";

// According to ISO/IEC FDIS 23001-7: CENC spec, IV should be either
// 64-bit (8-byte) or 128-bit (16-byte).
// |per_sample_iv_size| of 0 means constant_iv is used.
bool IsIvSizeValid(uint8_t per_sample_iv_size) {
  return per_sample_iv_size == 0 || per_sample_iv_size == 8 ||
         per_sample_iv_size == 16;
}

// Default values to construct the following fields in ddts box. Values are set
// according to FFMPEG.
// bit(2) FrameDuration; // 3 = 4096
// bit(5) StreamConstruction; // 18
// bit(1) CoreLFEPresent; // 0 = none
// bit(6) CoreLayout; // 31 = ignore core layout
// bit(14) CoreSize; // 0
// bit(1) StereoDownmix // 0 = none
// bit(3) RepresentationType; // 4
// bit(16) ChannelLayout; // 0xf = 5.1 channel layout.
// bit(1) MultiAssetFlag // 0 = single asset
// bit(1) LBRDurationMod // 0 = ignore
// bit(1) ReservedBoxPresent // 0 = none
// bit(5) Reserved // 0
const uint8_t kDdtsExtraData[] = {0xe4, 0x7c, 0, 4, 0, 0x0f, 0};

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

namespace shaka {
namespace media {
namespace mp4 {

namespace {

TrackType FourCCToTrackType(FourCC fourcc) {
  switch (fourcc) {
    case FOURCC_vide:
      return kVideo;
    case FOURCC_soun:
      return kAudio;
    case FOURCC_text:
      return kText;
    case FOURCC_subt:
      return kSubtitle;
    default:
      return kInvalid;
  }
}

FourCC TrackTypeToFourCC(TrackType track_type) {
  switch (track_type) {
    case kVideo:
      return FOURCC_vide;
    case kAudio:
      return FOURCC_soun;
    case kText:
      return FOURCC_text;
    case kSubtitle:
      return FOURCC_subt;
    default:
      return FOURCC_NULL;
  }
}

bool IsProtectionSchemeSupported(FourCC scheme) {
  return scheme == FOURCC_cenc || scheme == FOURCC_cens ||
         scheme == FOURCC_cbc1 || scheme == FOURCC_cbcs;
}

}  // namespace

FileType::FileType() = default;
FileType::~FileType() = default;

FourCC FileType::BoxType() const {
  return FOURCC_ftyp;
}

bool FileType::ReadWriteInternal(BoxBuffer* buffer) {
  RCHECK(ReadWriteHeaderInternal(buffer) &&
         buffer->ReadWriteFourCC(&major_brand) &&
         buffer->ReadWriteUInt32(&minor_version));
  size_t num_brands;
  if (buffer->Reading()) {
    RCHECK(buffer->BytesLeft() % sizeof(FourCC) == 0);
    num_brands = buffer->BytesLeft() / sizeof(FourCC);
    compatible_brands.resize(num_brands);
  } else {
    num_brands = compatible_brands.size();
  }
  for (size_t i = 0; i < num_brands; ++i)
    RCHECK(buffer->ReadWriteFourCC(&compatible_brands[i]));
  return true;
}

size_t FileType::ComputeSizeInternal() {
  return HeaderSize() + kFourCCSize + sizeof(minor_version) +
         kFourCCSize * compatible_brands.size();
}

FourCC SegmentType::BoxType() const {
  return FOURCC_styp;
}

ProtectionSystemSpecificHeader::ProtectionSystemSpecificHeader() = default;
ProtectionSystemSpecificHeader::~ProtectionSystemSpecificHeader() = default;

FourCC ProtectionSystemSpecificHeader::BoxType() const {
  return FOURCC_pssh;
}

bool ProtectionSystemSpecificHeader::ReadWriteInternal(BoxBuffer* buffer) {
  if (buffer->Reading()) {
    BoxReader* reader = buffer->reader();
    DCHECK(reader);
    raw_box.assign(reader->data(), reader->data() + reader->size());
  } else {
    DCHECK(!raw_box.empty());
    buffer->writer()->AppendVector(raw_box);
  }

  return true;
}

size_t ProtectionSystemSpecificHeader::ComputeSizeInternal() {
  return raw_box.size();
}

SampleAuxiliaryInformationOffset::SampleAuxiliaryInformationOffset() = default;
SampleAuxiliaryInformationOffset::~SampleAuxiliaryInformationOffset() = default;

FourCC SampleAuxiliaryInformationOffset::BoxType() const {
  return FOURCC_saio;
}

bool SampleAuxiliaryInformationOffset::ReadWriteInternal(BoxBuffer* buffer) {
  RCHECK(ReadWriteHeaderInternal(buffer));
  if (flags & 1)
    RCHECK(buffer->IgnoreBytes(8));  // aux_info_type and parameter.

  uint32_t count = static_cast<uint32_t>(offsets.size());
  RCHECK(buffer->ReadWriteUInt32(&count));
  offsets.resize(count);

  size_t num_bytes = (version == 1) ? sizeof(uint64_t) : sizeof(uint32_t);
  for (uint32_t i = 0; i < count; ++i)
    RCHECK(buffer->ReadWriteUInt64NBytes(&offsets[i], num_bytes));
  return true;
}

size_t SampleAuxiliaryInformationOffset::ComputeSizeInternal() {
  // This box is optional. Skip it if it is empty.
  if (offsets.size() == 0)
    return 0;
  size_t num_bytes = (version == 1) ? sizeof(uint64_t) : sizeof(uint32_t);
  return HeaderSize() + sizeof(uint32_t) + num_bytes * offsets.size();
}

SampleAuxiliaryInformationSize::SampleAuxiliaryInformationSize() = default;
SampleAuxiliaryInformationSize::~SampleAuxiliaryInformationSize() = default;

FourCC SampleAuxiliaryInformationSize::BoxType() const {
  return FOURCC_saiz;
}

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

size_t SampleAuxiliaryInformationSize::ComputeSizeInternal() {
  // This box is optional. Skip it if it is empty.
  if (sample_count == 0)
    return 0;
  return HeaderSize() + sizeof(default_sample_info_size) +
         sizeof(sample_count) +
         (default_sample_info_size == 0 ? sample_info_sizes.size() : 0);
}

bool SampleEncryptionEntry::ReadWrite(uint8_t iv_size,
                                      bool has_subsamples,
                                      BoxBuffer* buffer) {
  DCHECK(IsIvSizeValid(iv_size));
  DCHECK(buffer);

  RCHECK(buffer->ReadWriteVector(&initialization_vector, iv_size));

  if (!has_subsamples) {
    subsamples.clear();
    return true;
  }

  uint16_t subsample_count = static_cast<uint16_t>(subsamples.size());
  RCHECK(buffer->ReadWriteUInt16(&subsample_count));
  RCHECK(subsample_count > 0);
  subsamples.resize(subsample_count);
  for (auto& subsample : subsamples) {
    RCHECK(buffer->ReadWriteUInt16(&subsample.clear_bytes) &&
           buffer->ReadWriteUInt32(&subsample.cipher_bytes));
  }
  return true;
}

bool SampleEncryptionEntry::ParseFromBuffer(uint8_t iv_size,
                                            bool has_subsamples,
                                            BufferReader* reader) {
  DCHECK(IsIvSizeValid(iv_size));
  DCHECK(reader);

  initialization_vector.resize(iv_size);
  RCHECK(reader->ReadToVector(&initialization_vector, iv_size));

  if (!has_subsamples) {
    subsamples.clear();
    return true;
  }

  uint16_t subsample_count;
  RCHECK(reader->Read2(&subsample_count));
  RCHECK(subsample_count > 0);
  subsamples.resize(subsample_count);
  for (auto& subsample : subsamples) {
    RCHECK(reader->Read2(&subsample.clear_bytes) &&
           reader->Read4(&subsample.cipher_bytes));
  }
  return true;
}

uint32_t SampleEncryptionEntry::ComputeSize() const {
  const uint32_t subsample_entry_size = sizeof(uint16_t) + sizeof(uint32_t);
  const uint16_t subsample_count = static_cast<uint16_t>(subsamples.size());
  return static_cast<uint32_t>(
      initialization_vector.size() +
      (subsample_count > 0
           ? (sizeof(subsample_count) + subsample_entry_size * subsample_count)
           : 0));
}

uint32_t SampleEncryptionEntry::GetTotalSizeOfSubsamples() const {
  uint32_t size = 0;
  for (uint32_t i = 0; i < subsamples.size(); ++i)
    size += subsamples[i].clear_bytes + subsamples[i].cipher_bytes;
  return size;
}

SampleEncryption::SampleEncryption() = default;
SampleEncryption::~SampleEncryption() = default;

FourCC SampleEncryption::BoxType() const {
  return FOURCC_senc;
}

bool SampleEncryption::ReadWriteInternal(BoxBuffer* buffer) {
  RCHECK(ReadWriteHeaderInternal(buffer));

  // If we don't know |iv_size|, store sample encryption data to parse later
  // after we know iv_size.
  if (buffer->Reading() && iv_size == SampleEncryption::kInvalidIvSize) {
    RCHECK(
        buffer->ReadWriteVector(&sample_encryption_data, buffer->BytesLeft()));
    return true;
  }

  if (!IsIvSizeValid(iv_size)) {
    LOG(ERROR)
        << "IV_size can only be 8 or 16 or 0 for constant iv, but seeing "
        << iv_size;
    return false;
  }

  uint32_t sample_count =
      static_cast<uint32_t>(sample_encryption_entries.size());
  RCHECK(buffer->ReadWriteUInt32(&sample_count));

  sample_encryption_entries.resize(sample_count);
  for (auto& sample_encryption_entry : sample_encryption_entries) {
    RCHECK(sample_encryption_entry.ReadWrite(
               iv_size, (flags & kUseSubsampleEncryption) != 0, buffer) != 0);
  }
  return true;
}

size_t SampleEncryption::ComputeSizeInternal() {
  const uint32_t sample_count =
      static_cast<uint32_t>(sample_encryption_entries.size());
  if (sample_count == 0) {
    // Sample encryption box is optional. Skip it if it is empty.
    return 0;
  }

  DCHECK(IsIvSizeValid(iv_size));
  size_t box_size = HeaderSize() + sizeof(sample_count);
  if (flags & kUseSubsampleEncryption) {
    for (const SampleEncryptionEntry& sample_encryption_entry :
         sample_encryption_entries) {
      box_size += sample_encryption_entry.ComputeSize();
    }
  } else {
    box_size += sample_count * iv_size;
  }
  return box_size;
}

bool SampleEncryption::ParseFromSampleEncryptionData(
    uint8_t iv_size,
    std::vector<SampleEncryptionEntry>* sample_encryption_entries) const {
  DCHECK(IsIvSizeValid(iv_size));

  BufferReader reader(sample_encryption_data.data(),
                      sample_encryption_data.size());
  uint32_t sample_count = 0;
  RCHECK(reader.Read4(&sample_count));

  sample_encryption_entries->resize(sample_count);
  for (auto& sample_encryption_entry : *sample_encryption_entries) {
    RCHECK(sample_encryption_entry.ParseFromBuffer(
               iv_size, (flags & kUseSubsampleEncryption) != 0, &reader) != 0);
  }
  return true;
}

OriginalFormat::OriginalFormat() = default;
OriginalFormat::~OriginalFormat() = default;

FourCC OriginalFormat::BoxType() const {
  return FOURCC_frma;
}

bool OriginalFormat::ReadWriteInternal(BoxBuffer* buffer) {
  return ReadWriteHeaderInternal(buffer) && buffer->ReadWriteFourCC(&format);
}

size_t OriginalFormat::ComputeSizeInternal() {
  return HeaderSize() + kFourCCSize;
}

SchemeType::SchemeType() = default;
SchemeType::~SchemeType() = default;

FourCC SchemeType::BoxType() const {
  return FOURCC_schm;
}

bool SchemeType::ReadWriteInternal(BoxBuffer* buffer) {
  RCHECK(ReadWriteHeaderInternal(buffer) && buffer->ReadWriteFourCC(&type) &&
         buffer->ReadWriteUInt32(&version));
  return true;
}

size_t SchemeType::ComputeSizeInternal() {
  return HeaderSize() + kFourCCSize + sizeof(version);
}

TrackEncryption::TrackEncryption() = default;
TrackEncryption::~TrackEncryption() = default;

FourCC TrackEncryption::BoxType() const {
  return FOURCC_tenc;
}

bool TrackEncryption::ReadWriteInternal(BoxBuffer* buffer) {
  if (!buffer->Reading()) {
    if (default_kid.size() != kCencKeyIdSize) {
      LOG(WARNING) << "CENC defines key id length of " << kCencKeyIdSize
                   << " bytes; got " << default_kid.size()
                   << ". Resized accordingly.";
      default_kid.resize(kCencKeyIdSize);
    }
    RCHECK(default_crypt_byte_block < 16 && default_skip_byte_block < 16);
    if (default_crypt_byte_block != 0 && default_skip_byte_block != 0) {
      // Version 1 box is needed for pattern-based encryption.
      version = 1;
    }
  }

  RCHECK(ReadWriteHeaderInternal(buffer) &&
         buffer->IgnoreBytes(1));  // reserved.

  uint8_t pattern = default_crypt_byte_block << 4 | default_skip_byte_block;
  RCHECK(buffer->ReadWriteUInt8(&pattern));
  default_crypt_byte_block = pattern >> 4;
  default_skip_byte_block = pattern & 0x0F;

  RCHECK(buffer->ReadWriteUInt8(&default_is_protected) &&
         buffer->ReadWriteUInt8(&default_per_sample_iv_size) &&
         buffer->ReadWriteVector(&default_kid, kCencKeyIdSize));

  if (default_is_protected == 1) {
    if (default_per_sample_iv_size == 0) {  // For constant iv.
      uint8_t default_constant_iv_size =
          static_cast<uint8_t>(default_constant_iv.size());
      RCHECK(buffer->ReadWriteUInt8(&default_constant_iv_size));
      RCHECK(default_constant_iv_size == 8 || default_constant_iv_size == 16);
      RCHECK(buffer->ReadWriteVector(&default_constant_iv,
                                     default_constant_iv_size));
    } else {
      RCHECK(default_per_sample_iv_size == 8 ||
             default_per_sample_iv_size == 16);
      RCHECK(default_constant_iv.empty());
    }
  } else {
    // Expect |default_is_protected| to be 0, i.e. not protected. Other values
    // of |default_is_protected| is not supported.
    RCHECK(default_is_protected == 0);
    RCHECK(default_per_sample_iv_size == 0);
    RCHECK(default_constant_iv.empty());
  }
  return true;
}

size_t TrackEncryption::ComputeSizeInternal() {
  return HeaderSize() + sizeof(uint32_t) + kCencKeyIdSize +
         (default_constant_iv.empty()
              ? 0
              : (sizeof(uint8_t) + default_constant_iv.size()));
}

SchemeInfo::SchemeInfo() = default;
SchemeInfo::~SchemeInfo() = default;

FourCC SchemeInfo::BoxType() const {
  return FOURCC_schi;
}

bool SchemeInfo::ReadWriteInternal(BoxBuffer* buffer) {
  RCHECK(ReadWriteHeaderInternal(buffer) && buffer->PrepareChildren() &&
         buffer->ReadWriteChild(&track_encryption));
  return true;
}

size_t SchemeInfo::ComputeSizeInternal() {
  return HeaderSize() + track_encryption.ComputeSize();
}

ProtectionSchemeInfo::ProtectionSchemeInfo() = default;
ProtectionSchemeInfo::~ProtectionSchemeInfo() = default;

FourCC ProtectionSchemeInfo::BoxType() const {
  return FOURCC_sinf;
}

bool ProtectionSchemeInfo::ReadWriteInternal(BoxBuffer* buffer) {
  RCHECK(ReadWriteHeaderInternal(buffer) && buffer->PrepareChildren() &&
         buffer->ReadWriteChild(&format) && buffer->ReadWriteChild(&type));
  if (IsProtectionSchemeSupported(type.type)) {
    RCHECK(buffer->ReadWriteChild(&info));
  } else {
    DLOG(WARNING) << "Ignore unsupported protection scheme: "
                  << FourCCToString(type.type);
  }
  // Other protection schemes are silently ignored. Since the protection scheme
  // type can't be determined until this box is opened, we return 'true' for
  // non-CENC protection scheme types. It is the parent box's responsibility to
  // ensure that this scheme type is a supported one.
  return true;
}

size_t ProtectionSchemeInfo::ComputeSizeInternal() {
  // Skip sinf box if it is not initialized.
  if (format.format == FOURCC_NULL)
    return 0;
  return HeaderSize() + format.ComputeSize() + type.ComputeSize() +
         info.ComputeSize();
}

MovieHeader::MovieHeader() = default;
MovieHeader::~MovieHeader() = default;

FourCC MovieHeader::BoxType() const {
  return FOURCC_mvhd;
}

bool MovieHeader::ReadWriteInternal(BoxBuffer* buffer) {
  RCHECK(ReadWriteHeaderInternal(buffer));

  size_t num_bytes = (version == 1) ? sizeof(uint64_t) : sizeof(uint32_t);
  RCHECK(buffer->ReadWriteUInt64NBytes(&creation_time, num_bytes) &&
         buffer->ReadWriteUInt64NBytes(&modification_time, num_bytes) &&
         buffer->ReadWriteUInt32(&timescale) &&
         buffer->ReadWriteUInt64NBytes(&duration, num_bytes));

  std::vector<uint8_t> matrix(kUnityMatrix,
                              kUnityMatrix + arraysize(kUnityMatrix));
  RCHECK(buffer->ReadWriteInt32(&rate) && buffer->ReadWriteInt16(&volume) &&
         buffer->IgnoreBytes(10) &&  // reserved
         buffer->ReadWriteVector(&matrix, matrix.size()) &&
         buffer->IgnoreBytes(24) &&  // predefined zero
         buffer->ReadWriteUInt32(&next_track_id));
  return true;
}

size_t MovieHeader::ComputeSizeInternal() {
  version = IsFitIn32Bits(creation_time, modification_time, duration) ? 0 : 1;
  return HeaderSize() + sizeof(uint32_t) * (1 + version) * 3 +
         sizeof(timescale) + sizeof(rate) + sizeof(volume) +
         sizeof(next_track_id) + sizeof(kUnityMatrix) + 10 +
         24;  // 10 bytes reserved, 24 bytes predefined.
}

TrackHeader::TrackHeader() {
  flags = kTrackEnabled | kTrackInMovie | kTrackInPreview;
}

TrackHeader::~TrackHeader() = default;

FourCC TrackHeader::BoxType() const {
  return FOURCC_tkhd;
}

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
         buffer->ReadWriteUInt32(&width) && buffer->ReadWriteUInt32(&height));
  return true;
}

size_t TrackHeader::ComputeSizeInternal() {
  version = IsFitIn32Bits(creation_time, modification_time, duration) ? 0 : 1;
  return HeaderSize() + sizeof(track_id) +
         sizeof(uint32_t) * (1 + version) * 3 + sizeof(layer) +
         sizeof(alternate_group) + sizeof(volume) + sizeof(width) +
         sizeof(height) + sizeof(kUnityMatrix) + 14;  // 14 bytes reserved.
}

SampleDescription::SampleDescription() = default;
SampleDescription::~SampleDescription() = default;

FourCC SampleDescription::BoxType() const {
  return FOURCC_stsd;
}

bool SampleDescription::ReadWriteInternal(BoxBuffer* buffer) {
  uint32_t count = 0;
  switch (type) {
    case kVideo:
      count = static_cast<uint32_t>(video_entries.size());
      break;
    case kAudio:
      count = static_cast<uint32_t>(audio_entries.size());
      break;
    case kText:
    case kSubtitle:
      count = static_cast<uint32_t>(text_entries.size());
      break;
    default:
      NOTIMPLEMENTED() << "SampleDecryption type " << type
                       << " is not handled. Skipping.";
  }
  RCHECK(ReadWriteHeaderInternal(buffer) && buffer->ReadWriteUInt32(&count));

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
    } else if (type == kText || type == kSubtitle) {
      RCHECK(reader->ReadAllChildren(&text_entries));
      RCHECK(text_entries.size() == count);
    }
  } else {
    DCHECK_LT(0u, count);
    if (type == kVideo) {
      for (uint32_t i = 0; i < count; ++i)
        RCHECK(buffer->ReadWriteChild(&video_entries[i]));
    } else if (type == kAudio) {
      for (uint32_t i = 0; i < count; ++i)
        RCHECK(buffer->ReadWriteChild(&audio_entries[i]));
    } else if (type == kText || type == kSubtitle) {
      for (uint32_t i = 0; i < count; ++i)
        RCHECK(buffer->ReadWriteChild(&text_entries[i]));
    } else {
      NOTIMPLEMENTED();
    }
  }
  return true;
}

size_t SampleDescription::ComputeSizeInternal() {
  size_t box_size = HeaderSize() + sizeof(uint32_t);
  if (type == kVideo) {
    for (uint32_t i = 0; i < video_entries.size(); ++i)
      box_size += video_entries[i].ComputeSize();
  } else if (type == kAudio) {
    for (uint32_t i = 0; i < audio_entries.size(); ++i)
      box_size += audio_entries[i].ComputeSize();
  } else if (type == kText || type == kSubtitle) {
    for (uint32_t i = 0; i < text_entries.size(); ++i)
      box_size += text_entries[i].ComputeSize();
  }
  return box_size;
}

DecodingTimeToSample::DecodingTimeToSample() = default;
DecodingTimeToSample::~DecodingTimeToSample() = default;

FourCC DecodingTimeToSample::BoxType() const {
  return FOURCC_stts;
}

bool DecodingTimeToSample::ReadWriteInternal(BoxBuffer* buffer) {
  uint32_t count = static_cast<uint32_t>(decoding_time.size());
  RCHECK(ReadWriteHeaderInternal(buffer) && buffer->ReadWriteUInt32(&count));

  decoding_time.resize(count);
  for (uint32_t i = 0; i < count; ++i) {
    RCHECK(buffer->ReadWriteUInt32(&decoding_time[i].sample_count) &&
           buffer->ReadWriteUInt32(&decoding_time[i].sample_delta));
  }
  return true;
}

size_t DecodingTimeToSample::ComputeSizeInternal() {
  return HeaderSize() + sizeof(uint32_t) +
         sizeof(DecodingTime) * decoding_time.size();
}

CompositionTimeToSample::CompositionTimeToSample() = default;
CompositionTimeToSample::~CompositionTimeToSample() = default;

FourCC CompositionTimeToSample::BoxType() const {
  return FOURCC_ctts;
}

bool CompositionTimeToSample::ReadWriteInternal(BoxBuffer* buffer) {
  uint32_t count = static_cast<uint32_t>(composition_offset.size());
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

  RCHECK(ReadWriteHeaderInternal(buffer) && buffer->ReadWriteUInt32(&count));

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

size_t CompositionTimeToSample::ComputeSizeInternal() {
  // This box is optional. Skip it if it is empty.
  if (composition_offset.empty())
    return 0;
  // Structure CompositionOffset contains |sample_offset| (uint32_t) and
  // |sample_offset| (int64_t). The actual size of |sample_offset| is
  // 4 bytes (uint32_t for version 0 and int32_t for version 1).
  const size_t kCompositionOffsetSize = sizeof(uint32_t) * 2;
  return HeaderSize() + sizeof(uint32_t) +
         kCompositionOffsetSize * composition_offset.size();
}

SampleToChunk::SampleToChunk() = default;
SampleToChunk::~SampleToChunk() = default;

FourCC SampleToChunk::BoxType() const {
  return FOURCC_stsc;
}

bool SampleToChunk::ReadWriteInternal(BoxBuffer* buffer) {
  uint32_t count = static_cast<uint32_t>(chunk_info.size());
  RCHECK(ReadWriteHeaderInternal(buffer) && buffer->ReadWriteUInt32(&count));

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

size_t SampleToChunk::ComputeSizeInternal() {
  return HeaderSize() + sizeof(uint32_t) +
         sizeof(ChunkInfo) * chunk_info.size();
}

SampleSize::SampleSize() = default;
SampleSize::~SampleSize() = default;

FourCC SampleSize::BoxType() const {
  return FOURCC_stsz;
}

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

size_t SampleSize::ComputeSizeInternal() {
  return HeaderSize() + sizeof(sample_size) + sizeof(sample_count) +
         (sample_size == 0 ? sizeof(uint32_t) * sizes.size() : 0);
}

CompactSampleSize::CompactSampleSize() = default;
CompactSampleSize::~CompactSampleSize() = default;

FourCC CompactSampleSize::BoxType() const {
  return FOURCC_stz2;
}

bool CompactSampleSize::ReadWriteInternal(BoxBuffer* buffer) {
  uint32_t sample_count = static_cast<uint32_t>(sizes.size());
  RCHECK(ReadWriteHeaderInternal(buffer) && buffer->IgnoreBytes(3) &&
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

size_t CompactSampleSize::ComputeSizeInternal() {
  return HeaderSize() + sizeof(uint32_t) + sizeof(uint32_t) +
         (field_size * sizes.size() + 7) / 8;
}

ChunkOffset::ChunkOffset() = default;
ChunkOffset::~ChunkOffset() = default;

FourCC ChunkOffset::BoxType() const {
  return FOURCC_stco;
}

bool ChunkOffset::ReadWriteInternal(BoxBuffer* buffer) {
  uint32_t count = static_cast<uint32_t>(offsets.size());
  RCHECK(ReadWriteHeaderInternal(buffer) && buffer->ReadWriteUInt32(&count));

  offsets.resize(count);
  for (uint32_t i = 0; i < count; ++i)
    RCHECK(buffer->ReadWriteUInt64NBytes(&offsets[i], sizeof(uint32_t)));
  return true;
}

size_t ChunkOffset::ComputeSizeInternal() {
  return HeaderSize() + sizeof(uint32_t) + sizeof(uint32_t) * offsets.size();
}

ChunkLargeOffset::ChunkLargeOffset() = default;
ChunkLargeOffset::~ChunkLargeOffset() = default;

FourCC ChunkLargeOffset::BoxType() const {
  return FOURCC_co64;
}

bool ChunkLargeOffset::ReadWriteInternal(BoxBuffer* buffer) {
  uint32_t count = static_cast<uint32_t>(offsets.size());

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

  RCHECK(ReadWriteHeaderInternal(buffer) && buffer->ReadWriteUInt32(&count));

  offsets.resize(count);
  for (uint32_t i = 0; i < count; ++i)
    RCHECK(buffer->ReadWriteUInt64(&offsets[i]));
  return true;
}

size_t ChunkLargeOffset::ComputeSizeInternal() {
  uint32_t count = static_cast<uint32_t>(offsets.size());
  int use_large_offset =
      (count > 0 && !IsFitIn32Bits(offsets[count - 1])) ? 1 : 0;
  return HeaderSize() + sizeof(count) +
         sizeof(uint32_t) * (1 + use_large_offset) * offsets.size();
}

SyncSample::SyncSample() = default;
SyncSample::~SyncSample() = default;

FourCC SyncSample::BoxType() const {
  return FOURCC_stss;
}

bool SyncSample::ReadWriteInternal(BoxBuffer* buffer) {
  uint32_t count = static_cast<uint32_t>(sample_number.size());
  RCHECK(ReadWriteHeaderInternal(buffer) && buffer->ReadWriteUInt32(&count));

  sample_number.resize(count);
  for (uint32_t i = 0; i < count; ++i)
    RCHECK(buffer->ReadWriteUInt32(&sample_number[i]));
  return true;
}

size_t SyncSample::ComputeSizeInternal() {
  // Sync sample box is optional. Skip it if it is empty.
  if (sample_number.empty())
    return 0;
  return HeaderSize() + sizeof(uint32_t) +
         sizeof(uint32_t) * sample_number.size();
}

bool CencSampleEncryptionInfoEntry::ReadWrite(BoxBuffer* buffer) {
  if (!buffer->Reading()) {
    if (key_id.size() != kCencKeyIdSize) {
      LOG(WARNING) << "CENC defines key id length of " << kCencKeyIdSize
                   << " bytes; got " << key_id.size()
                   << ". Resized accordingly.";
      key_id.resize(kCencKeyIdSize);
    }
    RCHECK(crypt_byte_block < 16 && skip_byte_block < 16);
  }

  RCHECK(buffer->IgnoreBytes(1));  // reserved.

  uint8_t pattern = crypt_byte_block << 4 | skip_byte_block;
  RCHECK(buffer->ReadWriteUInt8(&pattern));
  crypt_byte_block = pattern >> 4;
  skip_byte_block = pattern & 0x0F;

  RCHECK(buffer->ReadWriteUInt8(&is_protected) &&
         buffer->ReadWriteUInt8(&per_sample_iv_size) &&
         buffer->ReadWriteVector(&key_id, kCencKeyIdSize));

  if (is_protected == 1) {
    if (per_sample_iv_size == 0) {  // For constant iv.
      uint8_t constant_iv_size = static_cast<uint8_t>(constant_iv.size());
      RCHECK(buffer->ReadWriteUInt8(&constant_iv_size));
      RCHECK(constant_iv_size == 8 || constant_iv_size == 16);
      RCHECK(buffer->ReadWriteVector(&constant_iv, constant_iv_size));
    } else {
      RCHECK(per_sample_iv_size == 8 || per_sample_iv_size == 16);
      DCHECK(constant_iv.empty());
    }
  } else {
    // Expect |is_protected| to be 0, i.e. not protected. Other values of
    // |is_protected| is not supported.
    RCHECK(is_protected == 0);
    RCHECK(per_sample_iv_size == 0);
  }
  return true;
}

uint32_t CencSampleEncryptionInfoEntry::ComputeSize() const {
  return static_cast<uint32_t>(
      sizeof(uint32_t) + kCencKeyIdSize +
      (constant_iv.empty() ? 0 : (sizeof(uint8_t) + constant_iv.size())));
}

bool AudioRollRecoveryEntry::ReadWrite(BoxBuffer* buffer) {
  RCHECK(buffer->ReadWriteInt16(&roll_distance));
  return true;
}

uint32_t AudioRollRecoveryEntry::ComputeSize() const {
  return sizeof(roll_distance);
}

SampleGroupDescription::SampleGroupDescription() = default;
SampleGroupDescription::~SampleGroupDescription() = default;

FourCC SampleGroupDescription::BoxType() const {
  return FOURCC_sgpd;
}

bool SampleGroupDescription::ReadWriteInternal(BoxBuffer* buffer) {
  RCHECK(ReadWriteHeaderInternal(buffer) &&
         buffer->ReadWriteUInt32(&grouping_type));

  switch (grouping_type) {
    case FOURCC_seig:
      return ReadWriteEntries(buffer, &cenc_sample_encryption_info_entries);
    case FOURCC_roll:
      return ReadWriteEntries(buffer, &audio_roll_recovery_entries);
    default:
      DCHECK(buffer->Reading());
      DLOG(WARNING) << "Ignore unsupported sample group: "
                    << FourCCToString(static_cast<FourCC>(grouping_type));
      return true;
  }
}

template <typename T>
bool SampleGroupDescription::ReadWriteEntries(BoxBuffer* buffer,
                                              std::vector<T>* entries) {
  uint32_t default_length = 0;
  if (!buffer->Reading()) {
    DCHECK(!entries->empty());
    default_length = (*entries)[0].ComputeSize();
    DCHECK_NE(default_length, 0u);
  }
  if (version == 1)
    RCHECK(buffer->ReadWriteUInt32(&default_length));
  if (version >= 2) {
    NOTIMPLEMENTED() << "Unsupported SampleGroupDescriptionBox 'sgpd' version "
                     << static_cast<int>(version);
    return false;
  }

  uint32_t count = static_cast<uint32_t>(entries->size());
  RCHECK(buffer->ReadWriteUInt32(&count));
  if (buffer->Reading()) {
    if (count == 0)
      return true;
  } else {
    RCHECK(count != 0);
  }
  entries->resize(count);

  for (T& entry : *entries) {
    if (version == 1) {
      uint32_t description_length = default_length;
      if (buffer->Reading() && default_length == 0)
        RCHECK(buffer->ReadWriteUInt32(&description_length));
      RCHECK(entry.ReadWrite(buffer));
      RCHECK(entry.ComputeSize() == description_length);
    } else {
      RCHECK(entry.ReadWrite(buffer));
    }
  }
  return true;
}

size_t SampleGroupDescription::ComputeSizeInternal() {
  // Version 0 is obsoleted, so always generate version 1 box.
  version = 1;
  size_t entries_size = 0;
  switch (grouping_type) {
    case FOURCC_seig:
      for (const auto& entry : cenc_sample_encryption_info_entries)
        entries_size += entry.ComputeSize();
      break;
    case FOURCC_roll:
      for (const auto& entry : audio_roll_recovery_entries)
        entries_size += entry.ComputeSize();
      break;
  }
  // This box is optional. Skip it if it is not used.
  if (entries_size == 0)
    return 0;
  return HeaderSize() + sizeof(grouping_type) +
         (version == 1 ? sizeof(uint32_t) : 0) + sizeof(uint32_t) +
         entries_size;
}

SampleToGroup::SampleToGroup() = default;
SampleToGroup::~SampleToGroup() = default;

FourCC SampleToGroup::BoxType() const {
  return FOURCC_sbgp;
}

bool SampleToGroup::ReadWriteInternal(BoxBuffer* buffer) {
  RCHECK(ReadWriteHeaderInternal(buffer) &&
         buffer->ReadWriteUInt32(&grouping_type));
  if (version == 1)
    RCHECK(buffer->ReadWriteUInt32(&grouping_type_parameter));

  if (grouping_type != FOURCC_seig && grouping_type != FOURCC_roll) {
    DCHECK(buffer->Reading());
    DLOG(WARNING) << "Ignore unsupported sample group: "
                  << FourCCToString(static_cast<FourCC>(grouping_type));
    return true;
  }

  uint32_t count = static_cast<uint32_t>(entries.size());
  RCHECK(buffer->ReadWriteUInt32(&count));
  entries.resize(count);
  for (uint32_t i = 0; i < count; ++i) {
    RCHECK(buffer->ReadWriteUInt32(&entries[i].sample_count) &&
           buffer->ReadWriteUInt32(&entries[i].group_description_index));
  }
  return true;
}

size_t SampleToGroup::ComputeSizeInternal() {
  // This box is optional. Skip it if it is not used.
  if (entries.empty())
    return 0;
  return HeaderSize() + sizeof(grouping_type) +
         (version == 1 ? sizeof(grouping_type_parameter) : 0) +
         sizeof(uint32_t) + entries.size() * sizeof(entries[0]);
}

SampleTable::SampleTable() = default;
SampleTable::~SampleTable() = default;

FourCC SampleTable::BoxType() const {
  return FOURCC_stbl;
}

bool SampleTable::ReadWriteInternal(BoxBuffer* buffer) {
  RCHECK(ReadWriteHeaderInternal(buffer) && buffer->PrepareChildren() &&
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
      sample_size.sample_count =
          static_cast<uint32_t>(compact_sample_size.sizes.size());
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
  if (buffer->Reading()) {
    RCHECK(buffer->reader()->TryReadChildren(&sample_group_descriptions) &&
           buffer->reader()->TryReadChildren(&sample_to_groups));
  } else {
    for (auto& sample_group_description : sample_group_descriptions)
      RCHECK(buffer->ReadWriteChild(&sample_group_description));
    for (auto& sample_to_group : sample_to_groups)
      RCHECK(buffer->ReadWriteChild(&sample_to_group));
  }
  return true;
}

size_t SampleTable::ComputeSizeInternal() {
  size_t box_size = HeaderSize() + description.ComputeSize() +
                    decoding_time_to_sample.ComputeSize() +
                    composition_time_to_sample.ComputeSize() +
                    sample_to_chunk.ComputeSize() + sample_size.ComputeSize() +
                    chunk_large_offset.ComputeSize() +
                    sync_sample.ComputeSize();
  for (auto& sample_group_description : sample_group_descriptions)
    box_size += sample_group_description.ComputeSize();
  for (auto& sample_to_group : sample_to_groups)
    box_size += sample_to_group.ComputeSize();
  return box_size;
}

EditList::EditList() = default;
EditList::~EditList() = default;

FourCC EditList::BoxType() const {
  return FOURCC_elst;
}

bool EditList::ReadWriteInternal(BoxBuffer* buffer) {
  uint32_t count = static_cast<uint32_t>(edits.size());
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

size_t EditList::ComputeSizeInternal() {
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

Edit::Edit() = default;
Edit::~Edit() = default;

FourCC Edit::BoxType() const {
  return FOURCC_edts;
}

bool Edit::ReadWriteInternal(BoxBuffer* buffer) {
  return ReadWriteHeaderInternal(buffer) && buffer->PrepareChildren() &&
         buffer->ReadWriteChild(&list);
}

size_t Edit::ComputeSizeInternal() {
  // Edit box is optional. Skip it if it is empty.
  if (list.edits.empty())
    return 0;
  return HeaderSize() + list.ComputeSize();
}

HandlerReference::HandlerReference() = default;
HandlerReference::~HandlerReference() = default;

FourCC HandlerReference::BoxType() const {
  return FOURCC_hdlr;
}

bool HandlerReference::ReadWriteInternal(BoxBuffer* buffer) {
  std::vector<uint8_t> handler_name;
  if (!buffer->Reading()) {
    switch (handler_type) {
      case FOURCC_vide:
        handler_name.assign(kVideoHandlerName,
                            kVideoHandlerName + arraysize(kVideoHandlerName));
        break;
      case FOURCC_soun:
        handler_name.assign(kAudioHandlerName,
                            kAudioHandlerName + arraysize(kAudioHandlerName));
        break;
      case FOURCC_text:
        handler_name.assign(kTextHandlerName,
                            kTextHandlerName + arraysize(kTextHandlerName));
        break;
      case FOURCC_subt:
        handler_name.assign(
            kSubtitleHandlerName,
            kSubtitleHandlerName + arraysize(kSubtitleHandlerName));
        break;
      case FOURCC_ID32:
        break;
      default:
        NOTIMPLEMENTED();
        return false;
    }
  }
  RCHECK(ReadWriteHeaderInternal(buffer) &&
         buffer->IgnoreBytes(4) &&  // predefined.
         buffer->ReadWriteFourCC(&handler_type));
  if (!buffer->Reading()) {
    RCHECK(buffer->IgnoreBytes(12) &&  // reserved.
           buffer->ReadWriteVector(&handler_name, handler_name.size()));
  }
  return true;
}

size_t HandlerReference::ComputeSizeInternal() {
  size_t box_size = HeaderSize() + kFourCCSize + 16;  // 16 bytes Reserved
  switch (handler_type) {
    case FOURCC_vide:
      box_size += sizeof(kVideoHandlerName);
      break;
    case FOURCC_soun:
      box_size += sizeof(kAudioHandlerName);
      break;
    case FOURCC_text:
      box_size += sizeof(kTextHandlerName);
      break;
    case FOURCC_subt:
      box_size += sizeof(kSubtitleHandlerName);
      break;
    case FOURCC_ID32:
      break;
    default:
      NOTIMPLEMENTED();
  }
  return box_size;
}

bool Language::ReadWrite(BoxBuffer* buffer) {
  if (buffer->Reading()) {
    // Read language codes into temp first then use BitReader to read the
    // values. ISO-639-2/T language code: unsigned int(5)[3] language (2 bytes).
    std::vector<uint8_t> temp;
    RCHECK(buffer->ReadWriteVector(&temp, 2));

    BitReader bit_reader(&temp[0], 2);
    bit_reader.SkipBits(1);
    char language[3];
    for (int i = 0; i < 3; ++i) {
      CHECK(bit_reader.ReadBits(5, &language[i]));
      language[i] += 0x60;
    }
    code.assign(language, 3);
  } else {
    // Set up default language if it is not set.
    const char kUndefinedLanguage[] = "und";
    if (code.empty())
      code = kUndefinedLanguage;
    DCHECK_EQ(code.size(), 3u);

    // Lang format: bit(1) pad, unsigned int(5)[3] language.
    uint16_t lang = 0;
    for (int i = 0; i < 3; ++i)
      lang |= (code[i] - 0x60) << ((2 - i) * 5);
    RCHECK(buffer->ReadWriteUInt16(&lang));
  }
  return true;
}

uint32_t Language::ComputeSize() const {
  // ISO-639-2/T language code: unsigned int(5)[3] language (2 bytes).
  return 2;
}

ID3v2::ID3v2() = default;
ID3v2::~ID3v2() = default;

FourCC ID3v2::BoxType() const {
  return FOURCC_ID32;
}

bool ID3v2::ReadWriteInternal(BoxBuffer* buffer) {
  RCHECK(ReadWriteHeaderInternal(buffer) && language.ReadWrite(buffer) &&
         buffer->ReadWriteVector(&id3v2_data, buffer->Reading()
                                                  ? buffer->BytesLeft()
                                                  : id3v2_data.size()));
  return true;
}

size_t ID3v2::ComputeSizeInternal() {
  // Skip ID3v2 box generation if there is no id3 data.
  return id3v2_data.size() == 0
             ? 0
             : HeaderSize() + language.ComputeSize() + id3v2_data.size();
}

Metadata::Metadata() = default;
Metadata::~Metadata() = default;

FourCC Metadata::BoxType() const {
  return FOURCC_meta;
}

bool Metadata::ReadWriteInternal(BoxBuffer* buffer) {
  RCHECK(ReadWriteHeaderInternal(buffer) && buffer->PrepareChildren() &&
         buffer->ReadWriteChild(&handler) && buffer->TryReadWriteChild(&id3v2));
  return true;
}

size_t Metadata::ComputeSizeInternal() {
  size_t id3v2_size = id3v2.ComputeSize();
  // Skip metadata box generation if there is no metadata box.
  return id3v2_size == 0 ? 0
                         : HeaderSize() + handler.ComputeSize() + id3v2_size;
}

CodecConfiguration::CodecConfiguration() = default;
CodecConfiguration::~CodecConfiguration() = default;

FourCC CodecConfiguration::BoxType() const {
  // CodecConfiguration box should be parsed according to format recovered in
  // VideoSampleEntry. |box_type| is determined dynamically there.
  return box_type;
}

bool CodecConfiguration::ReadWriteInternal(BoxBuffer* buffer) {
  DCHECK_NE(box_type, FOURCC_NULL);
  RCHECK(ReadWriteHeaderInternal(buffer));

  // VPCodecConfiguration box inherits from FullBox instead of Box. The extra 4
  // bytes are handled here.
  if (box_type == FOURCC_vpcC) {
    // Only version 1 box is supported.
    uint8_t vpcc_version = 1;
    uint32_t version_flags = vpcc_version << 24;
    RCHECK(buffer->ReadWriteUInt32(&version_flags));
    vpcc_version = version_flags >> 24;
    RCHECK(vpcc_version == 1);
  }

  if (buffer->Reading()) {
    RCHECK(buffer->ReadWriteVector(&data, buffer->BytesLeft()));
  } else {
    RCHECK(buffer->ReadWriteVector(&data, data.size()));
  }
  return true;
}

size_t CodecConfiguration::ComputeSizeInternal() {
  if (data.empty())
    return 0;
  DCHECK_NE(box_type, FOURCC_NULL);
  return HeaderSize() + (box_type == FOURCC_vpcC ? 4 : 0) + data.size();
}

ColorParameters::ColorParameters() = default;
ColorParameters::~ColorParameters() = default;

FourCC ColorParameters::BoxType() const {
  return FOURCC_colr;
}

bool ColorParameters::ReadWriteInternal(BoxBuffer* buffer) {
  if (buffer->Reading()) {
    BoxReader* reader = buffer->reader();
    DCHECK(reader);

    // Parse and store the raw box for colr atom preservation in the output mp4.
    raw_box.assign(reader->data(), reader->data() + reader->size());

    // Parse individual parameters for full codec string formation.
    RCHECK(reader->ReadFourCC(&color_parameter_type) &&
           reader->Read2(&color_primaries) &&
           reader->Read2(&transfer_characteristics) &&
           reader->Read2(&matrix_coefficients));
    // Type nclc does not contain video_full_range_flag data, and thus, it has 1
    // less byte than nclx. Only extract video_full_range_flag if of type nclx.
    if (color_parameter_type == FOURCC_nclx) {
      RCHECK(reader->Read1(&video_full_range_flag));
    }
  } else {
    // When writing, only need to write the raw_box.
    DCHECK(!raw_box.empty());
    buffer->writer()->AppendVector(raw_box);
  }
  return true;
}

size_t ColorParameters::ComputeSizeInternal() {
  return raw_box.size();
}

PixelAspectRatio::PixelAspectRatio() = default;
PixelAspectRatio::~PixelAspectRatio() = default;

FourCC PixelAspectRatio::BoxType() const {
  return FOURCC_pasp;
}

bool PixelAspectRatio::ReadWriteInternal(BoxBuffer* buffer) {
  RCHECK(ReadWriteHeaderInternal(buffer) &&
         buffer->ReadWriteUInt32(&h_spacing) &&
         buffer->ReadWriteUInt32(&v_spacing));
  return true;
}

size_t PixelAspectRatio::ComputeSizeInternal() {
  // This box is optional. Skip it if it is not initialized.
  if (h_spacing == 0 && v_spacing == 0)
    return 0;
  // Both values must be positive.
  DCHECK(h_spacing != 0 && v_spacing != 0);
  return HeaderSize() + sizeof(h_spacing) + sizeof(v_spacing);
}

VideoSampleEntry::VideoSampleEntry() = default;
VideoSampleEntry::~VideoSampleEntry() = default;

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
      case FOURCC_av01:
        compressor_name.assign(std::begin(kAv1CompressorName),
                               std::end(kAv1CompressorName));
        break;
      case FOURCC_avc1:
      case FOURCC_avc3:
        compressor_name.assign(std::begin(kAvcCompressorName),
                               std::end(kAvcCompressorName));
        break;
      case FOURCC_dvh1:
      case FOURCC_dvhe:
        compressor_name.assign(std::begin(kDolbyVisionCompressorName),
                               std::end(kDolbyVisionCompressorName));
        break;
      case FOURCC_hev1:
      case FOURCC_hvc1:
        compressor_name.assign(std::begin(kHevcCompressorName),
                               std::end(kHevcCompressorName));
        break;
      case FOURCC_vp08:
      case FOURCC_vp09:
        compressor_name.assign(std::begin(kVpcCompressorName),
                               std::end(kVpcCompressorName));
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
         buffer->ReadWriteUInt16(&width) && buffer->ReadWriteUInt16(&height) &&
         buffer->ReadWriteUInt32(&video_resolution) &&
         buffer->ReadWriteUInt32(&video_resolution) &&
         buffer->IgnoreBytes(4) &&  // reserved.
         buffer->ReadWriteUInt16(&video_frame_count) &&
         buffer->ReadWriteVector(&compressor_name, kCompressorNameSize) &&
         buffer->ReadWriteUInt16(&video_depth) &&
         buffer->ReadWriteInt16(&predefined));

  RCHECK(buffer->PrepareChildren());

  // This has to happen before reading codec configuration box as the actual
  // format is read from sinf.format.format, which is needed to parse the codec
  // configuration box.
  if (format == FOURCC_encv && buffer->Reading()) {
    // Continue scanning until a supported protection scheme is found, or
    // until we run out of protection schemes.
    while (!IsProtectionSchemeSupported(sinf.type.type))
      RCHECK(buffer->ReadWriteChild(&sinf));
  }

  const FourCC actual_format = GetActualFormat();
  if (buffer->Reading()) {
    codec_configuration.box_type = GetCodecConfigurationBoxType(actual_format);
  } else {
    DCHECK_EQ(codec_configuration.box_type,
              GetCodecConfigurationBoxType(actual_format));
  }
  if (codec_configuration.box_type == FOURCC_NULL)
    return false;

  RCHECK(buffer->ReadWriteChild(&codec_configuration));

  if (buffer->Reading()) {
    extra_codec_configs.clear();
    // Handle Dolby Vision boxes.
    const bool is_hevc =
        actual_format == FOURCC_dvhe || actual_format == FOURCC_dvh1 ||
        actual_format == FOURCC_hev1 || actual_format == FOURCC_hvc1;
    if (is_hevc) {
      for (FourCC fourcc : {FOURCC_dvcC, FOURCC_dvvC, FOURCC_hvcE}) {
        CodecConfiguration dv_box;
        dv_box.box_type = fourcc;
        RCHECK(buffer->TryReadWriteChild(&dv_box));
        if (!dv_box.data.empty())
          extra_codec_configs.push_back(std::move(dv_box));
      }
    }
  } else {
    for (CodecConfiguration& extra_codec_config : extra_codec_configs)
      RCHECK(buffer->ReadWriteChild(&extra_codec_config));
  }

  RCHECK(buffer->TryReadWriteChild(&colr));
  RCHECK(buffer->TryReadWriteChild(&pixel_aspect));

  // Somehow Edge does not support having sinf box before codec_configuration,
  // box, so just do it in the end of VideoSampleEntry. See
  // https://developer.microsoft.com/en-us/microsoft-edge/platform/issues/12658991/
  if (format == FOURCC_encv && !buffer->Reading()) {
    DCHECK(IsProtectionSchemeSupported(sinf.type.type));
    RCHECK(buffer->ReadWriteChild(&sinf));
  }
  return true;
}

size_t VideoSampleEntry::ComputeSizeInternal() {
  const FourCC actual_format = GetActualFormat();
  if (actual_format == FOURCC_NULL)
    return 0;
  codec_configuration.box_type = GetCodecConfigurationBoxType(actual_format);
  DCHECK_NE(codec_configuration.box_type, FOURCC_NULL);
  size_t size = HeaderSize() + sizeof(data_reference_index) + sizeof(width) +
                sizeof(height) + sizeof(kVideoResolution) * 2 +
                sizeof(kVideoFrameCount) + sizeof(kVideoDepth) +
                colr.ComputeSize() + pixel_aspect.ComputeSize() +
                sinf.ComputeSize() + codec_configuration.ComputeSize() +
                kCompressorNameSize + 6 + 4 + 16 +
                2;  // 6 + 4 bytes reserved, 16 + 2 bytes predefined.
  for (CodecConfiguration& codec_config : extra_codec_configs)
    size += codec_config.ComputeSize();
  return size;
}

FourCC VideoSampleEntry::GetCodecConfigurationBoxType(FourCC format) const {
  switch (format) {
    case FOURCC_av01:
      return FOURCC_av1C;
    case FOURCC_avc1:
    case FOURCC_avc3:
      return FOURCC_avcC;
    case FOURCC_dvh1:
    case FOURCC_dvhe:
    case FOURCC_hev1:
    case FOURCC_hvc1:
      return FOURCC_hvcC;
    case FOURCC_vp08:
    case FOURCC_vp09:
      return FOURCC_vpcC;
    default:
      LOG(ERROR) << FourCCToString(format) << " is not supported.";
      return FOURCC_NULL;
  }
}

std::vector<uint8_t> VideoSampleEntry::ExtraCodecConfigsAsVector() const {
  BufferWriter buffer;
  for (CodecConfiguration codec_config : extra_codec_configs)
    codec_config.Write(&buffer);
  return std::vector<uint8_t>(buffer.Buffer(), buffer.Buffer() + buffer.Size());
}

bool VideoSampleEntry::ParseExtraCodecConfigsVector(
    const std::vector<uint8_t>& data) {
  extra_codec_configs.clear();
  size_t pos = 0;
  while (pos < data.size()) {
    bool err = false;
    std::unique_ptr<BoxReader> box_reader(
        BoxReader::ReadBox(data.data() + pos, data.size() - pos, &err));
    RCHECK(!err && box_reader);

    CodecConfiguration codec_config;
    codec_config.box_type = box_reader->type();
    RCHECK(codec_config.Parse(box_reader.get()));
    extra_codec_configs.push_back(std::move(codec_config));

    pos += box_reader->pos();
  }
  return true;
}

ElementaryStreamDescriptor::ElementaryStreamDescriptor() = default;
ElementaryStreamDescriptor::~ElementaryStreamDescriptor() = default;

FourCC ElementaryStreamDescriptor::BoxType() const {
  return FOURCC_esds;
}

bool ElementaryStreamDescriptor::ReadWriteInternal(BoxBuffer* buffer) {
  RCHECK(ReadWriteHeaderInternal(buffer));
  if (buffer->Reading()) {
    std::vector<uint8_t> data;
    RCHECK(buffer->ReadWriteVector(&data, buffer->BytesLeft()));
    RCHECK(es_descriptor.Parse(data));
    if (es_descriptor.decoder_config_descriptor().IsAAC()) {
      RCHECK(aac_audio_specific_config.Parse(
          es_descriptor.decoder_config_descriptor()
              .decoder_specific_info_descriptor()
              .data()));
    }
  } else {
    DCHECK(buffer->writer());
    es_descriptor.Write(buffer->writer());
  }
  return true;
}

size_t ElementaryStreamDescriptor::ComputeSizeInternal() {
  // This box is optional. Skip it if not initialized.
  if (es_descriptor.decoder_config_descriptor().object_type() ==
      ObjectType::kForbidden) {
    return 0;
  }
  return HeaderSize() + es_descriptor.ComputeSize();
}

MHAConfiguration::MHAConfiguration() = default;
MHAConfiguration::~MHAConfiguration() = default;

FourCC MHAConfiguration::BoxType() const {
  return FOURCC_mhaC;
}

bool MHAConfiguration::ReadWriteInternal(BoxBuffer* buffer) {
  RCHECK(ReadWriteHeaderInternal(buffer) &&
         buffer->ReadWriteVector(
             &data, buffer->Reading() ? buffer->BytesLeft() : data.size()));
  RCHECK(data.size() > 1);
  mpeg_h_3da_profile_level_indication = data[1];
  return true;
}

size_t MHAConfiguration::ComputeSizeInternal() {
  // This box is optional. Skip it if not initialized.
  if (data.empty())
    return 0;
  return HeaderSize() + data.size();
}

DTSSpecific::DTSSpecific() = default;
DTSSpecific::~DTSSpecific() = default;
;

FourCC DTSSpecific::BoxType() const {
  return FOURCC_ddts;
}

bool DTSSpecific::ReadWriteInternal(BoxBuffer* buffer) {
  RCHECK(ReadWriteHeaderInternal(buffer) &&
         buffer->ReadWriteUInt32(&sampling_frequency) &&
         buffer->ReadWriteUInt32(&max_bitrate) &&
         buffer->ReadWriteUInt32(&avg_bitrate) &&
         buffer->ReadWriteUInt8(&pcm_sample_depth));

  if (buffer->Reading()) {
    RCHECK(buffer->ReadWriteVector(&extra_data, buffer->BytesLeft()));
  } else {
    if (extra_data.empty()) {
      extra_data.assign(kDdtsExtraData,
                        kDdtsExtraData + sizeof(kDdtsExtraData));
    }
    RCHECK(buffer->ReadWriteVector(&extra_data, extra_data.size()));
  }
  return true;
}

size_t DTSSpecific::ComputeSizeInternal() {
  // This box is optional. Skip it if not initialized.
  if (sampling_frequency == 0)
    return 0;
  return HeaderSize() + sizeof(sampling_frequency) + sizeof(max_bitrate) +
         sizeof(avg_bitrate) + sizeof(pcm_sample_depth) +
         sizeof(kDdtsExtraData);
}

AC3Specific::AC3Specific() = default;
AC3Specific::~AC3Specific() = default;

FourCC AC3Specific::BoxType() const {
  return FOURCC_dac3;
}

bool AC3Specific::ReadWriteInternal(BoxBuffer* buffer) {
  RCHECK(ReadWriteHeaderInternal(buffer) &&
         buffer->ReadWriteVector(
             &data, buffer->Reading() ? buffer->BytesLeft() : data.size()));
  return true;
}

size_t AC3Specific::ComputeSizeInternal() {
  // This box is optional. Skip it if not initialized.
  if (data.empty())
    return 0;
  return HeaderSize() + data.size();
}

EC3Specific::EC3Specific() = default;
EC3Specific::~EC3Specific() = default;

FourCC EC3Specific::BoxType() const {
  return FOURCC_dec3;
}

bool EC3Specific::ReadWriteInternal(BoxBuffer* buffer) {
  RCHECK(ReadWriteHeaderInternal(buffer));
  size_t size = buffer->Reading() ? buffer->BytesLeft() : data.size();
  RCHECK(buffer->ReadWriteVector(&data, size));
  return true;
}

size_t EC3Specific::ComputeSizeInternal() {
  // This box is optional. Skip it if not initialized.
  if (data.empty())
    return 0;
  return HeaderSize() + data.size();
}

AC4Specific::AC4Specific() = default;
AC4Specific::~AC4Specific() = default;

FourCC AC4Specific::BoxType() const {
  return FOURCC_dac4;
}

bool AC4Specific::ReadWriteInternal(BoxBuffer* buffer) {
  RCHECK(ReadWriteHeaderInternal(buffer));
  size_t size = buffer->Reading() ? buffer->BytesLeft() : data.size();
  RCHECK(buffer->ReadWriteVector(&data, size));
  return true;
}

size_t AC4Specific::ComputeSizeInternal() {
  // This box is optional. Skip it if not initialized.
  if (data.empty())
    return 0;
  return HeaderSize() + data.size();
}

OpusSpecific::OpusSpecific() = default;
OpusSpecific::~OpusSpecific() = default;

FourCC OpusSpecific::BoxType() const {
  return FOURCC_dOps;
}

bool OpusSpecific::ReadWriteInternal(BoxBuffer* buffer) {
  RCHECK(ReadWriteHeaderInternal(buffer));
  if (buffer->Reading()) {
    std::vector<uint8_t> data;
    const int kMinOpusSpecificBoxDataSize = 11;
    RCHECK(buffer->BytesLeft() >= kMinOpusSpecificBoxDataSize);
    RCHECK(buffer->ReadWriteVector(&data, buffer->BytesLeft()));
    preskip = data[2] + (data[3] << 8);

    // https://tools.ietf.org/html/draft-ietf-codec-oggopus-06#section-5
    BufferWriter writer;
    writer.AppendInt(FOURCC_Opus);
    writer.AppendInt(FOURCC_Head);
    // The version must always be 1.
    const uint8_t kOpusIdentificationHeaderVersion = 1;
    data[0] = kOpusIdentificationHeaderVersion;
    writer.AppendVector(data);
    writer.SwapBuffer(&opus_identification_header);
  } else {
    // https://tools.ietf.org/html/draft-ietf-codec-oggopus-06#section-5
    // The first 8 bytes is "magic signature".
    const size_t kOpusMagicSignatureSize = 8u;
    DCHECK_GT(opus_identification_header.size(), kOpusMagicSignatureSize);
    // https://www.opus-codec.org/docs/opus_in_isobmff.html
    // The version field shall be set to 0.
    const uint8_t kOpusSpecificBoxVersion = 0;
    buffer->writer()->AppendInt(kOpusSpecificBoxVersion);
    buffer->writer()->AppendArray(
        &opus_identification_header[kOpusMagicSignatureSize + 1],
        opus_identification_header.size() - kOpusMagicSignatureSize - 1);
  }
  return true;
}

size_t OpusSpecific::ComputeSizeInternal() {
  // This box is optional. Skip it if not initialized.
  if (opus_identification_header.empty())
    return 0;
  // https://tools.ietf.org/html/draft-ietf-codec-oggopus-06#section-5
  // The first 8 bytes is "magic signature".
  const size_t kOpusMagicSignatureSize = 8u;
  DCHECK_GT(opus_identification_header.size(), kOpusMagicSignatureSize);
  return HeaderSize() + opus_identification_header.size() -
         kOpusMagicSignatureSize;
}

FlacSpecific::FlacSpecific() = default;
FlacSpecific::~FlacSpecific() = default;

FourCC FlacSpecific::BoxType() const {
  return FOURCC_dfLa;
}

bool FlacSpecific::ReadWriteInternal(BoxBuffer* buffer) {
  RCHECK(ReadWriteHeaderInternal(buffer));
  size_t size = buffer->Reading() ? buffer->BytesLeft() : data.size();
  RCHECK(buffer->ReadWriteVector(&data, size));
  return true;
}

size_t FlacSpecific::ComputeSizeInternal() {
  // This box is optional. Skip it if not initialized.
  if (data.empty())
    return 0;
  return HeaderSize() + data.size();
}

AudioSampleEntry::AudioSampleEntry() = default;
AudioSampleEntry::~AudioSampleEntry() = default;

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

  RCHECK(buffer->TryReadWriteChild(&esds));
  RCHECK(buffer->TryReadWriteChild(&ddts));
  RCHECK(buffer->TryReadWriteChild(&dac3));
  RCHECK(buffer->TryReadWriteChild(&dec3));
  RCHECK(buffer->TryReadWriteChild(&dac4));
  RCHECK(buffer->TryReadWriteChild(&dops));
  RCHECK(buffer->TryReadWriteChild(&dfla));
  RCHECK(buffer->TryReadWriteChild(&mhac));

  // Somehow Edge does not support having sinf box before codec_configuration,
  // box, so just do it in the end of AudioSampleEntry. See
  // https://developer.microsoft.com/en-us/microsoft-edge/platform/issues/12658991/
  if (format == FOURCC_enca) {
    if (buffer->Reading()) {
      // Continue scanning until a supported protection scheme is found, or
      // until we run out of protection schemes.
      while (!IsProtectionSchemeSupported(sinf.type.type))
        RCHECK(buffer->ReadWriteChild(&sinf));
    } else {
      DCHECK(IsProtectionSchemeSupported(sinf.type.type));
      RCHECK(buffer->ReadWriteChild(&sinf));
    }
  }
  return true;
}

size_t AudioSampleEntry::ComputeSizeInternal() {
  if (GetActualFormat() == FOURCC_NULL)
    return 0;
  return HeaderSize() + sizeof(data_reference_index) + sizeof(channelcount) +
         sizeof(samplesize) + sizeof(samplerate) + sinf.ComputeSize() +
         esds.ComputeSize() + ddts.ComputeSize() + dac3.ComputeSize() +
         dec3.ComputeSize() + dops.ComputeSize() + dfla.ComputeSize() +
         dac4.ComputeSize() + mhac.ComputeSize() +
         // Reserved and predefined bytes.
         6 + 8 +  // 6 + 8 bytes reserved.
         4;       // 4 bytes predefined.
}

WebVTTConfigurationBox::WebVTTConfigurationBox() = default;
WebVTTConfigurationBox::~WebVTTConfigurationBox() = default;

FourCC WebVTTConfigurationBox::BoxType() const {
  return FOURCC_vttC;
}

bool WebVTTConfigurationBox::ReadWriteInternal(BoxBuffer* buffer) {
  RCHECK(ReadWriteHeaderInternal(buffer));
  return buffer->ReadWriteString(
      &config, buffer->Reading() ? buffer->BytesLeft() : config.size());
}

size_t WebVTTConfigurationBox::ComputeSizeInternal() {
  return HeaderSize() + config.size();
}

WebVTTSourceLabelBox::WebVTTSourceLabelBox() = default;
WebVTTSourceLabelBox::~WebVTTSourceLabelBox() = default;

FourCC WebVTTSourceLabelBox::BoxType() const {
  return FOURCC_vlab;
}

bool WebVTTSourceLabelBox::ReadWriteInternal(BoxBuffer* buffer) {
  RCHECK(ReadWriteHeaderInternal(buffer));
  return buffer->ReadWriteString(&source_label, buffer->Reading()
                                                    ? buffer->BytesLeft()
                                                    : source_label.size());
}

size_t WebVTTSourceLabelBox::ComputeSizeInternal() {
  if (source_label.empty())
    return 0;
  return HeaderSize() + source_label.size();
}

TextSampleEntry::TextSampleEntry() = default;
TextSampleEntry::~TextSampleEntry() = default;

FourCC TextSampleEntry::BoxType() const {
  if (format == FOURCC_NULL) {
    LOG(ERROR) << "TextSampleEntry should be parsed according to the "
               << "handler type recovered in its Media ancestor.";
  }
  return format;
}

bool TextSampleEntry::ReadWriteInternal(BoxBuffer* buffer) {
  if (buffer->Reading()) {
    DCHECK(buffer->reader());
    format = buffer->reader()->type();
  } else {
    RCHECK(ReadWriteHeaderInternal(buffer));
  }
  RCHECK(buffer->IgnoreBytes(6) &&  // reserved for SampleEntry.
         buffer->ReadWriteUInt16(&data_reference_index));

  if (format == FOURCC_wvtt) {
    // TODO(rkuroiwa): Handle the optional MPEG4BitRateBox.
    RCHECK(buffer->PrepareChildren() && buffer->ReadWriteChild(&config) &&
           buffer->ReadWriteChild(&label));
  } else if (format == FOURCC_stpp) {
    // These are marked as "optional"; but they should still have the
    // null-terminator, so this should still work.
    RCHECK(buffer->ReadWriteCString(&namespace_) &&
           buffer->ReadWriteCString(&schema_location));
  }
  return true;
}

size_t TextSampleEntry::ComputeSizeInternal() {
  // 6 for the (anonymous) reserved bytes for SampleEntry class.
  size_t ret = HeaderSize() + 6 + sizeof(data_reference_index);
  if (format == FOURCC_wvtt) {
    ret += config.ComputeSize() + label.ComputeSize();
  } else if (format == FOURCC_stpp) {
    // +2 for the two null terminators for these strings.
    ret += namespace_.size() + schema_location.size() + 2;
  }
  return ret;
}

MediaHeader::MediaHeader() = default;
MediaHeader::~MediaHeader() = default;

FourCC MediaHeader::BoxType() const {
  return FOURCC_mdhd;
}

bool MediaHeader::ReadWriteInternal(BoxBuffer* buffer) {
  RCHECK(ReadWriteHeaderInternal(buffer));

  uint8_t num_bytes = (version == 1) ? sizeof(uint64_t) : sizeof(uint32_t);
  RCHECK(buffer->ReadWriteUInt64NBytes(&creation_time, num_bytes) &&
         buffer->ReadWriteUInt64NBytes(&modification_time, num_bytes) &&
         buffer->ReadWriteUInt32(&timescale) &&
         buffer->ReadWriteUInt64NBytes(&duration, num_bytes) &&
         language.ReadWrite(buffer) &&
         // predefined.
         buffer->IgnoreBytes(2));
  return true;
}

size_t MediaHeader::ComputeSizeInternal() {
  version = IsFitIn32Bits(creation_time, modification_time, duration) ? 0 : 1;
  return HeaderSize() + sizeof(timescale) +
         sizeof(uint32_t) * (1 + version) * 3 + language.ComputeSize() +
         2;  // 2 bytes predefined.
}

VideoMediaHeader::VideoMediaHeader() {
  const uint32_t kVideoMediaHeaderFlags = 1;
  flags = kVideoMediaHeaderFlags;
}

VideoMediaHeader::~VideoMediaHeader() = default;

FourCC VideoMediaHeader::BoxType() const {
  return FOURCC_vmhd;
}
bool VideoMediaHeader::ReadWriteInternal(BoxBuffer* buffer) {
  RCHECK(ReadWriteHeaderInternal(buffer) &&
         buffer->ReadWriteUInt16(&graphicsmode) &&
         buffer->ReadWriteUInt16(&opcolor_red) &&
         buffer->ReadWriteUInt16(&opcolor_green) &&
         buffer->ReadWriteUInt16(&opcolor_blue));
  return true;
}

size_t VideoMediaHeader::ComputeSizeInternal() {
  return HeaderSize() + sizeof(graphicsmode) + sizeof(opcolor_red) +
         sizeof(opcolor_green) + sizeof(opcolor_blue);
}

SoundMediaHeader::SoundMediaHeader() = default;
SoundMediaHeader::~SoundMediaHeader() = default;

FourCC SoundMediaHeader::BoxType() const {
  return FOURCC_smhd;
}

bool SoundMediaHeader::ReadWriteInternal(BoxBuffer* buffer) {
  RCHECK(ReadWriteHeaderInternal(buffer) && buffer->ReadWriteUInt16(&balance) &&
         buffer->IgnoreBytes(2));  // reserved.
  return true;
}

size_t SoundMediaHeader::ComputeSizeInternal() {
  return HeaderSize() + sizeof(balance) + sizeof(uint16_t);
}

NullMediaHeader::NullMediaHeader() = default;
NullMediaHeader::~NullMediaHeader() = default;

FourCC NullMediaHeader::BoxType() const {
  return FOURCC_nmhd;
}

bool NullMediaHeader::ReadWriteInternal(BoxBuffer* buffer) {
  return ReadWriteHeaderInternal(buffer);
}

size_t NullMediaHeader::ComputeSizeInternal() {
  return HeaderSize();
}

SubtitleMediaHeader::SubtitleMediaHeader() = default;
SubtitleMediaHeader::~SubtitleMediaHeader() = default;

FourCC SubtitleMediaHeader::BoxType() const {
  return FOURCC_sthd;
}

bool SubtitleMediaHeader::ReadWriteInternal(BoxBuffer* buffer) {
  return ReadWriteHeaderInternal(buffer);
}

size_t SubtitleMediaHeader::ComputeSizeInternal() {
  return HeaderSize();
}

DataEntryUrl::DataEntryUrl() {
  const uint32_t kDataEntryUrlFlags = 1;
  flags = kDataEntryUrlFlags;
}

DataEntryUrl::~DataEntryUrl() = default;

FourCC DataEntryUrl::BoxType() const {
  return FOURCC_url;
}
bool DataEntryUrl::ReadWriteInternal(BoxBuffer* buffer) {
  RCHECK(ReadWriteHeaderInternal(buffer));
  if (buffer->Reading()) {
    RCHECK(buffer->ReadWriteVector(&location, buffer->BytesLeft()));
  } else {
    RCHECK(buffer->ReadWriteVector(&location, location.size()));
  }
  return true;
}

size_t DataEntryUrl::ComputeSizeInternal() {
  return HeaderSize() + location.size();
}

DataReference::DataReference() = default;
DataReference::~DataReference() = default;

FourCC DataReference::BoxType() const {
  return FOURCC_dref;
}
bool DataReference::ReadWriteInternal(BoxBuffer* buffer) {
  uint32_t entry_count = static_cast<uint32_t>(data_entry.size());
  RCHECK(ReadWriteHeaderInternal(buffer) &&
         buffer->ReadWriteUInt32(&entry_count));
  data_entry.resize(entry_count);
  RCHECK(buffer->PrepareChildren());
  for (uint32_t i = 0; i < entry_count; ++i)
    RCHECK(buffer->ReadWriteChild(&data_entry[i]));
  return true;
}

size_t DataReference::ComputeSizeInternal() {
  uint32_t count = static_cast<uint32_t>(data_entry.size());
  size_t box_size = HeaderSize() + sizeof(count);
  for (uint32_t i = 0; i < count; ++i)
    box_size += data_entry[i].ComputeSize();
  return box_size;
}

DataInformation::DataInformation() = default;
DataInformation::~DataInformation() = default;

FourCC DataInformation::BoxType() const {
  return FOURCC_dinf;
}

bool DataInformation::ReadWriteInternal(BoxBuffer* buffer) {
  return ReadWriteHeaderInternal(buffer) && buffer->PrepareChildren() &&
         buffer->ReadWriteChild(&dref);
}

size_t DataInformation::ComputeSizeInternal() {
  return HeaderSize() + dref.ComputeSize();
}

MediaInformation::MediaInformation() = default;
MediaInformation::~MediaInformation() = default;

FourCC MediaInformation::BoxType() const {
  return FOURCC_minf;
}

bool MediaInformation::ReadWriteInternal(BoxBuffer* buffer) {
  RCHECK(ReadWriteHeaderInternal(buffer) && buffer->PrepareChildren() &&
         buffer->ReadWriteChild(&dinf) &&
         buffer->ReadWriteChild(&sample_table));
  switch (sample_table.description.type) {
    case kVideo:
      RCHECK(buffer->ReadWriteChild(&vmhd));
      break;
    case kAudio:
      RCHECK(buffer->ReadWriteChild(&smhd));
      break;
    case kText:
      RCHECK(buffer->TryReadWriteChild(&nmhd));
      break;
    case kSubtitle:
      RCHECK(buffer->TryReadWriteChild(&sthd));
      break;
    default:
      NOTIMPLEMENTED();
  }
  // Hint is not supported for now.
  return true;
}

size_t MediaInformation::ComputeSizeInternal() {
  size_t box_size =
      HeaderSize() + dinf.ComputeSize() + sample_table.ComputeSize();
  switch (sample_table.description.type) {
    case kVideo:
      box_size += vmhd.ComputeSize();
      break;
    case kAudio:
      box_size += smhd.ComputeSize();
      break;
    case kText:
      box_size += nmhd.ComputeSize();
      break;
    case kSubtitle:
      box_size += sthd.ComputeSize();
      break;
    default:
      NOTIMPLEMENTED();
  }
  return box_size;
}

Media::Media() = default;
Media::~Media() = default;

FourCC Media::BoxType() const {
  return FOURCC_mdia;
}

bool Media::ReadWriteInternal(BoxBuffer* buffer) {
  RCHECK(ReadWriteHeaderInternal(buffer) && buffer->PrepareChildren() &&
         buffer->ReadWriteChild(&header));
  if (buffer->Reading()) {
    RCHECK(buffer->ReadWriteChild(&handler));
    // Maddeningly, the HandlerReference box specifies how to parse the
    // SampleDescription box, making the latter the only box (of those that we
    // support) which cannot be parsed correctly on its own (or even with
    // information from its strict ancestor tree). We thus copy the handler type
    // to the sample description box *before* parsing it to provide this
    // information while parsing.
    information.sample_table.description.type =
        FourCCToTrackType(handler.handler_type);
  } else {
    handler.handler_type =
        TrackTypeToFourCC(information.sample_table.description.type);
    RCHECK(handler.handler_type != FOURCC_NULL);
    RCHECK(buffer->ReadWriteChild(&handler));
  }
  RCHECK(buffer->ReadWriteChild(&information));
  return true;
}

size_t Media::ComputeSizeInternal() {
  handler.handler_type =
      TrackTypeToFourCC(information.sample_table.description.type);
  return HeaderSize() + header.ComputeSize() + handler.ComputeSize() +
         information.ComputeSize();
}

Track::Track() = default;
Track::~Track() = default;

FourCC Track::BoxType() const {
  return FOURCC_trak;
}

bool Track::ReadWriteInternal(BoxBuffer* buffer) {
  RCHECK(ReadWriteHeaderInternal(buffer) && buffer->PrepareChildren() &&
         buffer->ReadWriteChild(&header) && buffer->ReadWriteChild(&media) &&
         buffer->TryReadWriteChild(&edit) &&
         buffer->TryReadWriteChild(&sample_encryption));
  return true;
}

size_t Track::ComputeSizeInternal() {
  return HeaderSize() + header.ComputeSize() + media.ComputeSize() +
         edit.ComputeSize();
}

MovieExtendsHeader::MovieExtendsHeader() = default;
MovieExtendsHeader::~MovieExtendsHeader() = default;

FourCC MovieExtendsHeader::BoxType() const {
  return FOURCC_mehd;
}

bool MovieExtendsHeader::ReadWriteInternal(BoxBuffer* buffer) {
  RCHECK(ReadWriteHeaderInternal(buffer));
  size_t num_bytes = (version == 1) ? sizeof(uint64_t) : sizeof(uint32_t);
  RCHECK(buffer->ReadWriteUInt64NBytes(&fragment_duration, num_bytes));
  return true;
}

size_t MovieExtendsHeader::ComputeSizeInternal() {
  // This box is optional. Skip it if it is not used.
  if (fragment_duration == 0)
    return 0;
  version = IsFitIn32Bits(fragment_duration) ? 0 : 1;
  return HeaderSize() + sizeof(uint32_t) * (1 + version);
}

TrackExtends::TrackExtends() = default;
TrackExtends::~TrackExtends() = default;

FourCC TrackExtends::BoxType() const {
  return FOURCC_trex;
}

bool TrackExtends::ReadWriteInternal(BoxBuffer* buffer) {
  RCHECK(ReadWriteHeaderInternal(buffer) &&
         buffer->ReadWriteUInt32(&track_id) &&
         buffer->ReadWriteUInt32(&default_sample_description_index) &&
         buffer->ReadWriteUInt32(&default_sample_duration) &&
         buffer->ReadWriteUInt32(&default_sample_size) &&
         buffer->ReadWriteUInt32(&default_sample_flags));
  return true;
}

size_t TrackExtends::ComputeSizeInternal() {
  return HeaderSize() + sizeof(track_id) +
         sizeof(default_sample_description_index) +
         sizeof(default_sample_duration) + sizeof(default_sample_size) +
         sizeof(default_sample_flags);
}

MovieExtends::MovieExtends() = default;
MovieExtends::~MovieExtends() = default;

FourCC MovieExtends::BoxType() const {
  return FOURCC_mvex;
}

bool MovieExtends::ReadWriteInternal(BoxBuffer* buffer) {
  RCHECK(ReadWriteHeaderInternal(buffer) && buffer->PrepareChildren() &&
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

size_t MovieExtends::ComputeSizeInternal() {
  // This box is optional. Skip it if it does not contain any track.
  if (tracks.size() == 0)
    return 0;
  size_t box_size = HeaderSize() + header.ComputeSize();
  for (uint32_t i = 0; i < tracks.size(); ++i)
    box_size += tracks[i].ComputeSize();
  return box_size;
}

Movie::Movie() = default;
Movie::~Movie() = default;

FourCC Movie::BoxType() const {
  return FOURCC_moov;
}

bool Movie::ReadWriteInternal(BoxBuffer* buffer) {
  RCHECK(ReadWriteHeaderInternal(buffer) && buffer->PrepareChildren() &&
         buffer->ReadWriteChild(&header));
  if (buffer->Reading()) {
    BoxReader* reader = buffer->reader();
    DCHECK(reader);
    RCHECK(reader->ReadChildren(&tracks) && reader->TryReadChild(&extends) &&
           reader->TryReadChildren(&pssh));
  } else {
    // The 'meta' box is not well formed in the video captured by Android's
    // default camera app: spec indicates that it is a FullBox but it is written
    // as a Box. This results in the box failed to be parsed. See
    // https://github.com/shaka-project/shaka-packager/issues/319 for details.
    // We do not care the content of metadata box in the source content, so just
    // skip reading the box.
    RCHECK(buffer->TryReadWriteChild(&metadata));
    if (FLAGS_mvex_before_trak) {
      // |extends| has to be written before |tracks| to workaround Android
      // MediaExtractor bug which requires |mvex| to be placed before |trak|.
      // See https://github.com/shaka-project/shaka-packager/issues/711 for
      // details.
      RCHECK(buffer->TryReadWriteChild(&extends));
    }
    for (uint32_t i = 0; i < tracks.size(); ++i)
      RCHECK(buffer->ReadWriteChild(&tracks[i]));
    if (!FLAGS_mvex_before_trak) {
      RCHECK(buffer->TryReadWriteChild(&extends));
    }
    for (uint32_t i = 0; i < pssh.size(); ++i)
      RCHECK(buffer->ReadWriteChild(&pssh[i]));
  }
  return true;
}

size_t Movie::ComputeSizeInternal() {
  size_t box_size = HeaderSize() + header.ComputeSize() +
                    metadata.ComputeSize() + extends.ComputeSize();
  for (uint32_t i = 0; i < tracks.size(); ++i)
    box_size += tracks[i].ComputeSize();
  for (uint32_t i = 0; i < pssh.size(); ++i)
    box_size += pssh[i].ComputeSize();
  return box_size;
}

TrackFragmentDecodeTime::TrackFragmentDecodeTime() = default;
TrackFragmentDecodeTime::~TrackFragmentDecodeTime() = default;

FourCC TrackFragmentDecodeTime::BoxType() const {
  return FOURCC_tfdt;
}

bool TrackFragmentDecodeTime::ReadWriteInternal(BoxBuffer* buffer) {
  RCHECK(ReadWriteHeaderInternal(buffer));
  size_t num_bytes = (version == 1) ? sizeof(uint64_t) : sizeof(uint32_t);
  RCHECK(buffer->ReadWriteUInt64NBytes(&decode_time, num_bytes));
  return true;
}

size_t TrackFragmentDecodeTime::ComputeSizeInternal() {
  version = IsFitIn32Bits(decode_time) ? 0 : 1;
  return HeaderSize() + sizeof(uint32_t) * (1 + version);
}

MovieFragmentHeader::MovieFragmentHeader() = default;
MovieFragmentHeader::~MovieFragmentHeader() = default;

FourCC MovieFragmentHeader::BoxType() const {
  return FOURCC_mfhd;
}

bool MovieFragmentHeader::ReadWriteInternal(BoxBuffer* buffer) {
  return ReadWriteHeaderInternal(buffer) &&
         buffer->ReadWriteUInt32(&sequence_number);
}

size_t MovieFragmentHeader::ComputeSizeInternal() {
  return HeaderSize() + sizeof(sequence_number);
}

TrackFragmentHeader::TrackFragmentHeader() = default;
TrackFragmentHeader::~TrackFragmentHeader() = default;

FourCC TrackFragmentHeader::BoxType() const {
  return FOURCC_tfhd;
}

bool TrackFragmentHeader::ReadWriteInternal(BoxBuffer* buffer) {
  RCHECK(ReadWriteHeaderInternal(buffer) && buffer->ReadWriteUInt32(&track_id));

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

size_t TrackFragmentHeader::ComputeSizeInternal() {
  size_t box_size = HeaderSize() + sizeof(track_id);
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

TrackFragmentRun::TrackFragmentRun() = default;
TrackFragmentRun::~TrackFragmentRun() = default;

FourCC TrackFragmentRun::BoxType() const {
  return FOURCC_trun;
}

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

  uint32_t first_sample_flags(0);

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

size_t TrackFragmentRun::ComputeSizeInternal() {
  size_t box_size = HeaderSize() + sizeof(sample_count);
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

TrackFragment::TrackFragment() = default;
TrackFragment::~TrackFragment() = default;

FourCC TrackFragment::BoxType() const {
  return FOURCC_traf;
}

bool TrackFragment::ReadWriteInternal(BoxBuffer* buffer) {
  RCHECK(ReadWriteHeaderInternal(buffer) && buffer->PrepareChildren() &&
         buffer->ReadWriteChild(&header));
  if (buffer->Reading()) {
    DCHECK(buffer->reader());
    decode_time_absent = !buffer->reader()->ChildExist(&decode_time);
    if (!decode_time_absent)
      RCHECK(buffer->ReadWriteChild(&decode_time));
    RCHECK(buffer->reader()->TryReadChildren(&runs) &&
           buffer->reader()->TryReadChildren(&sample_group_descriptions) &&
           buffer->reader()->TryReadChildren(&sample_to_groups));
  } else {
    if (!decode_time_absent)
      RCHECK(buffer->ReadWriteChild(&decode_time));
    for (uint32_t i = 0; i < runs.size(); ++i)
      RCHECK(buffer->ReadWriteChild(&runs[i]));
    for (uint32_t i = 0; i < sample_to_groups.size(); ++i)
      RCHECK(buffer->ReadWriteChild(&sample_to_groups[i]));
    for (uint32_t i = 0; i < sample_group_descriptions.size(); ++i)
      RCHECK(buffer->ReadWriteChild(&sample_group_descriptions[i]));
  }
  return buffer->TryReadWriteChild(&auxiliary_size) &&
         buffer->TryReadWriteChild(&auxiliary_offset) &&
         buffer->TryReadWriteChild(&sample_encryption);
}

size_t TrackFragment::ComputeSizeInternal() {
  size_t box_size = HeaderSize() + header.ComputeSize() +
                    decode_time.ComputeSize() + auxiliary_size.ComputeSize() +
                    auxiliary_offset.ComputeSize() +
                    sample_encryption.ComputeSize();
  for (uint32_t i = 0; i < runs.size(); ++i)
    box_size += runs[i].ComputeSize();
  for (uint32_t i = 0; i < sample_group_descriptions.size(); ++i)
    box_size += sample_group_descriptions[i].ComputeSize();
  for (uint32_t i = 0; i < sample_to_groups.size(); ++i)
    box_size += sample_to_groups[i].ComputeSize();
  return box_size;
}

MovieFragment::MovieFragment() = default;
MovieFragment::~MovieFragment() = default;

FourCC MovieFragment::BoxType() const {
  return FOURCC_moof;
}

bool MovieFragment::ReadWriteInternal(BoxBuffer* buffer) {
  RCHECK(ReadWriteHeaderInternal(buffer) && buffer->PrepareChildren() &&
         buffer->ReadWriteChild(&header));
  if (buffer->Reading()) {
    BoxReader* reader = buffer->reader();
    DCHECK(reader);
    RCHECK(reader->ReadChildren(&tracks) && reader->TryReadChildren(&pssh));
  } else {
    for (uint32_t i = 0; i < tracks.size(); ++i)
      RCHECK(buffer->ReadWriteChild(&tracks[i]));
    for (uint32_t i = 0; i < pssh.size(); ++i)
      RCHECK(buffer->ReadWriteChild(&pssh[i]));
  }
  return true;
}

size_t MovieFragment::ComputeSizeInternal() {
  size_t box_size = HeaderSize() + header.ComputeSize();
  for (uint32_t i = 0; i < tracks.size(); ++i)
    box_size += tracks[i].ComputeSize();
  for (uint32_t i = 0; i < pssh.size(); ++i)
    box_size += pssh[i].ComputeSize();
  return box_size;
}

SegmentIndex::SegmentIndex() = default;
SegmentIndex::~SegmentIndex() = default;

FourCC SegmentIndex::BoxType() const {
  return FOURCC_sidx;
}

bool SegmentIndex::ReadWriteInternal(BoxBuffer* buffer) {
  RCHECK(ReadWriteHeaderInternal(buffer) &&
         buffer->ReadWriteUInt32(&reference_id) &&
         buffer->ReadWriteUInt32(&timescale));

  size_t num_bytes = (version == 1) ? sizeof(uint64_t) : sizeof(uint32_t);
  RCHECK(
      buffer->ReadWriteUInt64NBytes(&earliest_presentation_time, num_bytes) &&
      buffer->ReadWriteUInt64NBytes(&first_offset, num_bytes));

  uint16_t reference_count;
  if (references.size() <= std::numeric_limits<uint16_t>::max()) {
    reference_count = static_cast<uint16_t>(references.size());
  } else {
    reference_count = std::numeric_limits<uint16_t>::max();
    LOG(WARNING) << "Seeing " << references.size()
                 << " subsegment references, but at most " << reference_count
                 << " references can be stored in 'sidx' box."
                 << " The extra references are truncated.";
    LOG(WARNING) << "The stream will not play to the end in DASH.";
    LOG(WARNING) << "A possible workaround is to increase segment duration.";
  }
  RCHECK(buffer->IgnoreBytes(2) &&  // reserved.
         buffer->ReadWriteUInt16(&reference_count));
  if (buffer->Reading())
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

size_t SegmentIndex::ComputeSizeInternal() {
  version = IsFitIn32Bits(earliest_presentation_time, first_offset) ? 0 : 1;
  return HeaderSize() + sizeof(reference_id) + sizeof(timescale) +
         sizeof(uint32_t) * (1 + version) * 2 + 2 * sizeof(uint16_t) +
         3 * sizeof(uint32_t) *
             std::min(
                 references.size(),
                 static_cast<size_t>(std::numeric_limits<uint16_t>::max()));
}

MediaData::MediaData() = default;
MediaData::~MediaData() = default;

FourCC MediaData::BoxType() const {
  return FOURCC_mdat;
}

bool MediaData::ReadWriteInternal(BoxBuffer* buffer) {
  NOTIMPLEMENTED() << "Actual data is parsed and written separately.";
  return false;
}

size_t MediaData::ComputeSizeInternal() {
  return HeaderSize() + data_size;
}

CueSourceIDBox::CueSourceIDBox() = default;
CueSourceIDBox::~CueSourceIDBox() = default;

FourCC CueSourceIDBox::BoxType() const {
  return FOURCC_vsid;
}

bool CueSourceIDBox::ReadWriteInternal(BoxBuffer* buffer) {
  RCHECK(ReadWriteHeaderInternal(buffer) && buffer->ReadWriteInt32(&source_id));
  return true;
}

size_t CueSourceIDBox::ComputeSizeInternal() {
  if (source_id == kCueSourceIdNotSet)
    return 0;
  return HeaderSize() + sizeof(source_id);
}

CueTimeBox::CueTimeBox() = default;
CueTimeBox::~CueTimeBox() = default;

FourCC CueTimeBox::BoxType() const {
  return FOURCC_ctim;
}

bool CueTimeBox::ReadWriteInternal(BoxBuffer* buffer) {
  RCHECK(ReadWriteHeaderInternal(buffer));
  return buffer->ReadWriteString(
      &cue_current_time,
      buffer->Reading() ? buffer->BytesLeft() : cue_current_time.size());
}

size_t CueTimeBox::ComputeSizeInternal() {
  if (cue_current_time.empty())
    return 0;
  return HeaderSize() + cue_current_time.size();
}

CueIDBox::CueIDBox() = default;
CueIDBox::~CueIDBox() = default;

FourCC CueIDBox::BoxType() const {
  return FOURCC_iden;
}

bool CueIDBox::ReadWriteInternal(BoxBuffer* buffer) {
  RCHECK(ReadWriteHeaderInternal(buffer));
  return buffer->ReadWriteString(
      &cue_id, buffer->Reading() ? buffer->BytesLeft() : cue_id.size());
}

size_t CueIDBox::ComputeSizeInternal() {
  if (cue_id.empty())
    return 0;
  return HeaderSize() + cue_id.size();
}

CueSettingsBox::CueSettingsBox() = default;
CueSettingsBox::~CueSettingsBox() = default;

FourCC CueSettingsBox::BoxType() const {
  return FOURCC_sttg;
}

bool CueSettingsBox::ReadWriteInternal(BoxBuffer* buffer) {
  RCHECK(ReadWriteHeaderInternal(buffer));
  return buffer->ReadWriteString(
      &settings, buffer->Reading() ? buffer->BytesLeft() : settings.size());
}

size_t CueSettingsBox::ComputeSizeInternal() {
  if (settings.empty())
    return 0;
  return HeaderSize() + settings.size();
}

CuePayloadBox::CuePayloadBox() = default;
CuePayloadBox::~CuePayloadBox() = default;

FourCC CuePayloadBox::BoxType() const {
  return FOURCC_payl;
}

bool CuePayloadBox::ReadWriteInternal(BoxBuffer* buffer) {
  RCHECK(ReadWriteHeaderInternal(buffer));
  return buffer->ReadWriteString(
      &cue_text, buffer->Reading() ? buffer->BytesLeft() : cue_text.size());
}

size_t CuePayloadBox::ComputeSizeInternal() {
  return HeaderSize() + cue_text.size();
}

VTTEmptyCueBox::VTTEmptyCueBox() = default;
VTTEmptyCueBox::~VTTEmptyCueBox() = default;

FourCC VTTEmptyCueBox::BoxType() const {
  return FOURCC_vtte;
}

bool VTTEmptyCueBox::ReadWriteInternal(BoxBuffer* buffer) {
  return ReadWriteHeaderInternal(buffer);
}

size_t VTTEmptyCueBox::ComputeSizeInternal() {
  return HeaderSize();
}

VTTAdditionalTextBox::VTTAdditionalTextBox() = default;
VTTAdditionalTextBox::~VTTAdditionalTextBox() = default;

FourCC VTTAdditionalTextBox::BoxType() const {
  return FOURCC_vtta;
}

bool VTTAdditionalTextBox::ReadWriteInternal(BoxBuffer* buffer) {
  RCHECK(ReadWriteHeaderInternal(buffer));
  return buffer->ReadWriteString(
      &cue_additional_text,
      buffer->Reading() ? buffer->BytesLeft() : cue_additional_text.size());
}

size_t VTTAdditionalTextBox::ComputeSizeInternal() {
  return HeaderSize() + cue_additional_text.size();
}

VTTCueBox::VTTCueBox() = default;
VTTCueBox::~VTTCueBox() = default;

FourCC VTTCueBox::BoxType() const {
  return FOURCC_vttc;
}

bool VTTCueBox::ReadWriteInternal(BoxBuffer* buffer) {
  RCHECK(ReadWriteHeaderInternal(buffer) && buffer->PrepareChildren() &&
         buffer->TryReadWriteChild(&cue_source_id) &&
         buffer->TryReadWriteChild(&cue_id) &&
         buffer->TryReadWriteChild(&cue_time) &&
         buffer->TryReadWriteChild(&cue_settings) &&
         buffer->ReadWriteChild(&cue_payload));
  return true;
}

size_t VTTCueBox::ComputeSizeInternal() {
  return HeaderSize() + cue_source_id.ComputeSize() + cue_id.ComputeSize() +
         cue_time.ComputeSize() + cue_settings.ComputeSize() +
         cue_payload.ComputeSize();
}

}  // namespace mp4
}  // namespace media
}  // namespace shaka
