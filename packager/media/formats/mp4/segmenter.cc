// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/formats/mp4/segmenter.h"

#include <algorithm>

#include "packager/base/logging.h"
#include "packager/media/base/aes_cryptor.h"
#include "packager/media/base/buffer_writer.h"
#include "packager/media/base/key_source.h"
#include "packager/media/base/media_sample.h"
#include "packager/media/base/media_stream.h"
#include "packager/media/base/muxer_options.h"
#include "packager/media/base/muxer_util.h"
#include "packager/media/base/video_stream_info.h"
#include "packager/media/event/muxer_listener.h"
#include "packager/media/event/progress_listener.h"
#include "packager/media/formats/mp4/box_definitions.h"
#include "packager/media/formats/mp4/key_rotation_fragmenter.h"
#include "packager/version/version.h"

namespace shaka {
namespace media {
namespace mp4 {

namespace {
const size_t kCencKeyIdSize = 16u;

// The version of cenc implemented here. CENC 4.
const int kCencSchemeVersion = 0x00010000;

// The default KID for key rotation is all 0s.
const uint8_t kKeyRotationDefaultKeyId[] = {
  0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0
};

// Defines protection pattern for pattern-based encryption.
struct ProtectionPattern {
  uint8_t crypt_byte_block;
  uint8_t skip_byte_block;
};

static_assert(arraysize(kKeyRotationDefaultKeyId) == kCencKeyIdSize,
              "cenc_key_id_must_be_size_16");

uint64_t Rescale(uint64_t time_in_old_scale,
                 uint32_t old_scale,
                 uint32_t new_scale) {
  return static_cast<double>(time_in_old_scale) / old_scale * new_scale;
}

ProtectionPattern GetProtectionPattern(FourCC protection_scheme,
                                       TrackType track_type) {
  ProtectionPattern pattern;
  if (protection_scheme != FOURCC_cbcs && protection_scheme != FOURCC_cens) {
    // Not using pattern encryption.
    pattern.crypt_byte_block = 0u;
    pattern.skip_byte_block = 0u;
  } else if (track_type != kVideo) {
    // Tracks other than video are protected using whole-block full-sample
    // encryption, which is essentially a pattern of 1:0. Note that this may not
    // be the same as the non-pattern based encryption counterparts, e.g. in
    // 'cens' for full sample encryption, the whole sample is encrypted up to
    // the last 16-byte boundary, see 23001-7:2016(E) 9.7; while in 'cenc' for
    // full sample encryption, the last partial 16-byte block is also encrypted,
    // see 23001-7:2016(E) 9.4.2. Another difference is the use of constant iv.
    pattern.crypt_byte_block = 1u;
    pattern.skip_byte_block = 0u;
  } else {
    // Use 1:9 pattern for video.
    const uint8_t kCryptByteBlock = 1u;
    const uint8_t kSkipByteBlock = 9u;
    pattern.crypt_byte_block = kCryptByteBlock;
    pattern.skip_byte_block = kSkipByteBlock;
  }
  return pattern;
}

void GenerateSinf(const EncryptionKey& encryption_key,
                  FourCC old_type,
                  FourCC protection_scheme,
                  ProtectionPattern pattern,
                  ProtectionSchemeInfo* sinf) {
  sinf->format.format = old_type;

  DCHECK_NE(protection_scheme, FOURCC_NULL);
  sinf->type.type = protection_scheme;
  sinf->type.version = kCencSchemeVersion;

  auto& track_encryption = sinf->info.track_encryption;
  track_encryption.default_is_protected = 1;
  DCHECK(!encryption_key.iv.empty());
  if (protection_scheme == FOURCC_cbcs) {
    // ISO/IEC 23001-7:2016 10.4.1
    // For 'cbcs' scheme, Constant IVs SHALL be used.
    track_encryption.default_per_sample_iv_size = 0;
    track_encryption.default_constant_iv = encryption_key.iv;
  } else {
    track_encryption.default_per_sample_iv_size =
      static_cast<uint8_t>(encryption_key.iv.size());
  }
  track_encryption.default_crypt_byte_block = pattern.crypt_byte_block;
  track_encryption.default_skip_byte_block = pattern.skip_byte_block;
  track_encryption.default_kid = encryption_key.key_id;
}

void GenerateEncryptedSampleEntry(const EncryptionKey& encryption_key,
                                  double clear_lead_in_seconds,
                                  FourCC protection_scheme,
                                  ProtectionPattern pattern,
                                  SampleDescription* description) {
  DCHECK(description);
  if (description->type == kVideo) {
    DCHECK_EQ(1u, description->video_entries.size());

    // Add a second entry for clear content if needed.
    if (clear_lead_in_seconds > 0)
      description->video_entries.push_back(description->video_entries[0]);

    // Convert the first entry to an encrypted entry.
    VideoSampleEntry& entry = description->video_entries[0];
    GenerateSinf(encryption_key, entry.format, protection_scheme, pattern,
                 &entry.sinf);
    entry.format = FOURCC_encv;
  } else {
    DCHECK_EQ(kAudio, description->type);
    DCHECK_EQ(1u, description->audio_entries.size());

    // Add a second entry for clear content if needed.
    if (clear_lead_in_seconds > 0)
      description->audio_entries.push_back(description->audio_entries[0]);

    // Convert the first entry to an encrypted entry.
    AudioSampleEntry& entry = description->audio_entries[0];
    GenerateSinf(encryption_key, entry.format, protection_scheme, pattern,
                 &entry.sinf);
    entry.format = FOURCC_enca;
  }
}

}  // namespace

Segmenter::Segmenter(const MuxerOptions& options,
                     std::unique_ptr<FileType> ftyp,
                     std::unique_ptr<Movie> moov)
    : options_(options),
      ftyp_(std::move(ftyp)),
      moov_(std::move(moov)),
      moof_(new MovieFragment()),
      fragment_buffer_(new BufferWriter()),
      sidx_(new SegmentIndex()),
      muxer_listener_(NULL),
      progress_listener_(NULL),
      progress_target_(0),
      accumulated_progress_(0),
      sample_duration_(0u) {}

Segmenter::~Segmenter() {}

Status Segmenter::Initialize(const std::vector<MediaStream*>& streams,
                             MuxerListener* muxer_listener,
                             ProgressListener* progress_listener,
                             KeySource* encryption_key_source,
                             uint32_t max_sd_pixels,
                             uint32_t max_hd_pixels,
                             uint32_t max_uhd1_pixels,
                             double clear_lead_in_seconds,
                             double crypto_period_duration_in_seconds,
                             FourCC protection_scheme) {
  DCHECK_LT(0u, streams.size());
  muxer_listener_ = muxer_listener;
  progress_listener_ = progress_listener;
  moof_->header.sequence_number = 0;

  moof_->tracks.resize(streams.size());
  segment_durations_.resize(streams.size());
  fragmenters_.resize(streams.size());
  const bool key_rotation_enabled = crypto_period_duration_in_seconds != 0;
  const bool kInitialEncryptionInfo = true;

  for (uint32_t i = 0; i < streams.size(); ++i) {
    stream_map_[streams[i]] = i;
    moof_->tracks[i].header.track_id = i + 1;
    if (streams[i]->info()->stream_type() == kStreamVideo) {
      // Use the first video stream as the reference stream (which is 1-based).
      if (sidx_->reference_id == 0)
        sidx_->reference_id = i + 1;
    }
    if (!encryption_key_source) {
      fragmenters_[i].reset(
          new Fragmenter(streams[i]->info(), &moof_->tracks[i]));
      continue;
    }

    KeySource::TrackType track_type =
        GetTrackTypeForEncryption(*streams[i]->info(), max_sd_pixels,
                                  max_hd_pixels, max_uhd1_pixels);
    SampleDescription& description =
        moov_->tracks[i].media.information.sample_table.description;
    ProtectionPattern pattern =
        GetProtectionPattern(protection_scheme, description.type);

    if (key_rotation_enabled) {
      // Fill encrypted sample entry with default key.
      EncryptionKey encryption_key;
      encryption_key.key_id.assign(
          kKeyRotationDefaultKeyId,
          kKeyRotationDefaultKeyId + arraysize(kKeyRotationDefaultKeyId));
      if (!AesCryptor::GenerateRandomIv(protection_scheme,
                                        &encryption_key.iv)) {
        return Status(error::INTERNAL_ERROR, "Failed to generate random iv.");
      }
      GenerateEncryptedSampleEntry(encryption_key, clear_lead_in_seconds,
                                   protection_scheme, pattern, &description);
      if (muxer_listener_) {
        muxer_listener_->OnEncryptionInfoReady(
            kInitialEncryptionInfo, protection_scheme, encryption_key.key_id,
            encryption_key.iv, encryption_key.key_system_info);
      }

      fragmenters_[i].reset(new KeyRotationFragmenter(
          moof_.get(), streams[i]->info(), &moof_->tracks[i],
          encryption_key_source, track_type,
          crypto_period_duration_in_seconds * streams[i]->info()->time_scale(),
          clear_lead_in_seconds * streams[i]->info()->time_scale(),
          protection_scheme, pattern.crypt_byte_block, pattern.skip_byte_block,
          muxer_listener_));
      continue;
    }

    std::unique_ptr<EncryptionKey> encryption_key(new EncryptionKey());
    Status status =
        encryption_key_source->GetKey(track_type, encryption_key.get());
    if (!status.ok())
      return status;
    if (encryption_key->iv.empty()) {
      if (!AesCryptor::GenerateRandomIv(protection_scheme,
                                        &encryption_key->iv)) {
        return Status(error::INTERNAL_ERROR, "Failed to generate random iv.");
      }
    }

    GenerateEncryptedSampleEntry(*encryption_key, clear_lead_in_seconds,
                                 protection_scheme, pattern, &description);

    if (moov_->pssh.empty()) {
      moov_->pssh.resize(encryption_key->key_system_info.size());
      for (size_t i = 0; i < encryption_key->key_system_info.size(); i++) {
        moov_->pssh[i].raw_box = encryption_key->key_system_info[i].CreateBox();
      }

      if (muxer_listener_) {
        muxer_listener_->OnEncryptionInfoReady(
            kInitialEncryptionInfo, protection_scheme, encryption_key->key_id,
            encryption_key->iv, encryption_key->key_system_info);
      }
    }

    fragmenters_[i].reset(new EncryptingFragmenter(
        streams[i]->info(), &moof_->tracks[i], std::move(encryption_key),
        clear_lead_in_seconds * streams[i]->info()->time_scale(),
        protection_scheme, pattern.crypt_byte_block, pattern.skip_byte_block,
        muxer_listener_));
  }

  if (options_.mp4_use_decoding_timestamp_in_timeline) {
    for (uint32_t i = 0; i < streams.size(); ++i)
      fragmenters_[i]->set_use_decoding_timestamp_in_timeline(true);
  }

  // Choose the first stream if there is no VIDEO.
  if (sidx_->reference_id == 0)
    sidx_->reference_id = 1;
  sidx_->timescale = streams[GetReferenceStreamId()]->info()->time_scale();

  // Use media duration as progress target.
  progress_target_ = streams[GetReferenceStreamId()]->info()->duration();

  // Use the reference stream's time scale as movie time scale.
  moov_->header.timescale = sidx_->timescale;
  moof_->header.sequence_number = 1;

  // Fill in version information.
  const std::string version = GetPackagerVersion();
  if (!version.empty()) {
    moov_->metadata.handler.handler_type = FOURCC_ID32;
    moov_->metadata.id3v2.language.code = "eng";
    moov_->metadata.id3v2.private_frame.owner = GetPackagerProjectUrl();
    moov_->metadata.id3v2.private_frame.value = version;
  }
  return DoInitialize();
}

Status Segmenter::Finalize() {
  for (const std::unique_ptr<Fragmenter>& fragmenter : fragmenters_) {
    Status status = FinalizeFragment(true, fragmenter.get());
    if (!status.ok())
      return status;
  }

  // Set tracks and moov durations.
  // Note that the updated moov box will be written to output file for VOD case
  // only.
  for (std::vector<Track>::iterator track = moov_->tracks.begin();
       track != moov_->tracks.end();
       ++track) {
    track->header.duration = Rescale(track->media.header.duration,
                                     track->media.header.timescale,
                                     moov_->header.timescale);
    if (track->header.duration > moov_->header.duration)
      moov_->header.duration = track->header.duration;
  }
  moov_->extends.header.fragment_duration = moov_->header.duration;

  return DoFinalize();
}

Status Segmenter::AddSample(const MediaStream* stream,
                            scoped_refptr<MediaSample> sample) {
  // Find the fragmenter for this stream.
  DCHECK(stream);
  DCHECK(stream_map_.find(stream) != stream_map_.end());
  uint32_t stream_id = stream_map_[stream];
  Fragmenter* fragmenter = fragmenters_[stream_id].get();

  // Set default sample duration if it has not been set yet.
  if (moov_->extends.tracks[stream_id].default_sample_duration == 0) {
    moov_->extends.tracks[stream_id].default_sample_duration =
        sample->duration();
  }

  if (fragmenter->fragment_finalized()) {
    return Status(error::FRAGMENT_FINALIZED,
                  "Current fragment is finalized already.");
  }

  bool finalize_fragment = false;
  if (fragmenter->fragment_duration() >=
      options_.fragment_duration * stream->info()->time_scale()) {
    if (sample->is_key_frame() || !options_.fragment_sap_aligned) {
      finalize_fragment = true;
    }
  }
  bool finalize_segment = false;
  if (segment_durations_[stream_id] >=
      options_.segment_duration * stream->info()->time_scale()) {
    if (sample->is_key_frame() || !options_.segment_sap_aligned) {
      finalize_segment = true;
      finalize_fragment = true;
    }
  }

  Status status;
  if (finalize_fragment) {
    status = FinalizeFragment(finalize_segment, fragmenter);
    if (!status.ok())
      return status;
  }

  status = fragmenter->AddSample(sample);
  if (!status.ok())
    return status;

  if (sample_duration_ == 0)
    sample_duration_ = sample->duration();
  moov_->tracks[stream_id].media.header.duration += sample->duration();
  segment_durations_[stream_id] += sample->duration();
  DCHECK_GE(segment_durations_[stream_id], fragmenter->fragment_duration());
  return Status::OK;
}

uint32_t Segmenter::GetReferenceTimeScale() const {
  return moov_->header.timescale;
}

double Segmenter::GetDuration() const {
  if (moov_->header.timescale == 0) {
    // Handling the case where this is not properly initialized.
    return 0.0;
  }

  return static_cast<double>(moov_->header.duration) / moov_->header.timescale;
}

void Segmenter::UpdateProgress(uint64_t progress) {
  accumulated_progress_ += progress;

  if (!progress_listener_) return;
  if (progress_target_ == 0) return;
  // It might happen that accumulated progress exceeds progress_target due to
  // computation errors, e.g. rounding error. Cap it so it never reports > 100%
  // progress.
  if (accumulated_progress_ >= progress_target_) {
    progress_listener_->OnProgress(1.0);
  } else {
    progress_listener_->OnProgress(static_cast<double>(accumulated_progress_) /
                                   progress_target_);
  }
}

void Segmenter::SetComplete() {
  if (!progress_listener_) return;
  progress_listener_->OnProgress(1.0);
}

Status Segmenter::FinalizeSegment() {
  Status status = DoFinalizeSegment();

  // Reset segment information to initial state.
  sidx_->references.clear();
  std::vector<uint64_t>::iterator it = segment_durations_.begin();
  for (; it != segment_durations_.end(); ++it)
    *it = 0;

  return status;
}

uint32_t Segmenter::GetReferenceStreamId() {
  DCHECK(sidx_);
  return sidx_->reference_id - 1;
}

Status Segmenter::FinalizeFragment(bool finalize_segment,
                                   Fragmenter* fragmenter) {
  fragmenter->FinalizeFragment();

  // Check if all tracks are ready for fragmentation.
  for (const std::unique_ptr<Fragmenter>& fragmenter : fragmenters_) {
    if (!fragmenter->fragment_finalized())
      return Status::OK;
  }

  MediaData mdat;
  // Data offset relative to 'moof': moof size + mdat header size.
  // The code will also update box sizes for moof_ and its child boxes.
  uint64_t data_offset = moof_->ComputeSize() + mdat.HeaderSize();
  // 'traf' should follow 'mfhd' moof header box.
  uint64_t next_traf_position = moof_->HeaderSize() + moof_->header.box_size();
  for (size_t i = 0; i < moof_->tracks.size(); ++i) {
    TrackFragment& traf = moof_->tracks[i];
    if (traf.auxiliary_offset.offsets.size() > 0) {
      DCHECK_EQ(traf.auxiliary_offset.offsets.size(), 1u);
      DCHECK(!traf.sample_encryption.sample_encryption_entries.empty());

      next_traf_position += traf.box_size();
      // SampleEncryption 'senc' box should be the last box in 'traf'.
      // |auxiliary_offset| should point to the data of SampleEncryption.
      traf.auxiliary_offset.offsets[0] =
          next_traf_position - traf.sample_encryption.box_size() +
          traf.sample_encryption.HeaderSize() +
          sizeof(uint32_t);  // for sample count field in 'senc'
    }
    traf.runs[0].data_offset = data_offset + mdat.data_size;
    mdat.data_size += fragmenters_[i]->data()->Size();
  }

  // Generate segment reference.
  sidx_->references.resize(sidx_->references.size() + 1);
  fragmenters_[GetReferenceStreamId()]->GenerateSegmentReference(
      &sidx_->references[sidx_->references.size() - 1]);
  sidx_->references[sidx_->references.size() - 1].referenced_size =
      data_offset + mdat.data_size;

  // Write the fragment to buffer.
  moof_->Write(fragment_buffer_.get());
  mdat.WriteHeader(fragment_buffer_.get());
  for (const std::unique_ptr<Fragmenter>& fragmenter : fragmenters_)
    fragment_buffer_->AppendBuffer(*fragmenter->data());

  // Increase sequence_number for next fragment.
  ++moof_->header.sequence_number;

  if (finalize_segment)
    return FinalizeSegment();

  return Status::OK;
}

}  // namespace mp4
}  // namespace media
}  // namespace shaka
