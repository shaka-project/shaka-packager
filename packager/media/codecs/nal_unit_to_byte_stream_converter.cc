// Copyright 2016 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/media/codecs/nal_unit_to_byte_stream_converter.h>

#include <list>

#include <absl/log/check.h>
#include <absl/log/log.h>

#include <packager/macros/compiler.h>
#include <packager/media/base/bit_reader.h>
#include <packager/media/base/buffer_reader.h>
#include <packager/media/base/buffer_writer.h>
#include <packager/media/codecs/nalu_reader.h>

namespace shaka {
namespace media {

namespace {

const bool kEscapeData = true;
const uint8_t kNaluStartCode[] = {0x00, 0x00, 0x00, 0x01};

const uint8_t kEmulationPreventionByte = 0x03;

const uint8_t kAccessUnitDelimiterRbspAnyPrimaryPicType = 0xF0;

bool IsNaluEqual(const Nalu& left, const Nalu& right) {
  if (left.type() != right.type())
    return false;
  const size_t left_size = left.header_size() + left.payload_size();
  const size_t right_size = right.header_size() + right.payload_size();
  if (left_size != right_size)
    return false;
  return memcmp(left.data(), right.data(), left_size) == 0;
}

void AppendNalu(const Nalu& nalu,
                int /*nalu_length_size*/,
                bool escape_data,
                BufferWriter* buffer_writer) {
  if (escape_data) {
    EscapeNalByteSequence(nalu.data(), nalu.header_size() + nalu.payload_size(),
                          buffer_writer);
  } else {
    buffer_writer->AppendArray(nalu.data(),
                               nalu.header_size() + nalu.payload_size());
  }
}

void AddAccessUnitDelimiter(BufferWriter* buffer_writer) {
  buffer_writer->AppendInt(static_cast<uint8_t>(Nalu::H264_AUD));
  // For now, primary_pic_type is 7 which is "anything".
  buffer_writer->AppendInt(kAccessUnitDelimiterRbspAnyPrimaryPicType);
}

}  // namespace

void EscapeNalByteSequence(const uint8_t* input,
                           size_t input_size,
                           BufferWriter* output_writer) {
  // Keep track of consecutive zeros that it has seen (not including the current
  // byte), so that the algorithm doesn't need to go back to check the same
  // bytes.
  int consecutive_zero_count = 0;
  for (size_t i = 0; i < input_size; ++i) {
    if (consecutive_zero_count <= 1) {
      output_writer->AppendInt(input[i]);
    } else if (consecutive_zero_count == 2) {
      if (input[i] == 0 || input[i] == 1 || input[i] == 2 || input[i] == 3) {
        // Must be escaped.
        output_writer->AppendInt(kEmulationPreventionByte);
      }

      output_writer->AppendInt(input[i]);
      // Note that input[i] can be 0.
      // 00 00 00 00 00 00 should become
      // 00 00 03 00 00 03 00 00 03
      // So consecutive_zero_count is reset here and incremented below if
      // input[i] is 0.
      consecutive_zero_count = 0;
    }

    consecutive_zero_count = input[i] == 0 ? consecutive_zero_count + 1 : 0;
  }

  // ISO 14496-10 Section 7.4.1.1 mentions that if the last byte is 0 (which
  // only happens if RBSP has cabac_zero_word), 0x03 must be appended.
  if (consecutive_zero_count > 0) {
    DCHECK_GT(input_size, 0u);
    DCHECK_EQ(input[input_size - 1], 0u);
    output_writer->AppendInt(kEmulationPreventionByte);
  }
}

// This functions creates a new subsample entry (|clear_bytes|, |cipher_bytes|)
// and appends it to |subsamples|. It splits the oversized (64KB) clear_bytes
// into smaller ones.
void AppendSubsamples(uint32_t clear_bytes,
                      uint32_t cipher_bytes,
                      std::vector<SubsampleEntry>* subsamples) {
  while (clear_bytes > UINT16_MAX) {
    subsamples->emplace_back(UINT16_MAX, 0);
    clear_bytes -= UINT16_MAX;
  }
  subsamples->emplace_back(clear_bytes, cipher_bytes);
}

// TODO(hmchen): Wrap methods of processing subsamples into a separate class,
// e.g., SubsampleReader.
// This function finds the range of the subsamples corresponding a NAL unit
// size. If a subsample crosses the boundary of two NAL units, it is split into
// smaller subsamples. Each call processes one NAL unit and it assumes the input
// NAL unit is already aligned with subsamples->at(start_subsample_id).
//
// An example of calling multiple times on each NAL unit is as follow:
//
// Input:
//
// Nalu 0                         Nalu 1              Nalu 2
//  |                               |                    |
//  v                               v                    v
//  | clear | cipher |     clear    |        clear       | clear | cipher |
//
//  |  Subsample 0   |                      Subsample 1                   |
//
// Output:
//
//  |  Subsample 0   | Subsample 1  |     Subsample 2    | Subsample 3    |
//
// Nalu 0: start_subsample_id = 0, next_subsample_id = 2
// Nalu 1: start_subsample_id = 2, next_subsample_id = 3
// Nalu 2: start_subsample_id = 3, next_subsample_id = 4
bool AlignSubsamplesWithNalu(size_t nalu_size,
                             size_t start_subsample_id,
                             std::vector<SubsampleEntry>* subsamples,
                             size_t* next_subsample_id) {
  DCHECK(subsamples && !subsamples->empty());
  size_t subsample_id = start_subsample_id;
  size_t nalu_size_remain = nalu_size;
  size_t subsample_bytes = 0;
  while (subsample_id < subsamples->size()) {
    subsample_bytes = subsamples->at(subsample_id).clear_bytes +
                      subsamples->at(subsample_id).cipher_bytes;
    if (nalu_size_remain <= subsample_bytes) {
      break;
    }
    nalu_size_remain -= subsample_bytes;
    subsample_id++;
  }

  if (subsample_id == subsamples->size()) {
    DCHECK_GT(nalu_size_remain, 0u);
    LOG(ERROR)
        << "Total size of NAL unit is larger than the size of subsamples.";
    return false;
  }

  if (nalu_size_remain == subsample_bytes) {
    *next_subsample_id = subsample_id + 1;
    return true;
  }

  DCHECK_GT(subsample_bytes, nalu_size_remain);
  size_t clear_bytes = subsamples->at(subsample_id).clear_bytes;
  size_t new_clear_bytes = 0;
  size_t new_cipher_bytes = 0;
  if (nalu_size_remain < clear_bytes) {
    new_clear_bytes = nalu_size_remain;
  } else {
    new_clear_bytes = clear_bytes;
    new_cipher_bytes = nalu_size_remain - clear_bytes;
  }
  subsamples->insert(subsamples->begin() + subsample_id,
                     SubsampleEntry(static_cast<uint16_t>(new_clear_bytes),
                                    static_cast<uint32_t>(new_cipher_bytes)));
  subsample_id++;
  subsamples->at(subsample_id).clear_bytes -=
      static_cast<uint16_t>(new_clear_bytes);
  subsamples->at(subsample_id).cipher_bytes -=
      static_cast<uint32_t>(new_cipher_bytes);
  *next_subsample_id = subsample_id;
  return true;
}

// This function tries to merge clear-only into clear+cipher subsamples. This
// merge makes sure the clear_bytes will not exceed the clear size limits
// (2^16 bytes).
std::vector<SubsampleEntry> MergeSubsamples(
    const std::vector<SubsampleEntry>& subsamples) {
  std::vector<SubsampleEntry> new_subsamples;
  uint32_t clear_bytes = 0;
  for (size_t i = 0; i < subsamples.size(); ++i) {
    clear_bytes += subsamples[i].clear_bytes;
    // Add new subsample(s).
    if (subsamples[i].cipher_bytes > 0 || i == subsamples.size() - 1) {
      AppendSubsamples(clear_bytes, subsamples[i].cipher_bytes,
                       &new_subsamples);
      clear_bytes = 0;
    }
  }
  return new_subsamples;
}

NalUnitToByteStreamConverter::NalUnitToByteStreamConverter()
    : nalu_length_size_(0) {}
NalUnitToByteStreamConverter::~NalUnitToByteStreamConverter() {}

bool NalUnitToByteStreamConverter::Initialize(
    const uint8_t* decoder_configuration_data,
    size_t decoder_configuration_data_size) {
  if (!decoder_configuration_data || decoder_configuration_data_size == 0) {
    LOG(ERROR) << "Decoder conguration is empty.";
    return false;
  }

  if (!decoder_config_.Parse(std::vector<uint8_t>(
          decoder_configuration_data,
          decoder_configuration_data + decoder_configuration_data_size))) {
    return false;
  }

  if (decoder_config_.nalu_count() < 2) {
    LOG(ERROR) << "Cannot find SPS or PPS.";
    return false;
  }

  nalu_length_size_ = decoder_config_.nalu_length_size();

  BufferWriter buffer_writer(decoder_configuration_data_size);
  bool found_sps = false;
  bool found_pps = false;
  for (uint32_t i = 0; i < decoder_config_.nalu_count(); ++i) {
    const Nalu& nalu = decoder_config_.nalu(i);
    if (nalu.type() == Nalu::H264NaluType::H264_SPS) {
      buffer_writer.AppendArray(kNaluStartCode, std::size(kNaluStartCode));
      AppendNalu(nalu, nalu_length_size_, !kEscapeData, &buffer_writer);
      found_sps = true;
    } else if (nalu.type() == Nalu::H264NaluType::H264_PPS) {
      buffer_writer.AppendArray(kNaluStartCode, std::size(kNaluStartCode));
      AppendNalu(nalu, nalu_length_size_, !kEscapeData, &buffer_writer);
      found_pps = true;
    } else if (nalu.type() == Nalu::H264NaluType::H264_SPSExtension) {
      buffer_writer.AppendArray(kNaluStartCode, std::size(kNaluStartCode));
      AppendNalu(nalu, nalu_length_size_, !kEscapeData, &buffer_writer);
    }
  }
  if (!found_sps || !found_pps) {
    LOG(ERROR) << "Failed to find SPS or PPS.";
    return false;
  }

  buffer_writer.SwapBuffer(&decoder_configuration_in_byte_stream_);
  return true;
}

bool NalUnitToByteStreamConverter::ConvertUnitToByteStream(
    const uint8_t* sample,
    size_t sample_size,
    bool is_key_frame,
    std::vector<uint8_t>* output) {
  return ConvertUnitToByteStreamWithSubsamples(
      sample, sample_size, is_key_frame, false, output,
      nullptr);  // Skip subsample update.
}

// This ignores all AUD, SPS, and PPS in the sample. Instead uses the data
// parsed in Initialize(). However, if the SPS and PPS are different to
// those parsed in Initialized(), they are kept.
bool NalUnitToByteStreamConverter::ConvertUnitToByteStreamWithSubsamples(
    const uint8_t* sample,
    size_t sample_size,
    bool is_key_frame,
    bool escape_encrypted_nalu,
    std::vector<uint8_t>* output,
    std::vector<SubsampleEntry>* subsamples) {
  if (!sample || sample_size == 0) {
    LOG(WARNING) << "Sample is empty.";
    return true;
  }

  std::vector<SubsampleEntry> temp_subsamples;

  BufferWriter buffer_writer(sample_size);
  buffer_writer.AppendArray(kNaluStartCode, std::size(kNaluStartCode));
  AddAccessUnitDelimiter(&buffer_writer);
  if (is_key_frame)
    buffer_writer.AppendVector(decoder_configuration_in_byte_stream_);

  if (subsamples && !subsamples->empty()) {
    // The inserted part in buffer_writer is all clear. Add a corresponding
    // all-clear subsample.
    AppendSubsamples(static_cast<uint32_t>(buffer_writer.Size()), 0u,
                     &temp_subsamples);
  }

  NaluReader nalu_reader(Nalu::kH264, nalu_length_size_, sample, sample_size);
  Nalu nalu;
  NaluReader::Result result = nalu_reader.Advance(&nalu);

  size_t start_subsample_id = 0;
  size_t next_subsample_id = 0;
  while (result == NaluReader::kOk) {
    const size_t old_nalu_size =
        nalu_length_size_ + nalu.header_size() + nalu.payload_size();
    if (subsamples && !subsamples->empty()) {
      if (!AlignSubsamplesWithNalu(old_nalu_size, start_subsample_id,
                                   subsamples, &next_subsample_id)) {
        return false;
      }
    }
    switch (nalu.type()) {
      case Nalu::H264_AUD:
        break;
      case Nalu::H264_SPS:
        FALLTHROUGH_INTENDED;
      case Nalu::H264_SPSExtension:
        FALLTHROUGH_INTENDED;
      case Nalu::H264_PPS: {
        // Also write this SPS/PPS if it is not the same as SPS/PPS in decoder
        // configuration, which is already written.
        //
        // For more information see:
        //  - github.com/shaka-project/shaka-packager/issues/327
        //  - ISO/IEC 14496-15 5.4.5 Sync Sample
        //
        // TODO(kqyang): Parse sample data to figure out which SPS/PPS the
        // sample actually uses and include that only.
        bool new_decoder_config = true;
        for (size_t i = 0; i < decoder_config_.nalu_count(); ++i) {
          if (IsNaluEqual(decoder_config_.nalu(i), nalu)) {
            new_decoder_config = false;
            break;
          }
        }
        if (!new_decoder_config)
          break;
        FALLTHROUGH_INTENDED;
      }
      default:
        bool escape_data = false;
        if (subsamples && !subsamples->empty()) {
          if (escape_encrypted_nalu) {
            for (size_t i = start_subsample_id; i < next_subsample_id; ++i) {
              if (subsamples->at(i).cipher_bytes != 0) {
                escape_data = true;
                break;
              }
            }
          }
        }
        buffer_writer.AppendArray(kNaluStartCode, std::size(kNaluStartCode));
        AppendNalu(nalu, nalu_length_size_, escape_data, &buffer_writer);

        if (subsamples && !subsamples->empty()) {
          temp_subsamples.emplace_back(
              static_cast<uint16_t>(std::size(kNaluStartCode)), 0u);
          // Update the first subsample of each NAL unit, which replaces NAL
          // unit length field with start code. Note that if the escape_data is
          // true, the total data size and the cipher_bytes may be changed.
          // However, since the escape_data for encrypted nalu is only used in
          // Sample-AES, which means the subsample is not really used,
          // inaccurate subsamples should not be a big deal.
          if (subsamples->at(start_subsample_id).clear_bytes <
              nalu_length_size_) {
            LOG(ERROR) << "Clear bytes ("
                       << subsamples->at(start_subsample_id).clear_bytes
                       << ") in start subsample of NAL unit is less than NAL "
                          "unit length size ("
                       << nalu_length_size_
                       << "). The NAL unit length size is (partially) "
                          "encrypted. In that case, it cannot be "
                          "converted to byte stream.";
            return false;
          }
          subsamples->at(start_subsample_id).clear_bytes -= nalu_length_size_;
          temp_subsamples.insert(temp_subsamples.end(),
                                 subsamples->begin() + start_subsample_id,
                                 subsamples->begin() + next_subsample_id);
        }
        break;
    }

    start_subsample_id = next_subsample_id;
    result = nalu_reader.Advance(&nalu);
  }

  DCHECK_NE(result, NaluReader::kOk);
  if (result != NaluReader::kEOStream) {
    LOG(ERROR) << "Stopped reading before end of stream.";
    return false;
  }

  buffer_writer.SwapBuffer(output);
  if (subsamples && !subsamples->empty()) {
    if (next_subsample_id < subsamples->size()) {
      LOG(ERROR)
          << "The total size of NAL unit is shorter than the subsample size.";
      return false;
    }
    // This function may modify the new_subsamples. But since it creates a
    // merged verion and assign to the output subsamples, the input one is no
    // longer used.
    *subsamples = MergeSubsamples(temp_subsamples);
  }
  return true;
}

}  // namespace media
}  // namespace shaka
