// Copyright 2024 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/media/codecs/iamf_audio_util.h>

#include <iomanip>

#include <packager/media/base/bit_reader.h>
#include <packager/media/base/fourccs.h>
#include <packager/media/base/rcheck.h>
#include <packager/media/base/stream_info.h>

namespace shaka {
namespace media {

namespace {

const uint8_t kMaxIamfProfile = 2;

// 3.2. OBU type
// Only the IA Sequence Header and Codec Configs are used in this
// implementation.
enum ObuType {
  OBU_IA_Codec_Config = 0,
  OBU_IA_Sequence_Header = 31,
};

// 8.1.1. leb128(). Unsigned integer represented by a variable number of
// little-endian bytes.
bool ReadLeb128(BitReader& reader, size_t* size, size_t* leb128_bytes) {
  size_t value = 0;
  size_t bytes_read = 0;
  for (int i = 0; i < 8; i++) {
    size_t leb128_byte = 0;
    RCHECK(reader.ReadBits(8, &leb128_byte));
    value |= (leb128_byte & 0x7f) << (i * 7);
    bytes_read += 1;
    if (!(leb128_byte & 0x80))
      break;
  }
  // It is a requirement of bitstream conformance that the value returned from
  // the leb128 parsing process is less than or equal to (1<<32) - 1.
  RCHECK(value <= ((1ull << 32) - 1));
  if (size != nullptr) {
    *size = value;
  }
  if (leb128_bytes != nullptr) {
    *leb128_bytes = bytes_read;
  }
  return true;
}

bool ParseObuHeader(BitReader& reader, int& obu_type, size_t& obu_size) {
  size_t leb128_bytes;
  bool obu_trimming_status_flag;
  bool obu_extension_flag;

  RCHECK(reader.ReadBits(5, &obu_type));
  RCHECK(reader.SkipBits(1));  // Skip obu_redundant_copy
  RCHECK(reader.ReadBits(1, &obu_trimming_status_flag));
  RCHECK(reader.ReadBits(1, &obu_extension_flag));

  RCHECK(ReadLeb128(reader, &obu_size, &leb128_bytes));

  if (obu_trimming_status_flag) {
    // Skip num_samples_to_trim_at_end
    RCHECK(ReadLeb128(reader, nullptr, &leb128_bytes));
    obu_size -= leb128_bytes;
    // Skip num_samples_to_trim_at_start
    RCHECK(ReadLeb128(reader, nullptr, &leb128_bytes));
    obu_size -= leb128_bytes;
  }

  if (obu_extension_flag) {
    size_t extension_header_size;
    RCHECK(ReadLeb128(reader, &extension_header_size, &leb128_bytes));
    obu_size -= leb128_bytes;
    RCHECK(reader.SkipBits(extension_header_size * 8));
    obu_size -= extension_header_size * 8;
  }

  return true;
}

bool ParseSequenceHeaderObu(BitReader& reader,
                            uint8_t& primary_profile,
                            uint8_t& additional_profile) {
  uint32_t ia_code;

  RCHECK(reader.ReadBits(32, &ia_code));
  if (ia_code != FOURCC_iamf) {
    LOG(WARNING) << "Unknown ia_code= " << std::setfill('0') << std::setw(8)
                 << std::hex << ia_code;
    return false;
  }

  RCHECK(reader.ReadBits(8, &primary_profile));
  if (primary_profile > kMaxIamfProfile) {
    LOG(WARNING) << "Unknown primary_profile= " << primary_profile;
    return false;
  }

  RCHECK(reader.ReadBits(8, &additional_profile));
  if (additional_profile > kMaxIamfProfile) {
    LOG(WARNING) << "Unknown additional_profile= " << additional_profile;
    return false;
  }

  return true;
}

bool ParseCodecConfigObu(BitReader& reader, size_t obu_size, Codec& codec) {
  uint32_t codec_id;
  size_t leb128_bytes;

  // Skip codec_config_id
  RCHECK(ReadLeb128(reader, nullptr, &leb128_bytes));
  obu_size -= leb128_bytes;

  RCHECK(reader.ReadBits(32, &codec_id));
  obu_size -= 4;

  // Skip the remainder of the OBU.
  RCHECK(reader.SkipBits(obu_size * 8));

  switch (codec_id) {
    case FOURCC_Opus:
      codec = kCodecOpus;
      break;
    case FOURCC_mp4a:
      codec = kCodecAAC;
      break;
    case FOURCC_fLaC:
      codec = kCodecFlac;
      break;
    case FOURCC_ipcm:
      codec = kCodecPcm;
      break;
    default:
      LOG(WARNING) << "Unknown codec_id= " << std::setfill('0') << std::setw(8)
                   << std::hex << codec_id;
      return false;
  }

  return true;
}
}  // namespace

bool GetIamfCodecStringInfo(const std::vector<uint8_t>& iacb,
                            uint8_t& codec_string_info) {
  uint8_t primary_profile = 0;
  uint8_t additional_profile = 0;
  // codec used to encode IAMF audio substreams
  Codec iamf_codec = Codec::kUnknownCodec;

  int obu_type;
  size_t obu_size;

  BitReader reader(iacb.data(), iacb.size());

  // configurationVersion
  RCHECK(reader.SkipBits(8));

  // configOBUs_size
  RCHECK(ReadLeb128(reader, &obu_size, nullptr));

  while (reader.bits_available() > 0) {
    RCHECK(ParseObuHeader(reader, obu_type, obu_size));

    switch (obu_type) {
      case OBU_IA_Sequence_Header:
        RCHECK(ParseSequenceHeaderObu(reader, primary_profile,
                                      additional_profile));
        break;
      case OBU_IA_Codec_Config:
        RCHECK(ParseCodecConfigObu(reader, obu_size, iamf_codec));
        break;
      default:
        // Skip other irrelevant OBUs.
        RCHECK(reader.SkipBits(obu_size * 8));
        break;
    }
  }

  // In IAMF v1.1 (https://aomediacodec.github.io/iamf),
  // the valid values of primary_profile and additional_profile are {0, 1, 2}.
  // The valid codec_ids are {Opus, mp4a, fLaC, ipcm}.
  //
  // This can be represented in uint8_t as:
  // primary_profile (2bits) + additional_profile (2bits) + iamf_codec (4bits),
  // where iamf_codec is represented using the Codec enum.
  //
  // Since iamf_codec is limited to 16 values, subtract the value of kCodecAudio
  // to ensure it fits. If future audio codecs are added to the Codec enum,
  // it may break the assumption that IAMF supported codecs are present within
  // the first 16 audio codec entries.
  // Further, if these values change in future version of IAMF, this format may
  // need to be changed, and AudioStreamInfo::GetCodecString needs to be updated
  // accordingly.
  codec_string_info =
      ((primary_profile << 6) | ((additional_profile << 4) & 0x3F) |
       ((iamf_codec - kCodecAudio) & 0xF));

  return true;
}

}  // namespace media
}  // namespace shaka
