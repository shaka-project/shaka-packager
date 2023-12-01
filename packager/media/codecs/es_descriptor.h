// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PACKAGER_MEDIA_CODECS_ES_DESCRIPTOR_H_
#define PACKAGER_MEDIA_CODECS_ES_DESCRIPTOR_H_

#include <cstddef>
#include <cstdint>
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
  kISO_13818_3_MPEG1 = 0x69,   // MPEG1 ISO/IEC 13818-3, 16,22.05,24kHz
  kISO_11172_3_MPEG1 = 0x6B,   // MPEG1 ISO/IEC 11172-3, 32,44.1,48kHz
  kDTSC = 0xA9,                // DTS Coherent Acoustics audio
  kDTSE = 0xAC,                // DTS Express low bit rate audio
  kDTSH = 0xAA,                // DTS-HD High Resolution Audio
  kDTSL = 0xAB,                // DTS-HD Master Audio
};

enum class DescriptorTag {
  kForbidden = 0,
  kES = 0x03,
  kDecoderConfig = 0x04,
  kDecoderSpecificInfo = 0x05,
  kSLConfig = 0x06,
};

/// Defines the base Descriptor object as defined in ISO 14496-1:2004 Systems
/// section 7.2.2.2. All descriptors inherit from either BaseDescriptor.
class BaseDescriptor {
 public:
  explicit BaseDescriptor(DescriptorTag tag) : tag_(tag) {}

  /// Parse the descriptor from input data.
  /// @param data contains the descriptor data.
  bool Parse(const std::vector<uint8_t>& data);

  /// Read the descriptor.
  /// @param reader points to a BitReader object.
  bool Read(BitReader* reader);

  /// Write the descriptor to buffer. This function calls ComputeSize internally
  /// to compute and update descriptor size.
  /// @param writer points to a BufferWriter object which wraps the buffer for
  ///        writing.
  void Write(BufferWriter* writer);

  /// Compute the size of this descriptor. It will also update descriptor size.
  /// @return The size of result descriptor including child descriptors.
  size_t ComputeSize();

 protected:
  /// Write descriptor header.
  void WriteHeader(BufferWriter* writer);

  /// @return descriptor data size without header in bytes.
  size_t data_size() const { return data_size_; }

 private:
  // Read the descriptor data (header is already read).
  virtual bool ReadData(BitReader* reader) = 0;
  // Write the descriptor. The descriptor data size should already be updated.
  virtual void WriteInternal(BufferWriter* writer) = 0;
  // Compute the data size, with child descriptors included.
  virtual size_t ComputeDataSize() = 0;

  DescriptorTag tag_ = DescriptorTag::kForbidden;
  size_t data_size_ = 0;
};

/// Implements DecoderSpecificInfo descriptor according to ISO
/// 14496-1:2004 7.2.6.7 DecoderSpecificInfo.
class DecoderSpecificInfoDescriptor : public BaseDescriptor {
 public:
  DecoderSpecificInfoDescriptor()
      : BaseDescriptor(DescriptorTag::kDecoderSpecificInfo) {}

  const std::vector<uint8_t>& data() const { return data_; }

  void set_data(const std::vector<uint8_t>& data) { data_ = data; }

 private:
  bool ReadData(BitReader* reader) override;
  void WriteInternal(BufferWriter* writer) override;
  size_t ComputeDataSize() override;

  std::vector<uint8_t> data_;
};

/// Implements DecoderConfig descriptor according to ISO 14496-1:2004 7.2.6.6
/// DecoderConfigDescriptor.
class DecoderConfigDescriptor : public BaseDescriptor {
 public:
  DecoderConfigDescriptor() : BaseDescriptor(DescriptorTag::kDecoderConfig) {}

  uint32_t buffer_size_db() const { return buffer_size_db_; }
  void set_buffer_size_db(uint32_t buffer_size_db) {
    buffer_size_db_ = buffer_size_db;
  }

  uint32_t max_bitrate() const { return max_bitrate_; }
  void set_max_bitrate(uint32_t max_bitrate) { max_bitrate_ = max_bitrate; }

  uint32_t avg_bitrate() const { return avg_bitrate_; }
  void set_avg_bitrate(uint32_t avg_bitrate) { avg_bitrate_ = avg_bitrate; }

  ObjectType object_type() const { return object_type_; }
  void set_object_type(ObjectType object_type) { object_type_ = object_type; }

  /// @return true if the stream is AAC.
  bool IsAAC() const {
    return object_type_ == ObjectType::kISO_14496_3 ||
           object_type_ == ObjectType::kISO_13818_7_AAC_LC;
  }

  /// @return true if the stream is DTS.
  bool IsDTS() const {
    return object_type_ == ObjectType::kDTSC ||
           object_type_ == ObjectType::kDTSE ||
           object_type_ == ObjectType::kDTSH ||
           object_type_ == ObjectType::kDTSL;
  }

  const DecoderSpecificInfoDescriptor& decoder_specific_info_descriptor()
      const {
    return decoder_specific_info_descriptor_;
  }

  DecoderSpecificInfoDescriptor* mutable_decoder_specific_info_descriptor() {
    return &decoder_specific_info_descriptor_;
  }

 private:
  bool ReadData(BitReader* reader) override;
  void WriteInternal(BufferWriter* writer) override;
  size_t ComputeDataSize() override;

  ObjectType object_type_ = ObjectType::kForbidden;
  uint32_t buffer_size_db_ = 0;
  uint32_t max_bitrate_ = 0;
  uint32_t avg_bitrate_ = 0;
  DecoderSpecificInfoDescriptor decoder_specific_info_descriptor_;
};

/// Implements SLConfig descriptor according to ISO 14496-1:2004 7.2.6.8
/// SLConfigDescriptor.
class SLConfigDescriptor : public BaseDescriptor {
 public:
  SLConfigDescriptor() : BaseDescriptor(DescriptorTag::kSLConfig) {}

 private:
  bool ReadData(BitReader* reader) override;
  void WriteInternal(BufferWriter* writer) override;
  size_t ComputeDataSize() override;
};

/// This class parses object type and decoder specific information from an
/// elementary stream descriptor, which is usually contained in an esds
/// box. Please refer to ISO 14496 Part 1 7.2.6.5 for more details.
class ESDescriptor : public BaseDescriptor {
 public:
  ESDescriptor() : BaseDescriptor(DescriptorTag::kES) {}

  uint16_t esid() const { return esid_; }

  const DecoderConfigDescriptor& decoder_config_descriptor() const {
    return decoder_config_descriptor_;
  }

  DecoderConfigDescriptor* mutable_decoder_config_descriptor() {
    return &decoder_config_descriptor_;
  }

 private:
  bool ReadData(BitReader* reader) override;
  void WriteInternal(BufferWriter* writer) override;
  size_t ComputeDataSize() override;

  uint16_t esid_ = 0;  // Elementary Stream ID.

  DecoderConfigDescriptor decoder_config_descriptor_;
  SLConfigDescriptor sl_config_descriptor_;
};

}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_CODECS_ES_DESCRIPTOR_H_
