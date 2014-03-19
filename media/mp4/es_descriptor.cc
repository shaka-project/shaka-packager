// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mp4/es_descriptor.h"

#include "media/base/bit_reader.h"
#include "media/base/buffer_writer.h"
#include "media/mp4/rcheck.h"

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
bool ReadESSize(media::BitReader* reader, uint32* size) {
  uint8 msb;
  uint8 byte;

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

}  // namespace

namespace media {

namespace mp4 {

ESDescriptor::ESDescriptor() : esid_(0), object_type_(kForbidden) {}

ESDescriptor::~ESDescriptor() {}

bool ESDescriptor::Parse(const std::vector<uint8>& data) {
  BitReader reader(&data[0], data.size());
  uint8 tag;
  uint32 size;
  uint8 stream_dependency_flag;
  uint8 url_flag;
  uint8 ocr_stream_flag;
  uint16 dummy;

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
  uint8 tag;
  uint32 size;
  uint64 dummy;

  RCHECK(reader->ReadBits(8, &tag));
  RCHECK(tag == kDecoderConfigDescrTag);
  RCHECK(ReadESSize(reader, &size));

  RCHECK(reader->ReadBits(8, &object_type_));
  RCHECK(reader->ReadBits(64, &dummy));
  RCHECK(reader->ReadBits(32, &dummy));
  RCHECK(ParseDecoderSpecificInfo(reader));

  return true;
}

bool ESDescriptor::ParseDecoderSpecificInfo(BitReader* reader) {
  DCHECK(reader);
  uint8 tag;
  uint32 size;

  RCHECK(reader->ReadBits(8, &tag));
  RCHECK(tag == kDecoderSpecificInfoTag);
  RCHECK(ReadESSize(reader, &size));

  decoder_specific_info_.resize(size);
  for (uint32 i = 0; i < size; ++i)
    RCHECK(reader->ReadBits(8, &decoder_specific_info_[i]));

  return true;
}

void ESDescriptor::Write(BufferWriter* writer) const {
  // TODO: Consider writing Descriptor classes.
  // ElementaryStreamDescriptor, DecoderConfigDescriptor, SLConfigDescriptor,
  // DecoderSpecificInfoDescriptor.
  DCHECK(writer);
  CHECK_LT(decoder_specific_info_.size(), kMaxDecoderSpecificInfoSize);

  const std::vector<uint8> kEmptyDecodingBufferSize(3, 0);
  const uint32 kUnknownBitrate = 0;
  const uint8 kNoEsFlags = 0;

  const uint8 decoder_specific_info_size = decoder_specific_info_.size();

  // 6 bit stream type. The last bit is reserved with 1.
  const uint8 stream_type = (kAudioStreamType << 2) | 1;
  const uint8 decoder_config_size = decoder_specific_info_size + kHeaderSize +
                                    sizeof(uint8) +  // object_type_.
                                    sizeof(stream_type) +
                                    kEmptyDecodingBufferSize.size() +
                                    sizeof(kUnknownBitrate) * 2;

  const uint8 sl_config_size = sizeof(uint8);  // predefined.
  const uint8 es_size = decoder_config_size + kHeaderSize + sl_config_size +
                        kHeaderSize + sizeof(esid_) + sizeof(kNoEsFlags);

  writer->AppendInt(static_cast<uint8>(kESDescrTag));
  writer->AppendInt(es_size);
  writer->AppendInt(esid_);
  writer->AppendInt(kNoEsFlags);

  writer->AppendInt(static_cast<uint8>(kDecoderConfigDescrTag));
  writer->AppendInt(decoder_config_size);
  writer->AppendInt(static_cast<uint8>(object_type_));
  writer->AppendInt(stream_type);
  writer->AppendVector(kEmptyDecodingBufferSize);
  writer->AppendInt(kUnknownBitrate);  // max_bitrate.
  writer->AppendInt(kUnknownBitrate);  // avg_bitrate.

  writer->AppendInt(static_cast<uint8>(kDecoderSpecificInfoTag));
  writer->AppendInt(decoder_specific_info_size);
  writer->AppendVector(decoder_specific_info_);

  writer->AppendInt(static_cast<uint8>(kSLConfigTag));
  writer->AppendInt(sl_config_size);
  writer->AppendInt(static_cast<uint8>(kSLPredefinedMP4));
}

size_t ESDescriptor::ComputeSize() const {
  // A bit magical. Refer to ESDescriptor::Write for details.
  const uint8 decoder_specific_info_size = decoder_specific_info_.size();
  const uint8 decoder_config_size = decoder_specific_info_size + kHeaderSize +
                                    sizeof(uint8) * 5 + sizeof(uint32) * 2;
  const uint8 sl_config_size = sizeof(uint8);
  const uint8 es_size = decoder_config_size + kHeaderSize +
                        sl_config_size + kHeaderSize +
                        sizeof(esid_) + sizeof(uint8);
  return es_size + kHeaderSize;
}

}  // namespace mp4

}  // namespace media
