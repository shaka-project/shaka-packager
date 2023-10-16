// Copyright 2018 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/media/crypto/subsample_generator.h>

#include <algorithm>
#include <limits>

#include <absl/log/check.h>

#include <packager/macros/compiler.h>
#include <packager/media/base/decrypt_config.h>
#include <packager/media/base/video_stream_info.h>
#include <packager/media/codecs/av1_parser.h>
#include <packager/media/codecs/video_slice_header_parser.h>
#include <packager/media/codecs/vp8_parser.h>
#include <packager/media/codecs/vp9_parser.h>

namespace shaka {
namespace media {
namespace {

const size_t kAesBlockSize = 16u;

uint8_t GetNaluLengthSize(const StreamInfo& stream_info) {
  if (stream_info.stream_type() != kStreamVideo)
    return 0;

  const VideoStreamInfo& video_stream_info =
      static_cast<const VideoStreamInfo&>(stream_info);
  return video_stream_info.nalu_length_size();
}

bool ShouldAlignProtectedData(Codec codec,
                              FourCC protection_scheme,
                              bool vp9_subsample_encryption) {
  switch (codec) {
    case kCodecVP9:
      // "VP Codec ISO Media File Format Binding" document requires that the
      // encrypted bytes of each frame within the superframe must be block
      // aligned so that the counter state can be computed for each frame
      // within the superframe.
      // ISO/IEC 23001-7:2016 10.2 'cbc1' 10.3 'cens'
      // The BytesOfProtectedData size SHALL be a multiple of 16 bytes to
      // avoid partial blocks in Subsamples.
      // For consistency, apply block alignment to all frames when VP9 subsample
      // encryption is enabled.
      return vp9_subsample_encryption;
    default:
      // ISO/IEC 23001-7:2016 10.2 'cbc1' 10.3 'cens'
      // The BytesOfProtectedData size SHALL be a multiple of 16 bytes to avoid
      // partial blocks in Subsamples.
      // CMAF requires 'cenc' scheme BytesOfProtectedData SHALL be a multiple of
      // 16 bytes; while 'cbcs' scheme BytesOfProtectedData SHALL start on the
      // first byte of video data following the slice header.
      // https://aomediacodec.github.io/av1-isobmff/#subsample-encryption AV1
      // has a similar clause.
      return protection_scheme == FOURCC_cbc1 ||
             protection_scheme == FOURCC_cens ||
             protection_scheme == FOURCC_cenc;
  }
}

// A convenient util class to organize subsamples, e.g. combine consecutive
// subsamples with only clear bytes, split subsamples if the clear bytes exceeds
// 2^16 etc.
class SubsampleOrganizer {
 public:
  SubsampleOrganizer(bool align_protected_data,
                     std::vector<SubsampleEntry>* subsamples)
      : align_protected_data_(align_protected_data), subsamples_(subsamples) {}

  ~SubsampleOrganizer() {
    if (accumulated_clear_bytes_ > 0) {
      PushSubsample(accumulated_clear_bytes_, 0);
      accumulated_clear_bytes_ = 0;
    }
  }

  void AddSubsample(size_t clear_bytes, size_t cipher_bytes) {
    DCHECK_LT(clear_bytes, std::numeric_limits<uint32_t>::max());
    DCHECK_LT(cipher_bytes, std::numeric_limits<uint32_t>::max());

    if (align_protected_data_ && cipher_bytes != 0) {
      const size_t misalign_bytes = cipher_bytes % kAesBlockSize;
      clear_bytes += misalign_bytes;
      cipher_bytes -= misalign_bytes;
    }

    accumulated_clear_bytes_ += clear_bytes;
    // Accumulated clear bytes are handled later.
    if (cipher_bytes == 0)
      return;

    PushSubsample(accumulated_clear_bytes_, cipher_bytes);
    accumulated_clear_bytes_ = 0;
  }

