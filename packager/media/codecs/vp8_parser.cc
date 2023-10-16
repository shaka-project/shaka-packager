// Copyright 2015 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/media/codecs/vp8_parser.h>

#include <absl/log/check.h>
#include <absl/log/log.h>

#include <packager/macros/logging.h>
#include <packager/media/base/bit_reader.h>
#include <packager/media/base/rcheck.h>

namespace shaka {
namespace media {
namespace {

const uint32_t MB_FEATURE_TREE_PROBS = 3;
const uint32_t MAX_MB_SEGMENTS = 4;
const uint32_t MAX_REF_LF_DELTAS = 4;
const uint32_t MAX_MODE_LF_DELTAS = 4;
const uint32_t MB_LVL_MAX = 2;
const uint32_t MB_FEATURE_DATA_BITS[MB_LVL_MAX] = {7, 6};

bool VerifySyncCode(const uint8_t* data) {
  return data[0] == 0x9d && data[1] == 0x01 && data[2] == 0x2a;
}

bool ReadSegmentation(BitReader* reader) {
  bool enabled;
  RCHECK(reader->ReadBits(1, &enabled));
  if (!enabled)
    return true;

  bool update_map;
  RCHECK(reader->ReadBits(1, &update_map));
  bool update_data;
  RCHECK(reader->ReadBits(1, &update_data));

  if (update_data) {
    RCHECK(reader->SkipBits(1));  // abs_delta
    for (uint32_t i = 0; i < MAX_MB_SEGMENTS; ++i)
      for (uint32_t j = 0; j < MB_LVL_MAX; ++j) {
        RCHECK(reader->SkipBitsConditional(true, MB_FEATURE_DATA_BITS[j] + 1));
    }
  }
  if (update_map) {
    for (uint32_t i = 0; i < MB_FEATURE_TREE_PROBS; ++i)
      RCHECK(reader->SkipBitsConditional(true, 8));
  }
  return true;
}

bool ReadLoopFilter(BitReader* reader) {
  RCHECK(reader->SkipBits(10));  // filter_type, filter_evel, sharness_level

  bool mode_ref_delta_enabled;
  RCHECK(reader->ReadBits(1, &mode_ref_delta_enabled));
  if (!mode_ref_delta_enabled)
    return true;
  bool mode_ref_delta_update;
  RCHECK(reader->ReadBits(1, &mode_ref_delta_update));
  if (!mode_ref_delta_update)
    return true;

  for (uint32_t i = 0; i < MAX_REF_LF_DELTAS + MAX_MODE_LF_DELTAS; ++i)
    RCHECK(reader->SkipBitsConditional(true, 6 + 1));
  return true;
}

bool ReadQuantization(BitReader* reader) {
  uint32_t yac_index;
  RCHECK(reader->ReadBits(7, &yac_index));
  VLOG(4) << "yac_index: " << yac_index;
  RCHECK(reader->SkipBitsConditional(true, 4 + 1));  // y dc delta
  RCHECK(reader->SkipBitsConditional(true, 4 + 1));  // y2 dc delta
  RCHECK(reader->SkipBitsConditional(true, 4 + 1));  // y2 ac delta
  RCHECK(reader->SkipBitsConditional(true, 4 + 1));  // chroma dc delta
  RCHECK(reader->SkipBitsConditional(true, 4 + 1));  // chroma ac delta
  return true;
}

bool ReadRefreshFrame(BitReader* reader) {
  bool refresh_golden_frame;
  RCHECK(reader->ReadBits(1, &refresh_golden_frame));
  bool refresh_altref_frame;
  RCHECK(reader->ReadBits(1, &refresh_altref_frame));
  if (!refresh_golden_frame)
    RCHECK(reader->SkipBits(2));  // buffer copy flag
  if (!refresh_altref_frame)
    RCHECK(reader->SkipBits(2));  // buffer copy flag
  RCHECK(reader->SkipBits(2));    // sign bias flags
  return true;
}

}  // namespace

VP8Parser::VP8Parser() : width_(0), height_(0) {}
VP8Parser::~VP8Parser() {}

bool VP8Parser::Parse(const uint8_t* data,
                      size_t data_size,
                      std::vector<VPxFrameInfo>* vpx_frames) {
  DCHECK(data);
  DCHECK(vpx_frames);

  BitReader reader(data, data_size);
  // The following 3 bytes are read directly from |data|.
  RCHECK(reader.SkipBytes(3));

  // One bit for frame type.
  bool is_interframe = data[0] & 1;
  // 3-bit version number with 2 bits for profile and the other bit reserved for
  // future variants.
  uint8_t profile = (data[0] >> 1) & 3;
  // One bit for show frame flag.
  // Then 19 bits (the remaining 3 bits in the first byte + next two bytes) for
  // header size.
  uint32_t header_size = (data[0] | (data[1] << 8) | (data[2] << 16)) >> 5;
  RCHECK(header_size <= data_size);

  if (!is_interframe) {
    // The following 7 bytes are read directly from |data|.
    RCHECK(reader.SkipBytes(7));

    RCHECK(VerifySyncCode(&data[3]));

    // Bits 0b11000000 for data[7] and data[9] are scaling.
    width_ = data[6] | ((data[7] & 0x3f) << 8);
    height_ = data[8] | ((data[9] & 0x3f) << 8);

    RCHECK(reader.SkipBits(2));  // colorspace and pixel value clamping.
  }

  RCHECK(ReadSegmentation(&reader));
  RCHECK(ReadLoopFilter(&reader));
  RCHECK(reader.SkipBits(2));  // partitions bits
  RCHECK(ReadQuantization(&reader));

  if (is_interframe) {
    RCHECK(ReadRefreshFrame(&reader));
    RCHECK(reader.SkipBits(1));  // refresh_entropy_probs
    RCHECK(reader.SkipBits(1));  // refresh last frame flag
  } else {
    RCHECK(reader.SkipBits(1));  // refresh_entropy_probs
  }

  // The next field is entropy header (coef probability tree), which is encoded
  // using bool entropy encoder, i.e. compressed. We don't consider it as part
  // of uncompressed header.

  writable_codec_config()->set_profile(profile);
  // VP8 uses an 8-bit YUV 4:2:0 format.
  // http://tools.ietf.org/html/rfc6386 Section 2.
  writable_codec_config()->set_bit_depth(8);
  writable_codec_config()->SetChromaSubsampling(
      VPCodecConfigurationRecord::CHROMA_420_COLLOCATED_WITH_LUMA);

  VPxFrameInfo vpx_frame;
  vpx_frame.frame_size = data_size;
  vpx_frame.uncompressed_header_size =
      vpx_frame.frame_size - reader.bits_available() / 8;
  vpx_frame.is_keyframe = !is_interframe;
  vpx_frame.width = width_;
  vpx_frame.height = height_;

  vpx_frames->clear();
  vpx_frames->push_back(vpx_frame);

  VLOG(3) << "\n frame_size: " << vpx_frame.frame_size
          << "\n uncompressed_header_size: "
          << vpx_frame.uncompressed_header_size
          << "\n bits read: " << reader.bit_position()
          << "\n header_size: " << header_size
          << "\n width: " << vpx_frame.width
          << "\n height: " << vpx_frame.height;
  return true;
}

bool VP8Parser::IsKeyframe(const uint8_t* data, size_t data_size) {
  // Make sure the block is big enough for the minimal keyframe header size.
  if (data_size < 10)
    return false;

  // The LSb of the first byte must be a 0 for a keyframe.
  if ((data[0] & 0x01) != 0)
    return false;
  return VerifySyncCode(&data[3]);
}

}  // namespace media
}  // namespace shaka
