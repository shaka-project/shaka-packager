// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <packager/media/codecs/es_descriptor.h>

#include <absl/log/check.h>

#include <packager/media/base/bit_reader.h>
#include <packager/media/base/buffer_writer.h>
#include <packager/media/base/rcheck.h>

namespace shaka {
namespace media {
namespace {

// ISO/IEC 14496-1:2004 Section 7.2.6.6 Table 6: StreamType values.
enum StreamType {
  kForbiddenStreamType = 0x00,
  kObjectDescriptorStreamType = 0x01,
  kClockReferenceStreamType = 0x02,
  kSceneDescriptionStreamType = 0x03,
  kVisualStreamType = 0x04,
  kAudioStreamType = 0x05,
  kMPEG7StreamType = 0x06,
  kIPMPStreamType = 0x07,
  kObjectContentInfoStreamType = 0x08,
  kMPEGJStreamType = 0x09,
  kInteractionStream = 0x0A,
  kIPMPToolStreamType = 0x0B,
};

// ISO/IEC 14496-1:2004 Section 7.3.2.3 Table 12: ISO SL Config Descriptor.
enum SLPredefinedTags {
  kSLPredefinedNull = 0x01,
  kSLPredefinedMP4 = 0x02,
};

// The elementary stream size is specific by up to 4 bytes.
// The MSB of a byte indicates if there are more bytes for the size.
bool ReadDescriptorSize(BitReader* reader, size_t* size) {
  uint8_t msb;
  uint8_t byte;

  *size = 0;

  for (size_t i = 0; i < 4; ++i) {
    RCHECK(reader->ReadBits(1, &msb));
    RCHECK(reader->ReadBits(7, &byte));
    *size = (*size << 7) + byte;

    if (msb == 0)
      break;
  }

  return true;
}

void WriteDescriptorSize(size_t size, BufferWriter* writer) {
  std::vector<uint8_t> size_bytes;
  while (size > 0) {
    uint8_t byte = (size & 0x7F);
    size >>= 7;
    if (!size_bytes.empty())
      byte |= 0x80;
    size_bytes.push_back(byte);
  }
  for (auto iter = size_bytes.rbegin(); iter != size_bytes.rend(); iter++)
    writer->AppendInt(*iter);
}

size_t CountDescriptorSize(size_t size) {
  size_t num_bytes = 0;
  while (size > 0) {
    num_bytes++;
    size >>= 7;
  }
  return num_bytes;
}

}  // namespace

bool BaseDescriptor::Parse(const std::vector<uint8_t>& data) {
  BitReader reader(data.data(), data.size());
  return Read(&reader);
}

bool BaseDescriptor::Read(BitReader* reader) {
  uint8_t tag;
  RCHECK(reader->ReadBits(8, &tag));
  if (tag != static_cast<uint8_t>(tag_)) {
    LOG(ERROR) << "Expecting tag " << static_cast<int>(tag_) << ", but seeing "
               << static_cast<int>(tag);
    return false;
  }
  RCHECK(ReadDescriptorSize(reader, &data_size_));
  return ReadData(reader);
}

void BaseDescriptor::Write(BufferWriter* writer) {
  // Compute and update descriptor size.
  size_t size = ComputeSize();
  size_t buffer_size_before_write = writer->Size();

  WriteInternal(writer);

  DCHECK_EQ(size, writer->Size() - buffer_size_before_write);
}

size_t BaseDescriptor::ComputeSize() {
  data_size_ = ComputeDataSize();
  return 1 + CountDescriptorSize(data_size_) + data_size_;
}

void BaseDescriptor::WriteHeader(BufferWriter* writer) {
  writer->AppendInt(static_cast<uint8_t>(tag_));
  WriteDescriptorSize(data_size_, writer);
}

bool DecoderSpecificInfoDescriptor::ReadData(BitReader* reader) {
  data_.resize(data_size());
  for (uint8_t& data_entry : data_)
    RCHECK(reader->ReadBits(8, &data_entry));
  return true;
}

void DecoderSpecificInfoDescriptor::WriteInternal(BufferWriter* writer) {
  WriteHeader(writer);
  writer->AppendVector(data_);
}

size_t DecoderSpecificInfoDescriptor::ComputeDataSize() {
  return data_.size();
}

bool DecoderConfigDescriptor::ReadData(BitReader* reader) {
  const size_t start_pos = reader->bit_position();
  RCHECK(reader->ReadBits(8, &object_type_));

  int stream_type;
  RCHECK(reader->ReadBits(6, &stream_type));
  if (stream_type != kAudioStreamType) {
    LOG(ERROR) << "Seeing non audio stream type " << stream_type;
    return false;
  }

  RCHECK(reader->SkipBits(2));  // Skip |upStream| and |reserved|.
  RCHECK(reader->ReadBits(24, &buffer_size_db_));
  RCHECK(reader->ReadBits(32, &max_bitrate_));
  RCHECK(reader->ReadBits(32, &avg_bitrate_));
  const size_t fields_bits = reader->bit_position() - start_pos;

  const size_t kBitsInByte = 8;
  const bool has_child_tags = data_size() * kBitsInByte > fields_bits;
  decoder_specific_info_descriptor_ = DecoderSpecificInfoDescriptor();
  if (has_child_tags)
    RCHECK(decoder_specific_info_descriptor_.Read(reader));

  return true;
}

void DecoderConfigDescriptor::WriteInternal(BufferWriter* writer) {
  WriteHeader(writer);

  writer->AppendInt(static_cast<uint8_t>(object_type_));
  // 6 bit stream type. The last bit is reserved with 1.
  const uint8_t stream_type = (kAudioStreamType << 2) | 1;
  writer->AppendInt(stream_type);
  writer->AppendNBytes(buffer_size_db_, 3);
  writer->AppendInt(max_bitrate_);
  writer->AppendInt(avg_bitrate_);

  if (!decoder_specific_info_descriptor_.data().empty())
    decoder_specific_info_descriptor_.Write(writer);
}

size_t DecoderConfigDescriptor::ComputeDataSize() {
  // object_type (1 byte), stream_type (1 byte), decoding_buffer_size (3 bytes),
  // max_bitrate (4 bytes), avg_bitrate (4 bytes).
  const size_t data_size_without_children = 1 + 1 + 3 + 4 + 4;
  if (decoder_specific_info_descriptor_.data().empty())
    return data_size_without_children;
  return data_size_without_children +
         decoder_specific_info_descriptor_.ComputeSize();
}

bool SLConfigDescriptor::ReadData(BitReader*) {
  return true;
}

void SLConfigDescriptor::WriteInternal(BufferWriter* writer) {
  WriteHeader(writer);
  writer->AppendInt(static_cast<uint8_t>(kSLPredefinedMP4));
}

size_t SLConfigDescriptor::ComputeDataSize() {
  return 1;
}

bool ESDescriptor::ReadData(BitReader* reader) {
  bool stream_dependency_flag;
  bool url_flag;
  bool ocr_stream_flag;
  RCHECK(reader->ReadBits(16, &esid_));
  RCHECK(reader->ReadBits(1, &stream_dependency_flag));
  RCHECK(reader->ReadBits(1, &url_flag));
  RCHECK(!url_flag);  // We don't support url flag
  RCHECK(reader->ReadBits(1, &ocr_stream_flag));
  RCHECK(reader->SkipBits(5));  // streamPriority

  if (stream_dependency_flag)
    RCHECK(reader->SkipBits(16));  // dependsOn_ES_ID
  if (ocr_stream_flag)
    RCHECK(reader->SkipBits(16));  // OCR_ES_Id

  return decoder_config_descriptor_.Read(reader);
  // Skip the parsing of |sl_config_descriptor_| intentionally as we do not care
  // about the data.
}

void ESDescriptor::WriteInternal(BufferWriter* writer) {
  WriteHeader(writer);

  // According to ISO/IEC 14496-14:2018 Section 4.1.2, 
  // ES_ID is set to 0 when stored
  const uint16_t kEsid = 0;
  writer->AppendInt(kEsid);
  const uint8_t kNoEsFlags = 0;
  writer->AppendInt(kNoEsFlags);

  decoder_config_descriptor_.Write(writer);
  sl_config_descriptor_.Write(writer);
}

size_t ESDescriptor::ComputeDataSize() {
  // esid (2 bytes), es_flags (1 byte).
  const size_t data_size_without_children = 2 + 1;
  return data_size_without_children + decoder_config_descriptor_.ComputeSize() +
         sl_config_descriptor_.ComputeSize();
}

}  // namespace media
}  // namespace shaka
