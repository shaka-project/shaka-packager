// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PACKAGER_MEDIA_CODECS_ES_DESCRIPTOR_H_
#define PACKAGER_MEDIA_CODECS_ES_DESCRIPTOR_H_

#include <stddef.h>
#include <stdint.h>

#include <vector>

namespace shaka {
namespace media {

class BitReader;
class BufferWriter;

// The following values are extracted from ISO 14496 Part 1 Table 5 -
// objectTypeIndication Values. Only values currently in use are included.
enum class ObjectType : uint8_t {
  kForbidden = 0,
  kISO_14496_3 = 0x40,         // MPEG4 AAC
  kISO_13818_7_AAC_LC = 0x67,  // MPEG2 AAC-LC
  kDTSC = 0xA9,                // DTS Coherent Acoustics audio
  kDTSE = 0xAC,                // DTS Express low bit rate audio
  kDTSH = 0xAA,                // DTS-HD High Resolution Audio
  kDTSL = 0xAB,                // DTS-HD Master Audio
};

/// This class parses object type and decoder specific information from an
/// elementary stream descriptor, which is usually contained in an esds
/// box. Please refer to ISO 14496 Part 1 7.2.6.5 for more details.
class ESDescriptor {
 public:
  ESDescriptor();
  ~ESDescriptor();

  bool Parse(const std::vector<uint8_t>& data);
  void Write(BufferWriter* writer) const;
  size_t ComputeSize() const;

  uint16_t esid() const { return esid_; }
  void set_esid(uint16_t esid) { esid_ = esid; }

  uint32_t max_bitrate() const {return max_bitrate_; }
  void set_max_bitrate(uint32_t max_bitrate) { max_bitrate_ = max_bitrate; }

  uint32_t avg_bitrate() const { return avg_bitrate_; }
  void set_avg_bitrate(uint32_t avg_bitrate) { avg_bitrate_ = avg_bitrate; }

  ObjectType object_type() const { return object_type_; }
  void set_object_type(ObjectType object_type) { object_type_ = object_type; }

  const std::vector<uint8_t>& decoder_specific_info() const {
    return decoder_specific_info_;
  }
  void set_decoder_specific_info(
      const std::vector<uint8_t>& decoder_specific_info) {
    decoder_specific_info_ = decoder_specific_info;
  }

  /// @return true if the stream is AAC.
  bool IsAAC() const {
    return object_type_ == ObjectType::kISO_14496_3 ||
           object_type_ == ObjectType::kISO_13818_7_AAC_LC;
  }

  bool IsDTS() const {
    return object_type_ == ObjectType::kDTSC ||
           object_type_ == ObjectType::kDTSE ||
           object_type_ == ObjectType::kDTSH ||
           object_type_ == ObjectType::kDTSL;
  }

 private:
  enum Tag {
    kESDescrTag = 0x03,
    kDecoderConfigDescrTag = 0x04,
    kDecoderSpecificInfoTag = 0x05,
    kSLConfigTag = 0x06,
  };

  bool ParseDecoderConfigDescriptor(BitReader* reader);
  bool ParseDecoderSpecificInfo(BitReader* reader);

  uint16_t esid_;  // Elementary Stream ID.
  ObjectType object_type_;
  uint32_t max_bitrate_;
  uint32_t avg_bitrate_;
  std::vector<uint8_t> decoder_specific_info_;
};

}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_CODECS_ES_DESCRIPTOR_H_
