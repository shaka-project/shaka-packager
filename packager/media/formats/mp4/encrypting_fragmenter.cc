// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/formats/mp4/encrypting_fragmenter.h"

#include <limits>

#include "packager/media/base/aes_encryptor.h"
#include "packager/media/base/buffer_reader.h"
#include "packager/media/base/key_source.h"
#include "packager/media/base/media_sample.h"
#include "packager/media/filters/nalu_reader.h"
#include "packager/media/filters/vp8_parser.h"
#include "packager/media/filters/vp9_parser.h"
#include "packager/media/formats/mp4/box_definitions.h"

namespace edash_packager {
namespace media {
namespace mp4 {

namespace {
// Generate 64bit IV by default.
const size_t kDefaultIvSize = 8u;
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

VideoCodec GetVideoCodec(const StreamInfo& stream_info) {
  if (stream_info.stream_type() != kStreamVideo)
    return kUnknownVideoCodec;
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
    scoped_refptr<StreamInfo> info,
    TrackFragment* traf,
    scoped_ptr<EncryptionKey> encryption_key,
    int64_t clear_time)
    : Fragmenter(traf),
      info_(info),
      encryption_key_(encryption_key.Pass()),
      nalu_length_size_(GetNaluLengthSize(*info)),
      clear_time_(clear_time) {
  DCHECK(encryption_key_);
  VideoCodec video_codec = GetVideoCodec(*info);
  if (video_codec == kCodecVP8) {
    vpx_parser_.reset(new VP8Parser);
  } else if (video_codec == kCodecVP9) {
    vpx_parser_.reset(new VP9Parser);
  } else if (video_codec == kCodecH264) {
    header_parser_.reset(new H264VideoSliceHeaderParser);
  }
  // TODO(modmaker): Support H.265.
}

EncryptingFragmenter::~EncryptingFragmenter() {}

Status EncryptingFragmenter::AddSample(scoped_refptr<MediaSample> sample) {
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

  if (header_parser_ && !header_parser_->Initialize(info_->extra_data()))
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

  // Optimize saiz box.
  SampleAuxiliaryInformationSize& saiz = traf()->auxiliary_size;
  saiz.sample_count = traf()->runs[0].sample_sizes.size();
  if (!saiz.sample_info_sizes.empty()) {
    if (!OptimizeSampleEntries(&saiz.sample_info_sizes,
                               &saiz.default_sample_info_size)) {
      saiz.default_sample_info_size = 0;
    }
  } else {
    // |sample_info_sizes| table is filled in only for subsample encryption,
    // otherwise |sample_info_size| is just the IV size.
    DCHECK(!IsSubsampleEncryptionRequired());
    saiz.default_sample_info_size = encryptor_->iv().size();
  }
  traf()->sample_encryption.iv_size = encryptor_->iv().size();
}

Status EncryptingFragmenter::CreateEncryptor() {
  DCHECK(encryption_key_);

  scoped_ptr<AesCtrEncryptor> encryptor(new AesCtrEncryptor());
  const bool initialized = encryption_key_->iv.empty()
                               ? encryptor->InitializeWithRandomIv(
                                     encryption_key_->key, kDefaultIvSize)
                               : encryptor->InitializeWithIv(
                                     encryption_key_->key, encryption_key_->iv);
  if (!initialized)
    return Status(error::MUXER_FAILURE, "Failed to create the encryptor.");
  encryptor_ = encryptor.Pass();
  return Status::OK;
}

void EncryptingFragmenter::EncryptBytes(uint8_t* data, uint32_t size) {
  DCHECK(encryptor_);
  CHECK(encryptor_->Encrypt(data, size, data));
}

Status EncryptingFragmenter::EncryptSample(scoped_refptr<MediaSample> sample) {
  DCHECK(encryptor_);

  SampleEncryptionEntry sample_encryption_entry;
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
        subsample.clear_bytes = frame.uncompressed_header_size;
        subsample.cipher_bytes =
            frame.frame_size - frame.uncompressed_header_size;

        // "VP Codec ISO Media File Format Binding" document requires that the
        // encrypted bytes of each frame within the superframe must be block
        // aligned so that the counter state can be computed for each frame
        // within the superframe.
        if (is_superframe) {
          uint16_t misalign_bytes = subsample.cipher_bytes % kCencBlockSize;
          subsample.clear_bytes += misalign_bytes;
          subsample.cipher_bytes -= misalign_bytes;
        }

        sample_encryption_entry.subsamples.push_back(subsample);
        if (subsample.cipher_bytes > 0)
          EncryptBytes(data + subsample.clear_bytes, subsample.cipher_bytes);
        data += frame.frame_size;
      }
    } else {
      NaluReader reader(nalu_length_size_, data, sample->data_size());

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

          const uint64_t current_clear_bytes = nalu.header_size() +
                                               video_slice_header_size;
          const uint64_t cipher_bytes =
              nalu.payload_size() - video_slice_header_size;
          const uint8_t* nalu_data = nalu.data() + current_clear_bytes;
          EncryptBytes(const_cast<uint8_t*>(nalu_data), cipher_bytes);

          AddSubsamples(
              accumulated_clear_bytes + nalu_length_size_ + current_clear_bytes,
              cipher_bytes, &sample_encryption_entry.subsamples);
          accumulated_clear_bytes = 0;
        } else {
          // For non-video-slice NAL units, don't encrypt.
          accumulated_clear_bytes += nalu.header_size() + nalu.payload_size();
        }
      }
      if (result != NaluReader::kEOStream)
        return Status(error::MUXER_FAILURE, "Failed to parse NAL units.");
      AddSubsamples(accumulated_clear_bytes, 0,
                    &sample_encryption_entry.subsamples);
    }

    // The length of per-sample auxiliary datum, defined in CENC ch. 7.
    traf()->auxiliary_size.sample_info_sizes.push_back(
        sample_encryption_entry.ComputeSize());
  } else {
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
}  // namespace edash_packager
