// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "packager/media/formats/mp4/track_run_iterator.h"

#include <algorithm>
#include <limits>

#include "packager/media/base/buffer_reader.h"
#include "packager/media/base/fourccs.h"
#include "packager/media/base/rcheck.h"
#include "packager/media/formats/mp4/chunk_info_iterator.h"
#include "packager/media/formats/mp4/composition_offset_iterator.h"
#include "packager/media/formats/mp4/decoding_time_iterator.h"
#include "packager/media/formats/mp4/sync_sample_iterator.h"

namespace {
const int64_t kInvalidOffset = std::numeric_limits<int64_t>::max();
}  // namespace

namespace shaka {
namespace media {
namespace mp4 {

struct SampleInfo {
  int64_t size;
  int64_t duration;
  int64_t cts_offset;
  bool is_keyframe;
};

struct TrackRunInfo {
  uint32_t track_id;
  std::vector<SampleInfo> samples;
  int64_t timescale;
  int64_t start_dts;
  int64_t sample_start_offset;

  TrackType track_type;
  const AudioSampleEntry* audio_description;
  const VideoSampleEntry* video_description;

  // Stores sample encryption entries, which is populated from 'senc' box if it
  // is available, otherwise will try to load from cenc auxiliary information.
  std::vector<SampleEncryptionEntry> sample_encryption_entries;

  // These variables are useful to load |sample_encryption_entries| from cenc
  // auxiliary information when 'senc' box is not available.
  int64_t aux_info_start_offset;  // Only valid if aux_info_total_size > 0.
  int aux_info_default_size;
  std::vector<uint8_t> aux_info_sizes;  // Populated if default_size == 0.
  int aux_info_total_size;

