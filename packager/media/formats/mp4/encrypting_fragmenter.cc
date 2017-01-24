// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/formats/mp4/encrypting_fragmenter.h"

#include <limits>

#include "packager/media/base/aes_encryptor.h"
#include "packager/media/base/aes_pattern_cryptor.h"
#include "packager/media/base/buffer_reader.h"
#include "packager/media/base/key_source.h"
#include "packager/media/base/media_sample.h"
#include "packager/media/codecs/nalu_reader.h"
#include "packager/media/codecs/vp8_parser.h"
#include "packager/media/codecs/vp9_parser.h"
#include "packager/media/formats/mp4/box_definitions.h"

namespace shaka {
namespace media {
namespace mp4 {

namespace {
const size_t kCencBlockSize = 16u;

// Adds one or more subsamples to |*subsamples|.  This may add more than one
// if one of the values overflows the integer in the subsample.
void AddSubsamples(uint64_t clear_bytes,
                   uint64_t cipher_bytes,
                   std::vector<SubsampleEntry>* subsamples) {
  CHECK_LT(cipher_bytes, std::numeric_limits<uint32_t>::max());
  const uint64_t kUInt16Max = std::numeric_limits<uint16_t>::max();
  while (clear_bytes > kUInt16Max) {
    subsamples->push_back(SubsampleEntry(kUInt16Max, 0));
    clear_bytes -= kUInt16Max;
  }

  if (clear_bytes > 0 || cipher_bytes > 0)
    subsamples->push_back(SubsampleEntry(clear_bytes, cipher_bytes));
}

Codec GetCodec(const StreamInfo& stream_info) {
  if (stream_info.stream_type() != kStreamVideo) return kUnknownCodec;
  const VideoStreamInfo& video_stream_info =
      static_cast<const VideoStreamInfo&>(stream_info);
  return video_stream_info.codec();
}

uint8_t GetNaluLengthSize(const StreamInfo& stream_info) {
  if (stream_info.stream_type() != kStreamVideo)
    return 0;

  const VideoStreamInfo& video_stream_info =
      static_cast<const VideoStreamInfo&>(stream_info);
  return video_stream_info.nalu_length_size();
}
}  // namespace

EncryptingFragmenter::EncryptingFragmenter(
    std::shared_ptr<StreamInfo> info,
    TrackFragment* traf,
    std::unique_ptr<EncryptionKey> encryption_key,
    int64_t clear_time,
    FourCC protection_scheme,
    uint8_t crypt_byte_block,
    uint8_t skip_byte_block,
    MuxerListener* listener)
    : Fragmenter(info, traf),
      info_(info),
      encryption_key_(std::move(encryption_key)),
      nalu_length_size_(GetNaluLengthSize(*info)),
      video_codec_(GetCodec(*info)),
      clear_time_(clear_time),
      protection_scheme_(protection_scheme),
      crypt_byte_block_(crypt_byte_block),
      skip_byte_block_(skip_byte_block),
      listener_(listener) {
  DCHECK(encryption_key_);
  switch (video_codec_) {
    case kCodecVP8:
      vpx_parser_.reset(new VP8Parser);
      break;
    case kCodecVP9:
      vpx_parser_.reset(new VP9Parser);
      break;
    case kCodecH264:
      header_parser_.reset(new H264VideoSliceHeaderParser);
      break;
    case kCodecHVC1:
      FALLTHROUGH_INTENDED;
    case kCodecHEV1:
      header_parser_.reset(new H265VideoSliceHeaderParser);
      break;
    default:
      if (nalu_length_size_ > 0) {
        LOG(WARNING) << "Unknown video codec '" << video_codec_
                     << "', whole subsamples will be encrypted.";
      }
  }
}

EncryptingFragmenter::~EncryptingFragmenter() {}

Status EncryptingFragmenter::AddSample(std::shared_ptr<MediaSample> sample) {
  DCHECK(sample);
  if (!fragment_initialized()) {
    Status status = InitializeFragment(sample->dts());
    if (!status.ok())
      return status;
  }
  if (encryptor_) {
    Status status = EncryptSample(sample);
    if (!status.ok())
      return status;
  }
  return Fragmenter::AddSample(sample);
}

Status EncryptingFragmenter::InitializeFragment(int64_t first_sample_dts) {
  Status status = Fragmenter::InitializeFragment(first_sample_dts);
  if (!status.ok())
    return status;

  if (header_parser_ && !header_parser_->Initialize(info_->codec_config()))
    return Status(error::MUXER_FAILURE, "Fail to read SPS and PPS data.");

  traf()->auxiliary_size.sample_info_sizes.clear();
  traf()->auxiliary_offset.offsets.clear();
  if (IsSubsampleEncryptionRequired()) {
    traf()->sample_encryption.flags |=
        SampleEncryption::kUseSubsampleEncryption;
  }
  traf()->sample_encryption.sample_encryption_entries.clear();

  const bool enable_encryption = clear_time_ <= 0;
  if (!enable_encryption) {
    // This fragment should be in clear text.
    // At most two sample description entries, an encrypted entry and a clear
    // entry, are generated. The 1-based clear entry index is always 2.
    const uint32_t kClearSampleDescriptionIndex = 2;

    traf()->header.flags |=
        TrackFragmentHeader::kSampleDescriptionIndexPresentMask;
    traf()->header.sample_description_index = kClearSampleDescriptionIndex;
  } else {
    if (listener_)
      listener_->OnEncryptionStart();
  }
  return PrepareFragmentForEncryption(enable_encryption);
}

void EncryptingFragmenter::FinalizeFragment() {
  if (encryptor_) {
    DCHECK_LE(clear_time_, 0);
    FinalizeFragmentForEncryption();
  } else {
    DCHECK_GT(clear_time_, 0);
    clear_time_ -= fragment_duration();
  }
  Fragmenter::FinalizeFragment();
}

Status EncryptingFragmenter::PrepareFragmentForEncryption(
    bool enable_encryption) {
  return (!enable_encryption || encryptor_) ? Status::OK : CreateEncryptor();
}

void EncryptingFragmenter::FinalizeFragmentForEncryption() {
  // The offset will be adjusted in Segmenter after knowing moof size.
  traf()->auxiliary_offset.offsets.push_back(0);

  // For 'cbcs' scheme, Constant IVs SHALL be used.
  const uint8_t per_sample_iv_size =
    (protection_scheme_ == FOURCC_cbcs) ? 0 :
    static_cast<uint8_t>(encryptor_->iv().size());
  traf()->sample_encryption.iv_size = per_sample_iv_size;

  // Optimize saiz box.
  SampleAuxiliaryInformationSize& saiz = traf()->auxiliary_size;
  saiz.sample_count =
      static_cast<uint32_t>(traf()->runs[0].sample_sizes.size());
  if (!saiz.sample_info_sizes.empty()) {
    if (!OptimizeSampleEntries(&saiz.sample_info_sizes,
                               &saiz.default_sample_info_size)) {
      saiz.default_sample_info_size = 0;
    }
  } else {
    // |sample_info_sizes| table is filled in only for subsample encryption,
    // otherwise |sample_info_size| is just the IV size.
    DCHECK(!IsSubsampleEncryptionRequired());
    saiz.default_sample_info_size = static_cast<uint8_t>(per_sample_iv_size);
  }

  // It should only happen with full sample encryption + constant iv, i.e.
  // 'cbcs' applying to audio.
  if (saiz.default_sample_info_size == 0 && saiz.sample_info_sizes.empty()) {
    DCHECK_EQ(protection_scheme_, FOURCC_cbcs);
    DCHECK(!IsSubsampleEncryptionRequired());
    // ISO/IEC 23001-7:2016(E) The sample auxiliary information would then be
    // empty and should be emitted. Clear saiz and saio boxes so they are not
    // written.
    saiz.sample_count = 0;
    traf()->auxiliary_offset.offsets.clear();
  }
}

Status EncryptingFragmenter::CreateEncryptor() {
  DCHECK(encryption_key_);
  std::unique_ptr<AesCryptor> encryptor;
  switch (protection_scheme_) {
    case FOURCC_cenc:
      encryptor.reset(new AesCtrEncryptor);
      break;
    case FOURCC_cbc1:
      encryptor.reset(new AesCbcEncryptor(kNoPadding));
      break;
    case FOURCC_cens:
      encryptor.reset(new AesPatternCryptor(
          crypt_byte_block(), skip_byte_block(),
          AesPatternCryptor::kEncryptIfCryptByteBlockRemaining,
          AesCryptor::kDontUseConstantIv,
          std::unique_ptr<AesCryptor>(new AesCtrEncryptor())));
      break;
    case FOURCC_cbcs:
      encryptor.reset(new AesPatternCryptor(
          crypt_byte_block(), skip_byte_block(),
          AesPatternCryptor::kEncryptIfCryptByteBlockRemaining,
          AesCryptor::kUseConstantIv,
          std::unique_ptr<AesCryptor>(new AesCbcEncryptor(kNoPadding))));
      break;
    default:
      return Status(error::MUXER_FAILURE, "Unsupported protection scheme.");
  }

  DCHECK(!encryption_key_->iv.empty());
  const bool initialized =
      encryptor->InitializeWithIv(encryption_key_->key, encryption_key_->iv);
  if (!initialized)
    return Status(error::MUXER_FAILURE, "Failed to create the encryptor.");
  encryptor_ = std::move(encryptor);
  return Status::OK;
}

void EncryptingFragmenter::EncryptBytes(uint8_t* data, size_t size) {
  DCHECK(encryptor_);
  CHECK(encryptor_->Crypt(data, size, data));
}

Status EncryptingFragmenter::EncryptSample(
    std::shared_ptr<MediaSample> sample) {
  DCHECK(encryptor_);

  SampleEncryptionEntry sample_encryption_entry;
  // For 'cbcs' scheme, Constant IVs SHALL be used.
  if (protection_scheme_ != FOURCC_cbcs)
    sample_encryption_entry.initialization_vector = encryptor_->iv();
  uint8_t* data = sample->writable_data();
  if (IsSubsampleEncryptionRequired()) {
    if (vpx_parser_) {
      std::vector<VPxFrameInfo> vpx_frames;
      if (!vpx_parser_->Parse(sample->data(), sample->data_size(),
                              &vpx_frames)) {
        return Status(error::MUXER_FAILURE, "Failed to parse vpx frame.");
      }

      const bool is_superframe = vpx_frames.size() > 1;
      for (const VPxFrameInfo& frame : vpx_frames) {
        SubsampleEntry subsample;
        subsample.clear_bytes =
            static_cast<uint16_t>(frame.uncompressed_header_size);
        subsample.cipher_bytes = static_cast<uint32_t>(
            frame.frame_size - frame.uncompressed_header_size);

        // "VP Codec ISO Media File Format Binding" document requires that the
        // encrypted bytes of each frame within the superframe must be block
        // aligned so that the counter state can be computed for each frame
        // within the superframe.
        // ISO/IEC 23001-7:2016 10.2 'cbc1' 10.3 'cens'
        // The BytesOfProtectedData size SHALL be a multiple of 16 bytes to
        // avoid partial blocks in Subsamples.
        if (is_superframe || protection_scheme_ == FOURCC_cbc1 ||
            protection_scheme_ == FOURCC_cens) {
          const uint16_t misalign_bytes =
              subsample.cipher_bytes % kCencBlockSize;
          subsample.clear_bytes += misalign_bytes;
          subsample.cipher_bytes -= misalign_bytes;
        }

        sample_encryption_entry.subsamples.push_back(subsample);
        if (subsample.cipher_bytes > 0)
          EncryptBytes(data + subsample.clear_bytes, subsample.cipher_bytes);
        data += frame.frame_size;
      }
      // Add subsample for the superframe index if exists.
      if (is_superframe) {
        size_t index_size = sample->data() + sample->data_size() - data;
        DCHECK_LE(index_size, 2 + vpx_frames.size() * 4);
        DCHECK_GE(index_size, 2 + vpx_frames.size() * 1);
        SubsampleEntry subsample;
        subsample.clear_bytes = static_cast<uint16_t>(index_size);
        subsample.cipher_bytes = 0;
        sample_encryption_entry.subsamples.push_back(subsample);
      }
    } else {
      const Nalu::CodecType nalu_type =
          (video_codec_ == kCodecHVC1 || video_codec_ == kCodecHEV1)
              ? Nalu::kH265
              : Nalu::kH264;
      NaluReader reader(nalu_type, nalu_length_size_, data,
                        sample->data_size());

      // Store the current length of clear data.  This is used to squash
      // multiple unencrypted NAL units into fewer subsample entries.
      uint64_t accumulated_clear_bytes = 0;

      Nalu nalu;
      NaluReader::Result result;
      while ((result = reader.Advance(&nalu)) == NaluReader::kOk) {
        if (nalu.is_video_slice()) {
          // For video-slice NAL units, encrypt the video slice.  This skips
          // the frame header.  If this is an unrecognized codec (e.g. H.265),
          // the whole NAL unit will be encrypted.
          const int64_t video_slice_header_size =
              header_parser_ ? header_parser_->GetHeaderSize(nalu) : 0;
          if (video_slice_header_size < 0)
            return Status(error::MUXER_FAILURE, "Failed to read slice header.");

          uint64_t current_clear_bytes =
              nalu.header_size() + video_slice_header_size;
          uint64_t cipher_bytes = nalu.payload_size() - video_slice_header_size;

          // ISO/IEC 23001-7:2016 10.2 'cbc1' 10.3 'cens'
          // The BytesOfProtectedData size SHALL be a multiple of 16 bytes to
          // avoid partial blocks in Subsamples.
          if (protection_scheme_ == FOURCC_cbc1 ||
              protection_scheme_ == FOURCC_cens) {
            const uint16_t misalign_bytes = cipher_bytes % kCencBlockSize;
            current_clear_bytes += misalign_bytes;
            cipher_bytes -= misalign_bytes;
          }

          const uint8_t* nalu_data = nalu.data() + current_clear_bytes;
          EncryptBytes(const_cast<uint8_t*>(nalu_data), cipher_bytes);

          AddSubsamples(
              accumulated_clear_bytes + nalu_length_size_ + current_clear_bytes,
              cipher_bytes, &sample_encryption_entry.subsamples);
          accumulated_clear_bytes = 0;
        } else {
          // For non-video-slice NAL units, don't encrypt.
          accumulated_clear_bytes +=
              nalu_length_size_ + nalu.header_size() + nalu.payload_size();
        }
      }
      if (result != NaluReader::kEOStream)
        return Status(error::MUXER_FAILURE, "Failed to parse NAL units.");
      AddSubsamples(accumulated_clear_bytes, 0,
                    &sample_encryption_entry.subsamples);
    }
    DCHECK_EQ(sample_encryption_entry.GetTotalSizeOfSubsamples(),
              sample->data_size());

    // The length of per-sample auxiliary datum, defined in CENC ch. 7.
    traf()->auxiliary_size.sample_info_sizes.push_back(
        sample_encryption_entry.ComputeSize());
  } else {
    DCHECK_LE(crypt_byte_block(), 1u);
    DCHECK_EQ(skip_byte_block(), 0u);
    EncryptBytes(data, sample->data_size());
  }

  traf()->sample_encryption.sample_encryption_entries.push_back(
      sample_encryption_entry);
  encryptor_->UpdateIv();
  return Status::OK;
}

bool EncryptingFragmenter::IsSubsampleEncryptionRequired() {
  return vpx_parser_ || nalu_length_size_ != 0;
}

}  // namespace mp4
}  // namespace media
}  // namespace shaka
