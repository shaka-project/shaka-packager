// Copyright 2016 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/filters/nalu_reader.h"

#include <iostream>

#include "packager/base/logging.h"
#include "packager/media/base/buffer_reader.h"
#include "packager/media/filters/h264_parser.h"

namespace edash_packager {
namespace media {

namespace {
inline bool IsStartCode(const uint8_t* data) {
  return data[0] == 0x00 && data[1] == 0x00 && data[2] == 0x01;
}
}  // namespace

Nalu::Nalu()
    : data_(nullptr),
      header_size_(0),
      payload_size_(0),
      ref_idc_(0),
      nuh_layer_id_(0),
      nuh_temporal_id_(0),
      type_(0),
      is_video_slice_(false) {}

// ITU-T H.264 (02/2014) 7.4.1 NAL unit semantics
bool Nalu::InitializeFromH264(const uint8_t* data, uint64_t size) {
  DCHECK(data);
  if (size == 0)
    return false;
  const uint8_t header = data[0];
  if ((header & 0x80) != 0) {
    LOG(WARNING) << "forbidden_zero_bit shall be equal to 0 (header 0x"
                 << std::hex << static_cast<int>(header) << ").";
    return false;
  }

  data_ = data;
  header_size_ = 1;
  payload_size_ = size - header_size_;
  ref_idc_ = (header >> 5) & 0x3;
  type_ = header & 0x1F;

  // Reserved NAL units are not treated as valid NAL units here.
  if (type_ == Nalu::H264_Unspecified || type_ == Nalu::H264_Reserved17 ||
      type_ == Nalu::H264_Reserved18 || type_ >= Nalu::H264_Reserved22) {
    LOG(WARNING) << "Unspecified or reserved nal_unit_type " << type_
                 << " (header 0x" << std::hex << static_cast<int>(header)
                 << ").";
    return false;
  } else if (type_ == Nalu::H264_IDRSlice || type_ == Nalu::H264_SPS ||
      type_ == Nalu::H264_SPSExtension || type_ == Nalu::H264_SubsetSPS ||
      type_ == Nalu::H264_PPS) {
    if (ref_idc_ == 0) {
      LOG(WARNING) << "nal_ref_idc shall not be equal to 0 for nalu type "
                   << type_ << " (header 0x" << std::hex
                   << static_cast<int>(header) << ").";
      return false;
    }
  } else if (type_ == Nalu::H264_SEIMessage ||
             (type_ >= Nalu::H264_AUD && type_ <= Nalu::H264_FillerData)) {
    if (ref_idc_ != 0) {
      LOG(WARNING) << "nal_ref_idc shall be equal to 0 for nalu type " << type_
                   << " (header 0x" << std::hex << static_cast<int>(header)
                   << ").";
      return false;
    }
  }

  is_video_slice_ = (type_ >= Nalu::H264_NonIDRSlice &&
                     type_ <= Nalu::H264_IDRSlice);
  return true;
}

// ITU-T H.265 (04/2015) 7.4.2.2 NAL unit header semantics
bool Nalu::InitializeFromH265(const uint8_t* data, uint64_t size) {
  DCHECK(data);
  if (size < 2)
    return false;
  const uint16_t header = (data[0] << 8) | data[1];
  if ((header & 0x8000) != 0) {
    LOG(WARNING) << "forbidden_zero_bit shall be equal to 0 (header 0x"
                 << std::hex << header << ").";
    return false;
  }

  data_ = data;
  header_size_ = 2;
  payload_size_ = size - header_size_;

  type_ = (header >> 9) & 0x3F;
  nuh_layer_id_ = (header >> 3) & 0x3F;
  const int nuh_temporal_id_plus1 = header & 0x7;
  if (nuh_temporal_id_plus1 == 0) {
    LOG(WARNING) << "nul_temporal_id_plus1 shall not be equal to 0 (header 0x"
                 << std::hex << header << ").";
    return false;
  }
  nuh_temporal_id_ = nuh_temporal_id_plus1 - 1;

  if (type_ == Nalu::H265_EOB && nuh_layer_id_ != 0) {
    LOG(WARNING) << "nuh_layer_id shall be equal to 0 for nalu type " << type_
                 << " (header 0x" << std::hex << header << ").";
    return false;
  }

  // Reserved NAL units are not treated as valid NAL units here.
  if ((type_ >= Nalu::H265_RSV_VCL_N10 && type_ <= Nalu::H265_RSV_VCL_R15) ||
      (type_ >= Nalu::H265_RSV_IRAP_VCL22 && type_ < Nalu::H265_RSV_VCL31) ||
      (type_ >= Nalu::H265_RSV_NVCL41)) {
    LOG(WARNING) << "Unspecified or reserved nal_unit_type " << type_
                 << " (header 0x" << std::hex << header << ").";
    return false;
  } else if ((type_ >= Nalu::H265_BLA_W_LP &&
              type_ <= Nalu::H265_RSV_IRAP_VCL23) ||
             type_ == Nalu::H265_VPS || type_ == Nalu::H265_SPS ||
             type_ == Nalu::H265_EOS || type_ == Nalu::H265_EOB) {
    if (nuh_temporal_id_ != 0) {
      LOG(WARNING) << "TemporalId shall be equal to 0 for nalu type " << type_
                   << " (header 0x" << std::hex << header << ").";
      return false;
    }
  } else if (type_ == Nalu::H265_TSA_N || type_ == Nalu::H265_TSA_R ||
             (nuh_layer_id_ == 0 &&
              (type_ == Nalu::H265_STSA_N || type_ == Nalu::H265_STSA_R))) {
    if (nuh_temporal_id_ == 0) {
      LOG(WARNING) << "TemporalId shall not be equal to 0 for nalu type "
                   << type_ << " (header 0x" << std::hex << header << ").";
      return false;
    }
  }

  is_video_slice_ = type_ >= Nalu::H265_TRAIL_N && type_ <= Nalu::H265_CRA_NUT;
  return true;
}

NaluReader::NaluReader(CodecType type,
                       uint8_t nal_length_size,
                       const uint8_t* stream,
                       uint64_t stream_size)
    : stream_(stream),
      stream_size_(stream_size),
      nalu_type_(type),
      nalu_length_size_(nal_length_size),
      format_(nal_length_size == 0 ? kAnnexbByteStreamFormat
                                   : kNalUnitStreamFormat) {
  DCHECK(stream);
}
NaluReader::~NaluReader() {}

NaluReader::Result NaluReader::Advance(Nalu* nalu) {
  if (stream_size_ <= 0)
    return NaluReader::kEOStream;

  uint8_t nalu_length_size_or_start_code_size;
  uint64_t nalu_length;
  if (format_ == kAnnexbByteStreamFormat) {
    // This will move |stream_| to the start code.
    uint64_t nalu_length_with_header;
    if (!LocateNaluByStartCode(&nalu_length_with_header,
                               &nalu_length_size_or_start_code_size)) {
      LOG(ERROR) << "Could not find next NALU, bytes left in stream: "
                 << stream_size_;
      // This is actually an error.  Since we always move to past the end of
      // each NALU, if there is no next start code, then this is the first call
      // and there are no start codes in the stream.
      return NaluReader::kInvalidStream;
    }
    nalu_length = nalu_length_with_header - nalu_length_size_or_start_code_size;
  } else {
    BufferReader reader(stream_, stream_size_);
    if (!reader.ReadNBytesInto8(&nalu_length, nalu_length_size_))
      return NaluReader::kInvalidStream;
    nalu_length_size_or_start_code_size = nalu_length_size_;

    if (nalu_length + nalu_length_size_ > stream_size_) {
      LOG(ERROR) << "NALU length exceeds stream size: "
                 << stream_size_ << " < " << nalu_length;
      return NaluReader::kInvalidStream;
    }
    if (nalu_length == 0) {
      LOG(ERROR) << "NALU size 0";
      return NaluReader::kInvalidStream;
    }
  }

  const uint8_t* nalu_data = stream_ + nalu_length_size_or_start_code_size;
  if (nalu_type_ == kH264) {
    if (!nalu->InitializeFromH264(nalu_data, nalu_length))
      return NaluReader::kInvalidStream;
  } else {
    DCHECK_EQ(kH265, nalu_type_);
    if (!nalu->InitializeFromH265(nalu_data, nalu_length))
      return NaluReader::kInvalidStream;
  }

  // Move parser state to after this NALU, so next time Advance
  // is called, we will effectively be skipping it.
  stream_ += nalu_length_size_or_start_code_size + nalu_length;
  stream_size_ -= nalu_length_size_or_start_code_size + nalu_length;

  DVLOG(4) << "NALU type: " << static_cast<int>(nalu->type())
           << " at: " << reinterpret_cast<const void*>(nalu->data())
           << " data size: " << nalu->payload_size();

  return NaluReader::kOk;
}

bool NaluReader::StartsWithStartCode() {
  if (stream_size_ >= 3) {
    if (IsStartCode(stream_))
      return true;
  }
  if (stream_size_ >= 4) {
    if (stream_[0] == 0x00 && IsStartCode(stream_ + 1))
      return true;
  }
  return false;
}

// static
bool NaluReader::FindStartCode(const uint8_t* data,
                               uint64_t data_size,
                               uint64_t* offset,
                               uint8_t* start_code_size) {
  uint64_t bytes_left = data_size;

  while (bytes_left >= 3) {
    if (IsStartCode(data)) {
      // Found three-byte start code, set pointer at its beginning.
      *offset = data_size - bytes_left;
      *start_code_size = 3;

      // If there is a zero byte before this start code,
      // then it's actually a four-byte start code, so backtrack one byte.
      if (*offset > 0 && *(data - 1) == 0x00) {
        --(*offset);
        ++(*start_code_size);
      }

      return true;
    }

    ++data;
    --bytes_left;
  }

  // End of data: offset is pointing to the first byte that was not considered
  // as a possible start of a start code.
  *offset = data_size - bytes_left;
  *start_code_size = 0;
  return false;
}

bool NaluReader::LocateNaluByStartCode(uint64_t* nalu_size,
                                       uint8_t* start_code_size) {
  // Find the start code of next NALU.
  uint64_t nalu_start_off = 0;
  uint8_t annexb_start_code_size = 0;
  if (!FindStartCode(stream_, stream_size_,
                     &nalu_start_off, &annexb_start_code_size)) {
    DVLOG(4) << "Could not find start code, end of stream?";
    return false;
  }

  // Move the stream to the beginning of the NALU (pointing at the start code).
  stream_ += nalu_start_off;
  stream_size_ -= nalu_start_off;

  const uint8_t* nalu_data = stream_ + annexb_start_code_size;
  uint64_t max_nalu_data_size = stream_size_ - annexb_start_code_size;
  if (max_nalu_data_size <= 0) {
    DVLOG(3) << "End of stream";
    return false;
  }

  // Find the start code of next NALU;
  // if successful, |nalu_size_without_start_code| is the number of bytes from
  // after previous start code to before this one;
  // if next start code is not found, it is still a valid NALU since there
  // are some bytes left after the first start code: all the remaining bytes
  // belong to the current NALU.
  uint64_t nalu_size_without_start_code = 0;
  uint8_t next_start_code_size = 0;
  while (true) {
    if (!FindStartCode(nalu_data, max_nalu_data_size,
                       &nalu_size_without_start_code, &next_start_code_size)) {
      nalu_data += max_nalu_data_size;
      break;
    }

    nalu_data += nalu_size_without_start_code + next_start_code_size;
    max_nalu_data_size -= nalu_size_without_start_code + next_start_code_size;
    // If it is not a valid NAL unit, we will continue searching. This is to
    // handle the case where emulation prevention are not applied.
    Nalu nalu;
    if (nalu_type_ == kH264
            ? nalu.InitializeFromH264(nalu_data, max_nalu_data_size)
            : nalu.InitializeFromH265(nalu_data, max_nalu_data_size)) {
      nalu_data -= next_start_code_size;
      break;
    }
    LOG(WARNING) << "Seeing invalid NAL unit. Emulation prevention may not "
                    "have been applied properly. Assuming it is part of the "
                    "previous NAL unit.";
  }
  *nalu_size = nalu_data - stream_;
  *start_code_size = annexb_start_code_size;
  return true;
}

}  // namespace media
}  // namespace edash_packager