  TrackRunInfo();
  ~TrackRunInfo();
};

TrackRunInfo::TrackRunInfo()
    : track_id(0),
      timescale(-1),
      start_dts(-1),
      sample_start_offset(-1),
      track_type(kInvalid),
      audio_description(NULL),
      video_description(NULL),
      aux_info_start_offset(-1),
      aux_info_default_size(0),
      aux_info_total_size(0) {}
TrackRunInfo::~TrackRunInfo() {}

TrackRunIterator::TrackRunIterator(const Movie* moov)
    : moov_(moov), sample_dts_(0), sample_offset_(0) {
  CHECK(moov);
}

TrackRunIterator::~TrackRunIterator() {}

static void PopulateSampleInfo(const TrackExtends& trex,
                               const TrackFragmentHeader& tfhd,
                               const TrackFragmentRun& trun,
                               const uint32_t i,
                               SampleInfo* sample_info) {
  if (i < trun.sample_sizes.size()) {
    sample_info->size = trun.sample_sizes[i];
  } else if (tfhd.default_sample_size > 0) {
    sample_info->size = tfhd.default_sample_size;
  } else {
    sample_info->size = trex.default_sample_size;
  }

  if (i < trun.sample_durations.size()) {
    sample_info->duration = trun.sample_durations[i];
  } else if (tfhd.default_sample_duration > 0) {
    sample_info->duration = tfhd.default_sample_duration;
  } else {
    sample_info->duration = trex.default_sample_duration;
  }

  if (i < trun.sample_composition_time_offsets.size()) {
    sample_info->cts_offset = trun.sample_composition_time_offsets[i];
  } else {
    sample_info->cts_offset = 0;
  }

  uint32_t flags;
  if (i < trun.sample_flags.size()) {
    flags = trun.sample_flags[i];
  } else if (tfhd.flags & TrackFragmentHeader::kDefaultSampleFlagsPresentMask) {
    flags = tfhd.default_sample_flags;
  } else {
    flags = trex.default_sample_flags;
  }
  sample_info->is_keyframe = !(flags & TrackFragmentHeader::kNonKeySampleMask);
}

// In well-structured encrypted media, each track run will be immediately
// preceded by its auxiliary information; this is the only optimal storage
// pattern in terms of minimum number of bytes from a serial stream needed to
// begin playback. It also allows us to optimize caching on memory-constrained
// architectures, because we can cache the relatively small auxiliary
// information for an entire run and then discard data from the input stream,
// instead of retaining the entire 'mdat' box.
//
// We optimize for this situation (with no loss of generality) by sorting track
// runs during iteration in order of their first data offset (either sample data
// or auxiliary data).
class CompareMinTrackRunDataOffset {
 public:
  bool operator()(const TrackRunInfo& a, const TrackRunInfo& b) {
    int64_t a_aux = a.aux_info_total_size ? a.aux_info_start_offset : kInvalidOffset;
    int64_t b_aux = b.aux_info_total_size ? b.aux_info_start_offset : kInvalidOffset;

    int64_t a_lesser = std::min(a_aux, a.sample_start_offset);
    int64_t a_greater = std::max(a_aux, a.sample_start_offset);
    int64_t b_lesser = std::min(b_aux, b.sample_start_offset);
    int64_t b_greater = std::max(b_aux, b.sample_start_offset);

    if (a_lesser == b_lesser)
      return a_greater < b_greater;
    return a_lesser < b_lesser;
  }
};

bool TrackRunIterator::Init() {
  runs_.clear();

  for (std::vector<Track>::const_iterator trak = moov_->tracks.begin();
       trak != moov_->tracks.end(); ++trak) {
    const SampleDescription& stsd =
        trak->media.information.sample_table.description;
    if (stsd.type != kAudio && stsd.type != kVideo) {
      DVLOG(1) << "Skipping unhandled track type";
      continue;
    }

    // Edit list is ignored.
    // We may consider supporting the single edit with a nonnegative media time
    // if it is required. Just need to pass the media_time to Muxer and
    // generate the edit list.
    const std::vector<EditListEntry>& edits = trak->edit.list.edits;
    if (!edits.empty()) {
      if (edits.size() > 1)
        DVLOG(1) << "Multi-entry edit box detected.";

      DLOG(INFO) << "Edit list with media time " << edits[0].media_time
                 << " ignored.";
    }

    DecodingTimeIterator decoding_time(
        trak->media.information.sample_table.decoding_time_to_sample);
    CompositionOffsetIterator composition_offset(
        trak->media.information.sample_table.composition_time_to_sample);
    bool has_composition_offset = composition_offset.IsValid();
    ChunkInfoIterator chunk_info(
        trak->media.information.sample_table.sample_to_chunk);
    SyncSampleIterator sync_sample(
        trak->media.information.sample_table.sync_sample);
    // Skip processing saiz and saio boxes for non-fragmented mp4 as we
    // don't support encrypted non-fragmented mp4.

    const SampleSize& sample_size =
        trak->media.information.sample_table.sample_size;
    const std::vector<uint64_t>& chunk_offset_vector =
        trak->media.information.sample_table.chunk_large_offset.offsets;

    int64_t run_start_dts = 0;

    uint32_t num_samples = sample_size.sample_count;
    uint32_t num_chunks = chunk_offset_vector.size();

    // Check that total number of samples match.
    DCHECK_EQ(num_samples, decoding_time.NumSamples());
    if (has_composition_offset)
      DCHECK_EQ(num_samples, composition_offset.NumSamples());
    if (num_chunks > 0)
      DCHECK_EQ(num_samples, chunk_info.NumSamples(1, num_chunks));
    DCHECK_GE(num_chunks, chunk_info.LastFirstChunk());

    if (num_samples > 0) {
      // Verify relevant tables are not empty.
      RCHECK(decoding_time.IsValid());
      RCHECK(chunk_info.IsValid());
    }

    uint32_t sample_index = 0;
    for (uint32_t chunk_index = 0; chunk_index < num_chunks; ++chunk_index) {
      RCHECK(chunk_info.current_chunk() == chunk_index + 1);

      TrackRunInfo tri;
      tri.track_id = trak->header.track_id;
      tri.timescale = trak->media.header.timescale;
      tri.start_dts = run_start_dts;
      tri.sample_start_offset = chunk_offset_vector[chunk_index];

      uint32_t desc_idx = chunk_info.sample_description_index();
      RCHECK(desc_idx > 0);  // Descriptions are one-indexed in the file.
      desc_idx -= 1;

      tri.track_type = stsd.type;
      if (tri.track_type == kAudio) {
        RCHECK(!stsd.audio_entries.empty());
        if (desc_idx > stsd.audio_entries.size())
          desc_idx = 0;
        tri.audio_description = &stsd.audio_entries[desc_idx];
        // We don't support encrypted non-fragmented mp4 for now.
        RCHECK(tri.audio_description->sinf.info.track_encryption
                   .default_is_protected == 0);
      } else if (tri.track_type == kVideo) {
        RCHECK(!stsd.video_entries.empty());
        if (desc_idx > stsd.video_entries.size())
          desc_idx = 0;
        tri.video_description = &stsd.video_entries[desc_idx];
        // We don't support encrypted non-fragmented mp4 for now.
        RCHECK(tri.video_description->sinf.info.track_encryption
                   .default_is_protected == 0);
      }

      uint32_t samples_per_chunk = chunk_info.samples_per_chunk();
      tri.samples.resize(samples_per_chunk);
      for (uint32_t k = 0; k < samples_per_chunk; ++k) {
        SampleInfo& sample = tri.samples[k];
        sample.size = sample_size.sample_size != 0
                          ? sample_size.sample_size
                          : sample_size.sizes[sample_index];
        sample.duration = decoding_time.sample_delta();
        sample.cts_offset =
            has_composition_offset ? composition_offset.sample_offset() : 0;
        sample.is_keyframe = sync_sample.IsSyncSample();

        run_start_dts += sample.duration;

        // Advance to next sample. Should success except for last sample.
        ++sample_index;
        RCHECK(chunk_info.AdvanceSample() && sync_sample.AdvanceSample());
        if (sample_index == num_samples) {
          // We should hit end of tables for decoding time and composition
          // offset.
          RCHECK(!decoding_time.AdvanceSample());
          if (has_composition_offset)
            RCHECK(!composition_offset.AdvanceSample());
        } else {
          RCHECK(decoding_time.AdvanceSample());
          if (has_composition_offset)
            RCHECK(composition_offset.AdvanceSample());
        }
      }

      runs_.push_back(tri);
    }
  }

  std::sort(runs_.begin(), runs_.end(), CompareMinTrackRunDataOffset());
  run_itr_ = runs_.begin();
  ResetRun();
  return true;
}

bool TrackRunIterator::Init(const MovieFragment& moof) {
  runs_.clear();

  next_fragment_start_dts_.resize(moof.tracks.size(), 0);
  for (size_t i = 0; i < moof.tracks.size(); i++) {
    const TrackFragment& traf = moof.tracks[i];

    const Track* trak = NULL;
    for (size_t t = 0; t < moov_->tracks.size(); t++) {
      if (moov_->tracks[t].header.track_id == traf.header.track_id)
        trak = &moov_->tracks[t];
    }
    RCHECK(trak);

    const TrackExtends* trex = NULL;
    for (size_t t = 0; t < moov_->extends.tracks.size(); t++) {
      if (moov_->extends.tracks[t].track_id == traf.header.track_id)
        trex = &moov_->extends.tracks[t];
    }
    RCHECK(trex);

    const SampleDescription& stsd =
        trak->media.information.sample_table.description;
    if (stsd.type != kAudio && stsd.type != kVideo) {
      DVLOG(1) << "Skipping unhandled track type";
      continue;
    }
    size_t desc_idx = traf.header.sample_description_index;
    if (!desc_idx)
      desc_idx = trex->default_sample_description_index;
    RCHECK(desc_idx > 0);  // Descriptions are one-indexed in the file
    desc_idx -= 1;

    const AudioSampleEntry* audio_sample_entry = NULL;
    const VideoSampleEntry* video_sample_entry = NULL;
    switch (stsd.type) {
      case kAudio:
        RCHECK(!stsd.audio_entries.empty());
        if (desc_idx > stsd.audio_entries.size())
          desc_idx = 0;
        audio_sample_entry = &stsd.audio_entries[desc_idx];
        break;
      case kVideo:
        RCHECK(!stsd.video_entries.empty());
        if (desc_idx > stsd.video_entries.size())
          desc_idx = 0;
        video_sample_entry = &stsd.video_entries[desc_idx];
        break;
      default:
        NOTREACHED();
        break;
    }

    // SampleEncryptionEntries should not have been parsed, without having
    // iv_size. Parse the box now.
    DCHECK(traf.sample_encryption.sample_encryption_entries.empty());
    std::vector<SampleEncryptionEntry> sample_encryption_entries;
    if (!traf.sample_encryption.sample_encryption_data.empty()) {
      RCHECK(audio_sample_entry || video_sample_entry);
      const uint8_t default_per_sample_iv_size =
          audio_sample_entry
              ? audio_sample_entry->sinf.info.track_encryption
                    .default_per_sample_iv_size
              : video_sample_entry->sinf.info.track_encryption
                    .default_per_sample_iv_size;
      RCHECK(traf.sample_encryption.ParseFromSampleEncryptionData(
          default_per_sample_iv_size, &sample_encryption_entries));
    }

    int64_t run_start_dts = traf.decode_time_absent
                                ? next_fragment_start_dts_[i]
                                : traf.decode_time.decode_time;
    int sample_count_sum = 0;

    for (size_t j = 0; j < traf.runs.size(); j++) {
      const TrackFragmentRun& trun = traf.runs[j];
      TrackRunInfo tri;
      tri.track_id = traf.header.track_id;
      tri.timescale = trak->media.header.timescale;
      tri.start_dts = run_start_dts;
      tri.sample_start_offset = trun.data_offset;

      tri.track_type = stsd.type;
      tri.audio_description = audio_sample_entry;
      tri.video_description = video_sample_entry;

      tri.aux_info_start_offset = -1;
      tri.aux_info_total_size = 0;
      // Populate sample encryption entries from SampleEncryption 'senc' box if
      // it is available; otherwise initialize aux_info variables, which will
      // be used to populate sample encryption entries later in CacheAuxInfo.
      if (!sample_encryption_entries.empty()) {
        RCHECK(sample_encryption_entries.size() >=
               sample_count_sum + trun.sample_count);
        for (size_t k = 0; k < trun.sample_count; ++k) {
          tri.sample_encryption_entries.push_back(
              sample_encryption_entries[sample_count_sum + k]);
        }
      } else if (traf.auxiliary_offset.offsets.size() > j) {
        // Collect information from the auxiliary_offset entry with the same
        // index in the 'saiz' container as the current run's index in the
        // 'trun' container, if it is present.
        tri.aux_info_start_offset = traf.auxiliary_offset.offsets[j];
        // There should be an auxiliary info entry corresponding to each sample
        // in the auxiliary offset entry's corresponding track run.
        RCHECK(traf.auxiliary_size.sample_count >=
               sample_count_sum + trun.sample_count);
        tri.aux_info_default_size =
            traf.auxiliary_size.default_sample_info_size;
        if (tri.aux_info_default_size == 0) {
          const std::vector<uint8_t>& sizes =
              traf.auxiliary_size.sample_info_sizes;
          tri.aux_info_sizes.insert(
              tri.aux_info_sizes.begin(),
              sizes.begin() + sample_count_sum,
              sizes.begin() + sample_count_sum + trun.sample_count);
        }

        // If the default info size is positive, find the total size of the aux
        // info block from it, otherwise sum over the individual sizes of each
        // aux info entry in the aux_offset entry.
        if (tri.aux_info_default_size) {
          tri.aux_info_total_size =
              tri.aux_info_default_size * trun.sample_count;
        } else {
          tri.aux_info_total_size = 0;
          for (size_t k = 0; k < trun.sample_count; k++) {
            tri.aux_info_total_size += tri.aux_info_sizes[k];
          }
        }
      }

      tri.samples.resize(trun.sample_count);
      for (size_t k = 0; k < trun.sample_count; k++) {
        PopulateSampleInfo(*trex, traf.header, trun, k, &tri.samples[k]);
        run_start_dts += tri.samples[k].duration;
      }
      runs_.push_back(tri);
      sample_count_sum += trun.sample_count;
    }
    next_fragment_start_dts_[i] = run_start_dts;
  }

  std::sort(runs_.begin(), runs_.end(), CompareMinTrackRunDataOffset());
  run_itr_ = runs_.begin();
  ResetRun();
  return true;
}

void TrackRunIterator::AdvanceRun() {
  ++run_itr_;
  ResetRun();
}

void TrackRunIterator::ResetRun() {
  if (!IsRunValid())
    return;
  sample_dts_ = run_itr_->start_dts;
  sample_offset_ = run_itr_->sample_start_offset;
  sample_itr_ = run_itr_->samples.begin();
}

void TrackRunIterator::AdvanceSample() {
  DCHECK(IsSampleValid());
  sample_dts_ += sample_itr_->duration;
  sample_offset_ += sample_itr_->size;
  ++sample_itr_;
}

// This implementation only indicates a need for caching if CENC auxiliary
// info is available in the stream.
bool TrackRunIterator::AuxInfoNeedsToBeCached() {
  DCHECK(IsRunValid());
  return is_encrypted() && aux_info_size() > 0 &&
         run_itr_->sample_encryption_entries.size() == 0;
}

// This implementation currently only caches CENC auxiliary info.
bool TrackRunIterator::CacheAuxInfo(const uint8_t* buf, int buf_size) {
  RCHECK(AuxInfoNeedsToBeCached() && buf_size >= aux_info_size());

  std::vector<SampleEncryptionEntry>& sample_encryption_entries =
      runs_[run_itr_ - runs_.begin()].sample_encryption_entries;
  sample_encryption_entries.resize(run_itr_->samples.size());
  int64_t pos = 0;
  for (size_t i = 0; i < run_itr_->samples.size(); i++) {
    int info_size = run_itr_->aux_info_default_size;
    if (!info_size)
      info_size = run_itr_->aux_info_sizes[i];

    BufferReader reader(buf + pos, info_size);
    const bool has_subsamples =
        info_size > track_encryption().default_per_sample_iv_size;
    RCHECK(sample_encryption_entries[i].ParseFromBuffer(
        track_encryption().default_per_sample_iv_size, has_subsamples,
        &reader));
    pos += info_size;
  }

  return true;
}

bool TrackRunIterator::IsRunValid() const { return run_itr_ != runs_.end(); }

bool TrackRunIterator::IsSampleValid() const {
  return IsRunValid() && (sample_itr_ != run_itr_->samples.end());
}

// Because tracks are in sorted order and auxiliary information is cached when
// returning samples, it is guaranteed that no data will be required before the
// lesser of the minimum data offset of this track and the next in sequence.
// (The stronger condition - that no data is required before the minimum data
// offset of this track alone - is not guaranteed, because the BMFF spec does
// not have any inter-run ordering restrictions.)
int64_t TrackRunIterator::GetMaxClearOffset() {
  int64_t offset = kInvalidOffset;

  if (IsSampleValid()) {
    offset = std::min(offset, sample_offset_);
    if (AuxInfoNeedsToBeCached())
      offset = std::min(offset, aux_info_offset());
  }
  if (run_itr_ != runs_.end()) {
    std::vector<TrackRunInfo>::const_iterator next_run = run_itr_ + 1;
    if (next_run != runs_.end()) {
      offset = std::min(offset, next_run->sample_start_offset);
      if (next_run->aux_info_total_size)
        offset = std::min(offset, next_run->aux_info_start_offset);
    }
  }
  if (offset == kInvalidOffset)
    return runs_.empty() ? 0 : runs_[0].sample_start_offset;
  return offset;
}

uint32_t TrackRunIterator::track_id() const {
  DCHECK(IsRunValid());
  return run_itr_->track_id;
}

bool TrackRunIterator::is_encrypted() const {
  DCHECK(IsRunValid());
  return track_encryption().default_is_protected == 1;
}

int64_t TrackRunIterator::aux_info_offset() const {
  return run_itr_->aux_info_start_offset;
}

int TrackRunIterator::aux_info_size() const {
  return run_itr_->aux_info_total_size;
}

bool TrackRunIterator::is_audio() const {
  DCHECK(IsRunValid());
  return run_itr_->track_type == kAudio;
}

bool TrackRunIterator::is_video() const {
  DCHECK(IsRunValid());
  return run_itr_->track_type == kVideo;
}

const AudioSampleEntry& TrackRunIterator::audio_description() const {
  DCHECK(is_audio());
  DCHECK(run_itr_->audio_description);
  return *run_itr_->audio_description;
}

const VideoSampleEntry& TrackRunIterator::video_description() const {
  DCHECK(is_video());
  DCHECK(run_itr_->video_description);
  return *run_itr_->video_description;
}

int64_t TrackRunIterator::sample_offset() const {
  DCHECK(IsSampleValid());
  return sample_offset_;
}

int TrackRunIterator::sample_size() const {
  DCHECK(IsSampleValid());
  return sample_itr_->size;
}

int64_t TrackRunIterator::dts() const {
  DCHECK(IsSampleValid());
  return sample_dts_;
}

int64_t TrackRunIterator::cts() const {
  DCHECK(IsSampleValid());
  return sample_dts_ + sample_itr_->cts_offset;
}

int64_t TrackRunIterator::duration() const {
  DCHECK(IsSampleValid());
  return sample_itr_->duration;
}

bool TrackRunIterator::is_keyframe() const {
  DCHECK(IsSampleValid());
  return sample_itr_->is_keyframe;
}

const TrackEncryption& TrackRunIterator::track_encryption() const {
  if (is_audio())
    return audio_description().sinf.info.track_encryption;
  DCHECK(is_video());
  return video_description().sinf.info.track_encryption;
}

std::unique_ptr<DecryptConfig> TrackRunIterator::GetDecryptConfig() {
  std::vector<uint8_t> iv;
  std::vector<SubsampleEntry> subsamples;

  size_t sample_idx = sample_itr_ - run_itr_->samples.begin();
  if (sample_idx < run_itr_->sample_encryption_entries.size()) {
    const SampleEncryptionEntry& sample_encryption_entry =
        run_itr_->sample_encryption_entries[sample_idx];
    DCHECK(is_encrypted());
    DCHECK(!AuxInfoNeedsToBeCached());

    const size_t total_size_of_subsamples =
        sample_encryption_entry.GetTotalSizeOfSubsamples();
    if (total_size_of_subsamples != 0 &&
        total_size_of_subsamples != static_cast<size_t>(sample_size())) {
      LOG(ERROR) << "Incorrect CENC subsample size.";
      return std::unique_ptr<DecryptConfig>();
    }

    iv = sample_encryption_entry.initialization_vector;
    subsamples = sample_encryption_entry.subsamples;
  }

  FourCC protection_scheme = is_audio() ? audio_description().sinf.type.type
                                        : video_description().sinf.type.type;
  if (iv.empty()) {
    if (protection_scheme != FOURCC_cbcs) {
      LOG(WARNING)
          << "Constant IV should only be used with 'cbcs' protection scheme.";
    }
    iv = track_encryption().default_constant_iv;
    if (iv.empty()) {
      LOG(ERROR) << "IV cannot be empty.";
      return std::unique_ptr<DecryptConfig>();
    }
  }
  return std::unique_ptr<DecryptConfig>(new DecryptConfig(
      track_encryption().default_kid, iv, subsamples, protection_scheme,
      track_encryption().default_crypt_byte_block,
      track_encryption().default_skip_byte_block));
}

}  // namespace mp4
}  // namespace media
}  // namespace shaka
