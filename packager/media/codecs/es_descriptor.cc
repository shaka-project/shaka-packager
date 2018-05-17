// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "packager/media/codecs/es_descriptor.h"

#include "packager/media/base/bit_reader.h"
#include "packager/media/base/buffer_writer.h"
#include "packager/media/base/rcheck.h"

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
bool ReadESSize(BitReader* reader, uint32_t* size) {
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

// Descryptor Header Size: 1 byte tag and 1 byte size (we don't support
// multi-bytes size for now).
const size_t kHeaderSize = 2;
const size_t kMaxDecoderSpecificInfoSize = 64;
const uint32_t kUnknownBitrate = 0;
const size_t kBitsInByte = 8;

}  // namespace

ESDescriptor::ESDescriptor()
    : esid_(0),
      object_type_(ObjectType::kForbidden),
      max_bitrate_(kUnknownBitrate),
      avg_bitrate_(kUnknownBitrate) {}

ESDescriptor::~ESDescriptor() {}

bool ESDescriptor::Parse(const std::vector<uint8_t>& data) {
  BitReader reader(&data[0], data.size());
  uint8_t tag;
  uint32_t size;
  uint8_t stream_dependency_flag;
  uint8_t url_flag;
  uint8_t ocr_stream_flag;
  uint16_t dummy;

  RCHECK(reader.ReadBits(8, &tag));
  RCHECK(tag == kESDescrTag);
  RCHECK(ReadESSize(&reader, &size));

  RCHECK(reader.ReadBits(16, &esid_));  // ES_ID
  RCHECK(reader.ReadBits(1, &stream_dependency_flag));
  RCHECK(reader.ReadBits(1, &url_flag));
  RCHECK(!url_flag);  // We don't support url flag
  RCHECK(reader.ReadBits(1, &ocr_stream_flag));
  RCHECK(reader.ReadBits(5, &dummy));  // streamPriority

  if (stream_dependency_flag)
    RCHECK(reader.ReadBits(16, &dummy));  // dependsOn_ES_ID
  if (ocr_stream_flag)
    RCHECK(reader.ReadBits(16, &dummy));  // OCR_ES_Id

  RCHECK(ParseDecoderConfigDescriptor(&reader));

  return true;
}

bool ESDescriptor::ParseDecoderConfigDescriptor(BitReader* reader) {
  uint8_t tag;
  uint32_t size;
  uint32_t dummy;

  RCHECK(reader->ReadBits(8, &tag));
  RCHECK(tag == kDecoderConfigDescrTag);
  RCHECK(ReadESSize(reader, &size));

  const size_t start_pos = reader->bit_position();
  RCHECK(reader->ReadBits(8, &object_type_));
  RCHECK(reader->ReadBits(32, &dummy));
  RCHECK(reader->ReadBits(32, &max_bitrate_));
  RCHECK(reader->ReadBits(32, &avg_bitrate_));
  const size_t fields_bits = reader->bit_position() - start_pos;

  const bool has_child_tags = size * kBitsInByte > fields_bits;
  if (has_child_tags)
    RCHECK(ParseDecoderSpecificInfo(reader));

  return true;
}

bool ESDescriptor::ParseDecoderSpecificInfo(BitReader* reader) {
  DCHECK(reader);
  uint8_t tag;
  uint32_t size;

  RCHECK(reader->ReadBits(8, &tag));
  RCHECK(tag == kDecoderSpecificInfoTag);
  RCHECK(ReadESSize(reader, &size));

  decoder_specific_info_.resize(size);
  for (uint32_t i = 0; i < size; ++i)
    RCHECK(reader->ReadBits(8, &decoder_specific_info_[i]));
  return true;
}

void ESDescriptor::Write(BufferWriter* writer) const {
  DCHECK(writer);
  CHECK_LT(decoder_specific_info_.size(), kMaxDecoderSpecificInfoSize);

  const std::vector<uint8_t> kEmptyDecodingBufferSize(3, 0);
  const uint8_t kNoEsFlags = 0;

  const uint8_t decoder_specific_info_size =
    static_cast<uint8_t>(decoder_specific_info_.size());

  // 6 bit stream type. The last bit is reserved with 1.
  const uint8_t stream_type = (kAudioStreamType << 2) | 1;
  const uint8_t decoder_config_size =
    static_cast<uint8_t>(decoder_specific_info_size + kHeaderSize +
       sizeof(uint8_t) +  // object_type_.
       sizeof(stream_type) +
       kEmptyDecodingBufferSize.size() +
       sizeof(kUnknownBitrate) * 2);

  const uint8_t sl_config_size = sizeof(uint8_t);  // predefined.
  const uint8_t es_size = decoder_config_size + kHeaderSize + sl_config_size +
                          kHeaderSize + sizeof(esid_) + sizeof(kNoEsFlags);

  writer->AppendInt(static_cast<uint8_t>(kESDescrTag));
  writer->AppendInt(es_size);
  writer->AppendInt(esid_);
  writer->AppendInt(kNoEsFlags);

  writer->AppendInt(static_cast<uint8_t>(kDecoderConfigDescrTag));
  writer->AppendInt(decoder_config_size);
  writer->AppendInt(static_cast<uint8_t>(object_type_));
  writer->AppendInt(stream_type);
  writer->AppendVector(kEmptyDecodingBufferSize);
  writer->AppendInt(max_bitrate_);
  writer->AppendInt(avg_bitrate_);

  writer->AppendInt(static_cast<uint8_t>(kDecoderSpecificInfoTag));
  writer->AppendInt(decoder_specific_info_size);
  writer->AppendVector(decoder_specific_info_);

  writer->AppendInt(static_cast<uint8_t>(kSLConfigTag));
  writer->AppendInt(sl_config_size);
  writer->AppendInt(static_cast<uint8_t>(kSLPredefinedMP4));
}

size_t ESDescriptor::ComputeSize() const {
  // A bit magical. Refer to ESDescriptor::Write for details.
  const uint8_t decoder_specific_info_size =
    static_cast<uint8_t>(decoder_specific_info_.size());
  const uint8_t decoder_config_size = decoder_specific_info_size + kHeaderSize +
                                      sizeof(uint8_t) * 5 +
                                      sizeof(uint32_t) * 2;
  const uint8_t sl_config_size = sizeof(uint8_t);
  const uint8_t es_size = decoder_config_size + kHeaderSize + sl_config_size +
                          kHeaderSize + sizeof(esid_) + sizeof(uint8_t);
  return es_size + kHeaderSize;
}

}  // namespace media
}  // namespace shaka