 private:
  SubsampleOrganizer(const SubsampleOrganizer&) = delete;
  SubsampleOrganizer& operator=(const SubsampleOrganizer&) = delete;

  void PushSubsample(size_t clear_bytes, size_t cipher_bytes) {
    const uint16_t kUInt16Max = std::numeric_limits<uint16_t>::max();
    while (clear_bytes > kUInt16Max) {
      subsamples_->emplace_back(kUInt16Max, 0);
      clear_bytes -= kUInt16Max;
    }
    subsamples_->emplace_back(static_cast<uint16_t>(clear_bytes),
                              static_cast<uint32_t>(cipher_bytes));
  }

  const bool align_protected_data_ = false;
  std::vector<SubsampleEntry>* const subsamples_ = nullptr;
  size_t accumulated_clear_bytes_ = 0;
};

}  // namespace

SubsampleGenerator::SubsampleGenerator(bool vp9_subsample_encryption)
    : vp9_subsample_encryption_(vp9_subsample_encryption) {}

SubsampleGenerator::~SubsampleGenerator() {}

Status SubsampleGenerator::Initialize(FourCC protection_scheme,
                                      const StreamInfo& stream_info) {
  codec_ = stream_info.codec();
  nalu_length_size_ = GetNaluLengthSize(stream_info);

  switch (codec_) {
    case kCodecAV1:
      av1_parser_.reset(new AV1Parser);
      break;
    case kCodecVP9:
      if (vp9_subsample_encryption_)
        vpx_parser_.reset(new VP9Parser);
      break;
    case kCodecH264:
      header_parser_.reset(new H264VideoSliceHeaderParser);
      break;
    case kCodecH265:
    case kCodecH265DolbyVision:
      header_parser_.reset(new H265VideoSliceHeaderParser);
      break;
    default:
      // Other codecs should have nalu length size == 0.
      if (nalu_length_size_ > 0) {
        LOG(WARNING) << "Unknown video codec '" << codec_ << "'";
        return Status(error::ENCRYPTION_FAILURE, "Unknown video codec.");
      }
  }
  if (av1_parser_) {
    // Parse configOBUs in AV1CodecConfigurationRecord if exists.
    // https://aomediacodec.github.io/av1-isobmff/#av1codecconfigurationbox-syntax.
    const size_t kConfigOBUsOffset = 4;
    const bool has_config_obus =
        stream_info.codec_config().size() > kConfigOBUsOffset;
    std::vector<AV1Parser::Tile> tiles;
    if (has_config_obus &&
        !av1_parser_->Parse(
            &stream_info.codec_config()[kConfigOBUsOffset],
            stream_info.codec_config().size() - kConfigOBUsOffset, &tiles)) {
      return Status(
          error::ENCRYPTION_FAILURE,
          "Failed to parse configOBUs in AV1CodecConfigurationRecord.");
    }
    DCHECK(tiles.empty());
  }
  if (header_parser_) {
    CHECK_NE(nalu_length_size_, 0u) << "AnnexB stream is not supported yet";
    if (!header_parser_->Initialize(stream_info.codec_config())) {
      return Status(error::ENCRYPTION_FAILURE,
                    "Failed to read SPS and PPS data.");
    }
  }

  align_protected_data_ = ShouldAlignProtectedData(codec_, protection_scheme,
                                                   vp9_subsample_encryption_);

  if (protection_scheme == kAppleSampleAesProtectionScheme) {
    const size_t kH264LeadingClearBytesSize = 32u;
    const size_t kAudioLeadingClearBytesSize = 16u;
    switch (codec_) {
      case kCodecH264:
        leading_clear_bytes_size_ = kH264LeadingClearBytesSize;
        min_protected_data_size_ =
            leading_clear_bytes_size_ + kAesBlockSize + 1u;
        break;
      case kCodecAAC:
        FALLTHROUGH_INTENDED;
      case kCodecAC3:
        leading_clear_bytes_size_ = kAudioLeadingClearBytesSize;
        min_protected_data_size_ = leading_clear_bytes_size_ + kAesBlockSize;
        break;
      case kCodecEAC3:
        // E-AC3 encryption is handled by SampleAesEc3Cryptor, which also
        // manages leading clear bytes.
        leading_clear_bytes_size_ = 0;
        min_protected_data_size_ = leading_clear_bytes_size_ + kAesBlockSize;
        break;
      default:
        LOG(ERROR) << "Unexpected codec for SAMPLE-AES " << codec_;
        return Status(error::ENCRYPTION_FAILURE,
                      "Unexpected codec for SAMPLE-AES.");
    }
  }
  return Status::OK;
}

Status SubsampleGenerator::GenerateSubsamples(
    const uint8_t* frame,
    size_t frame_size,
    std::vector<SubsampleEntry>* subsamples) {
  subsamples->clear();
  switch (codec_) {
    case kCodecAV1:
      return GenerateSubsamplesFromAV1Frame(frame, frame_size, subsamples);
    case kCodecH264:
      FALLTHROUGH_INTENDED;
    case kCodecH265:
    case kCodecH265DolbyVision:
      return GenerateSubsamplesFromH26xFrame(frame, frame_size, subsamples);
    case kCodecVP9:
      if (vp9_subsample_encryption_)
        return GenerateSubsamplesFromVPxFrame(frame, frame_size, subsamples);
      // Full sample encrypted so no subsamples.
      break;
    default:
      // Other codecs are full sample encrypted unless there are clear leading
      // bytes.
      if (leading_clear_bytes_size_ > 0) {
        SubsampleOrganizer subsample_organizer(align_protected_data_,
                                               subsamples);
        const size_t clear_bytes =
            std::min(frame_size, leading_clear_bytes_size_);
        const size_t cipher_bytes = frame_size - clear_bytes;
        subsample_organizer.AddSubsample(clear_bytes, cipher_bytes);
      } else {
        // Full sample encrypted so no subsamples.
      }
      break;
  }
  return Status::OK;
}

void SubsampleGenerator::InjectVpxParserForTesting(
    std::unique_ptr<VPxParser> vpx_parser) {
  vpx_parser_ = std::move(vpx_parser);
}

void SubsampleGenerator::InjectVideoSliceHeaderParserForTesting(
    std::unique_ptr<VideoSliceHeaderParser> header_parser) {
  header_parser_ = std::move(header_parser);
}

void SubsampleGenerator::InjectAV1ParserForTesting(
    std::unique_ptr<AV1Parser> av1_parser) {
  av1_parser_ = std::move(av1_parser);
}

Status SubsampleGenerator::GenerateSubsamplesFromVPxFrame(
    const uint8_t* frame,
    size_t frame_size,
    std::vector<SubsampleEntry>* subsamples) {
  DCHECK(vpx_parser_);
  std::vector<VPxFrameInfo> vpx_frames;
  if (!vpx_parser_->Parse(frame, frame_size, &vpx_frames))
    return Status(error::ENCRYPTION_FAILURE, "Failed to parse vpx frame.");

  SubsampleOrganizer subsample_organizer(align_protected_data_, subsamples);

  size_t total_size = 0;
  for (const VPxFrameInfo& vpx_frame : vpx_frames) {
    subsample_organizer.AddSubsample(
        vpx_frame.uncompressed_header_size,
        vpx_frame.frame_size - vpx_frame.uncompressed_header_size);
    total_size += vpx_frame.frame_size;
  }
  // Add subsample for the superframe index if exists.
  const bool is_superframe = vpx_frames.size() > 1;
  if (is_superframe) {
    const size_t index_size = frame_size - total_size;
    DCHECK_LE(index_size, 2 + vpx_frames.size() * 4);
    DCHECK_GE(index_size, 2 + vpx_frames.size() * 1);
    subsample_organizer.AddSubsample(index_size, 0);
  } else {
    DCHECK_EQ(total_size, frame_size);
  }
  return Status::OK;
}

Status SubsampleGenerator::GenerateSubsamplesFromH26xFrame(
    const uint8_t* frame,
    size_t frame_size,
    std::vector<SubsampleEntry>* subsamples) {
  DCHECK_NE(nalu_length_size_, 0u);
  DCHECK(header_parser_);

  SubsampleOrganizer subsample_organizer(align_protected_data_, subsamples);

  const Nalu::CodecType nalu_type =
      (codec_ == kCodecH265 || codec_ == kCodecH265DolbyVision) ? Nalu::kH265
                                                                : Nalu::kH264;
  NaluReader reader(nalu_type, nalu_length_size_, frame, frame_size);

  Nalu nalu;
  NaluReader::Result result;
  while ((result = reader.Advance(&nalu)) == NaluReader::kOk) {
    // |header_parser_| is only used if |leading_clear_bytes_size_| is not
    // availble. See lines below.
    if (leading_clear_bytes_size_ == 0 && !header_parser_->ProcessNalu(nalu)) {
      LOG(ERROR) << "Failed to process NAL unit: NAL type = " << nalu.type();
      return Status(error::ENCRYPTION_FAILURE, "Failed to process NAL unit.");
    }

    const size_t nalu_total_size = nalu.header_size() + nalu.payload_size();
    size_t clear_bytes = 0;
    if (nalu.is_video_slice() && nalu_total_size >= min_protected_data_size_) {
      clear_bytes = leading_clear_bytes_size_;
      if (clear_bytes == 0) {
        // For video-slice NAL units, encrypt the video slice.  This skips
        // the frame header.
        const int64_t video_slice_header_size =
            header_parser_->GetHeaderSize(nalu);
        if (video_slice_header_size < 0) {
          LOG(ERROR) << "Failed to read slice header.";
          return Status(error::ENCRYPTION_FAILURE,
                        "Failed to read slice header.");
        }
        clear_bytes = nalu.header_size() + video_slice_header_size;
      }
    } else {
      // For non-video-slice or small NAL units, don't encrypt.
      clear_bytes = nalu_total_size;
    }
    const size_t cipher_bytes = nalu_total_size - clear_bytes;
    subsample_organizer.AddSubsample(nalu_length_size_ + clear_bytes,
                                     cipher_bytes);
  }
  if (result != NaluReader::kEOStream) {
    LOG(ERROR) << "Failed to parse NAL units.";
    return Status(error::ENCRYPTION_FAILURE, "Failed to parse NAL units.");
  }
  return Status::OK;
}

Status SubsampleGenerator::GenerateSubsamplesFromAV1Frame(
    const uint8_t* frame,
    size_t frame_size,
    std::vector<SubsampleEntry>* subsamples) {
  DCHECK(av1_parser_);
  std::vector<AV1Parser::Tile> av1_tiles;
  if (!av1_parser_->Parse(frame, frame_size, &av1_tiles))
    return Status(error::ENCRYPTION_FAILURE, "Failed to parse AV1 frame.");

  SubsampleOrganizer subsample_organizer(align_protected_data_, subsamples);

  size_t last_tile_end_offset = 0;
  for (const AV1Parser::Tile& tile : av1_tiles) {
    DCHECK_LE(last_tile_end_offset, tile.start_offset_in_bytes);
    // Per AV1 in ISO-BMFF spec [1], only decode_tile is encrypted.
    // [1] https://aomediacodec.github.io/av1-isobmff/#subsample-encryption
    subsample_organizer.AddSubsample(
        tile.start_offset_in_bytes - last_tile_end_offset, tile.size_in_bytes);
    last_tile_end_offset = tile.start_offset_in_bytes + tile.size_in_bytes;
  }
  DCHECK_LE(last_tile_end_offset, frame_size);
  if (last_tile_end_offset < frame_size)
    subsample_organizer.AddSubsample(frame_size - last_tile_end_offset, 0);
  return Status::OK;
}

}  // namespace media
}  // namespace shaka
