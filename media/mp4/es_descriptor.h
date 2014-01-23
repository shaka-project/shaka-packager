// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MP4_ES_DESCRIPTOR_H_
#define MEDIA_MP4_ES_DESCRIPTOR_H_

#include <vector>

#include "base/basictypes.h"

namespace media {

class BitReader;
class BufferWriter;

namespace mp4 {

// The following values are extracted from ISO 14496 Part 1 Table 5 -
// objectTypeIndication Values. Only values currently in use are included.
enum ObjectType {
  kForbidden = 0,
  kISO_14496_3 = 0x40,         // MPEG4 AAC
  kISO_13818_7_AAC_LC = 0x67,  // MPEG2 AAC-LC
  kEAC3 = 0xa6                 // Dolby Digital Plus
};

/// This class parses object type and decoder specific information from an
/// elementary stream descriptor, which is usually contained in an esds
/// box. Please refer to ISO 14496 Part 1 7.2.6.5 for more details.
class ESDescriptor {
 public:
  ESDescriptor();
  ~ESDescriptor();

  bool Parse(const std::vector<uint8>& data);
  void Write(BufferWriter* writer) const;
  size_t ComputeSize() const;

  uint16 esid() const { return esid_; }
  void set_esid(uint16 esid) { esid_ = esid; }

  ObjectType object_type() const { return object_type_; }
  void set_object_type(ObjectType object_type) { object_type_ = object_type; }

  const std::vector<uint8>& decoder_specific_info() const {
    return decoder_specific_info_;
  }
  void set_decoder_specific_info(
      const std::vector<uint8>& decoder_specific_info) {
    decoder_specific_info_ = decoder_specific_info;
  }

  /// @return true if the stream is AAC.
  bool IsAAC() const {
    return object_type_ == kISO_14496_3 || object_type_ == kISO_13818_7_AAC_LC;
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

  uint16 esid_;  // Elementary Stream ID.
  ObjectType object_type_;
  std::vector<uint8> decoder_specific_info_;
};

}  // namespace mp4

}  // namespace media

#endif  // MEDIA_MP4_ES_DESCRIPTOR_H_
