// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "packager/media/formats/mp4/mp4_media_parser.h"

#include <limits>

#include "packager/base/callback.h"
#include "packager/base/callback_helpers.h"
#include "packager/base/logging.h"
#include "packager/base/memory/ref_counted.h"
#include "packager/base/strings/string_number_conversions.h"
#include "packager/media/base/aes_encryptor.h"
#include "packager/media/base/audio_stream_info.h"
#include "packager/media/base/buffer_reader.h"
#include "packager/media/base/decrypt_config.h"
#include "packager/media/base/key_source.h"
#include "packager/media/base/media_sample.h"
#include "packager/media/base/video_stream_info.h"
#include "packager/media/file/file.h"
#include "packager/media/file/file_closer.h"
#include "packager/media/formats/mp4/box_definitions.h"
#include "packager/media/formats/mp4/box_reader.h"
#include "packager/media/formats/mp4/es_descriptor.h"
#include "packager/media/formats/mp4/rcheck.h"
#include "packager/media/formats/mp4/track_run_iterator.h"

namespace {

uint64_t Rescale(uint64_t time_in_old_scale,
                 uint32_t old_scale,
                 uint32_t new_scale) {
  return (static_cast<double>(time_in_old_scale) / old_scale) * new_scale;
}


const char kWidevineKeySystemId[] = "edef8ba979d64acea3c827dcd51d21ed";

}  // namespace

namespace edash_packager {
namespace media {
namespace mp4 {

MP4MediaParser::MP4MediaParser()
    : state_(kWaitingForInit), moof_head_(0), mdat_tail_(0) {}

MP4MediaParser::~MP4MediaParser() {
  STLDeleteValues(&decryptor_map_);
}

void MP4MediaParser::Init(const InitCB& init_cb,
                          const NewSampleCB& new_sample_cb,
                          KeySource* decryption_key_source) {
  DCHECK_EQ(state_, kWaitingForInit);
  DCHECK(init_cb_.is_null());
  DCHECK(!init_cb.is_null());
  DCHECK(!new_sample_cb.is_null());

  ChangeState(kParsingBoxes);
  init_cb_ = init_cb;
  new_sample_cb_ = new_sample_cb;
  decryption_key_source_ = decryption_key_source;
}

void MP4MediaParser::Reset() {
  queue_.Reset();
  runs_.reset();
  moof_head_ = 0;
  mdat_tail_ = 0;
}

void MP4MediaParser::Flush() {
  DCHECK_NE(state_, kWaitingForInit);
  Reset();
  ChangeState(kParsingBoxes);
}

bool MP4MediaParser::Parse(const uint8_t* buf, int size) {
  DCHECK_NE(state_, kWaitingForInit);

  if (state_ == kError)
    return false;

  queue_.Push(buf, size);

  bool result, err = false;

  do {
    if (state_ == kParsingBoxes) {
      result = ParseBox(&err);
    } else {
      DCHECK_EQ(kEmittingSamples, state_);
      result = EnqueueSample(&err);
      if (result) {
        int64_t max_clear = runs_->GetMaxClearOffset() + moof_head_;
        err = !ReadAndDiscardMDATsUntil(max_clear);
      }
    }
  } while (result && !err);

  if (err) {
    DLOG(ERROR) << "Error while parsing MP4";
    moov_.reset();
    Reset();
    ChangeState(kError);
    return false;
  }

  return true;
}

bool MP4MediaParser::LoadMoov(const std::string& file_path) {
  scoped_ptr<File, FileCloser> file(
      File::OpenWithNoBuffering(file_path.c_str(), "r"));
  if (!file) {
    LOG(ERROR) << "Unable to open media file '" << file_path << "'";
    return false;
  }
  if (!file->Seek(0)) {
    LOG(WARNING) << "Filesystem does not support seeking on file '" << file_path
               << "'";
    return false;
  }

  uint64_t file_position(0);
  bool mdat_seen(false);
  while (true) {
    const uint32_t kBoxHeaderReadSize(16);
    std::vector<uint8_t> buffer(kBoxHeaderReadSize);
    int64_t bytes_read = file->Read(&buffer[0], kBoxHeaderReadSize);
    if (bytes_read == 0) {
      LOG(ERROR) << "Could not find 'moov' box in file '" << file_path << "'";
      return false;
    }
    if (bytes_read < kBoxHeaderReadSize) {
      LOG(ERROR) << "Error reading media file '" << file_path << "'";
      return false;
    }
    uint64_t box_size;
    FourCC box_type;
    bool err;
    if (!BoxReader::StartTopLevelBox(&buffer[0], kBoxHeaderReadSize, &box_type,
                                     &box_size, &err)) {
      LOG(ERROR) << "Could not start top level box from file '" << file_path
                 << "'";
      return false;
    }
    if (box_type == FOURCC_MDAT) {
      mdat_seen = true;
    } else if (box_type == FOURCC_MOOV) {
      if (!mdat_seen) {
        // 'moov' is before 'mdat'. Nothing to do.
        break;
      }
      // 'mdat' before 'moov'. Read and parse 'moov'.
      if (!Parse(&buffer[0], bytes_read)) {
        LOG(ERROR) << "Error parsing mp4 file '" << file_path << "'";
        return false;
      }
      uint64_t bytes_to_read = box_size - bytes_read;
      buffer.resize(bytes_to_read);
      while (bytes_to_read > 0) {
        bytes_read = file->Read(&buffer[0], bytes_to_read);
        if (bytes_read <= 0) {
          LOG(ERROR) << "Error reading 'moov' contents from file '" << file_path
                     << "'";
          return false;
        }
        if (!Parse(&buffer[0], bytes_read)) {
          LOG(ERROR) << "Error parsing mp4 file '" << file_path << "'";
          return false;
        }
        bytes_to_read -= bytes_read;
      }
      queue_.Reset();  // So that we don't need to adjust data offsets.
      mdat_tail_ = 0;  // So it will skip boxes until mdat.
      break;  // Done.
    }
    file_position += box_size;
    if (!file->Seek(file_position)) {
      LOG(ERROR) << "Error skipping box in mp4 file '" << file_path << "'";
      return false;
    }
  }
  return true;
}

bool MP4MediaParser::ParseBox(bool* err) {
  const uint8_t* buf;
  int size;
  queue_.Peek(&buf, &size);
  if (!size)
    return false;

  scoped_ptr<BoxReader> reader(BoxReader::ReadTopLevelBox(buf, size, err));
  if (reader.get() == NULL)
    return false;

  if (reader->type() == FOURCC_MDAT) {
    // The code ends up here only if a MOOV box is not yet seen.
    DCHECK(!moov_);

    NOTIMPLEMENTED() << " Files with MDAT before MOOV is not supported yet.";
    *err = true;
    return false;
  }

  // Set up mdat offset for ReadMDATsUntil().
  mdat_tail_ = queue_.head() + reader->size();

  if (reader->type() == FOURCC_MOOV) {
    *err = !ParseMoov(reader.get());
  } else if (reader->type() == FOURCC_MOOF) {
    moof_head_ = queue_.head();
    *err = !ParseMoof(reader.get());

    // Return early to avoid evicting 'moof' data from queue. Auxiliary info may
    // be located anywhere in the file, including inside the 'moof' itself.
    // (Since 'default-base-is-moof' is mandated, no data references can come
    // before the head of the 'moof', so keeping this box around is sufficient.)
    return !(*err);
  } else {
    VLOG(2) << "Skipping top-level box: " << FourCCToString(reader->type());
  }

  queue_.Pop(reader->size());
  return !(*err);
}

bool MP4MediaParser::ParseMoov(BoxReader* reader) {
  if (moov_)
    return true;  // Already parsed the 'moov' box.

  moov_.reset(new Movie);
  RCHECK(moov_->Parse(reader));
  runs_.reset();

  std::vector<scoped_refptr<StreamInfo> > streams;

  for (std::vector<Track>::const_iterator track = moov_->tracks.begin();
       track != moov_->tracks.end(); ++track) {
    const uint32_t timescale = track->media.header.timescale;

    // Calculate duration (based on timescale).
    uint64_t duration = 0;
    if (track->media.header.duration > 0) {
      duration = track->media.header.duration;
    } else if (moov_->extends.header.fragment_duration > 0) {
      DCHECK(moov_->header.timescale != 0);
      duration = Rescale(moov_->extends.header.fragment_duration,
                         moov_->header.timescale,
                         timescale);
    } else if (moov_->header.duration > 0 &&
               moov_->header.duration != std::numeric_limits<uint64_t>::max()) {
      DCHECK(moov_->header.timescale != 0);
      duration =
          Rescale(moov_->header.duration, moov_->header.timescale, timescale);
    }

    const SampleDescription& samp_descr =
        track->media.information.sample_table.description;

    size_t desc_idx = 0;

    // Read sample description index from mvex if it exists otherwise read
    // from the first entry in Sample To Chunk box.
    if (moov_->extends.tracks.size() > 0) {
      for (size_t t = 0; t < moov_->extends.tracks.size(); t++) {
        const TrackExtends& trex = moov_->extends.tracks[t];
        if (trex.track_id == track->header.track_id) {
          desc_idx = trex.default_sample_description_index;
          break;
        }
      }
    } else {
      const std::vector<ChunkInfo>& chunk_info =
          track->media.information.sample_table.sample_to_chunk.chunk_info;
      RCHECK(chunk_info.size() > 0);
      desc_idx = chunk_info[0].sample_description_index;
    }
    RCHECK(desc_idx > 0);
    desc_idx -= 1;  // BMFF descriptor index is one-based

    if (track->media.handler.type == kAudio) {
      RCHECK(!samp_descr.audio_entries.empty());

      // It is not uncommon to find otherwise-valid files with incorrect sample
      // description indices, so we fail gracefully in that case.
      if (desc_idx >= samp_descr.audio_entries.size())
        desc_idx = 0;
      const AudioSampleEntry& entry = samp_descr.audio_entries[desc_idx];

      if (!(entry.format == FOURCC_MP4A || entry.format == FOURCC_EAC3 ||
            (entry.format == FOURCC_ENCA &&
             entry.sinf.format.format == FOURCC_MP4A))) {
        LOG(ERROR) << "Unsupported audio format 0x"
                   << std::hex << entry.format << " in stsd box.";
        return false;
      }

      ObjectType audio_type = entry.esds.es_descriptor.object_type();
      DVLOG(1) << "audio_type " << std::hex << audio_type;
      if (audio_type == kForbidden && entry.format == FOURCC_EAC3) {
        audio_type = kEAC3;
      }

      AudioCodec codec = kUnknownAudioCodec;
      uint8_t num_channels = 0;
      uint32_t sampling_frequency = 0;
      uint8_t audio_object_type = 0;
      std::vector<uint8_t> extra_data;
      // Check if it is MPEG4 AAC defined in ISO 14496 Part 3 or
      // supported MPEG2 AAC variants.
      if (entry.esds.es_descriptor.IsAAC()) {
        codec = kCodecAAC;
        const AACAudioSpecificConfig& aac_audio_specific_config =
            entry.esds.aac_audio_specific_config;
        num_channels = aac_audio_specific_config.num_channels();
        sampling_frequency = aac_audio_specific_config.frequency();
        audio_object_type = aac_audio_specific_config.audio_object_type();
        extra_data = entry.esds.es_descriptor.decoder_specific_info();
      } else if (audio_type == kEAC3) {
        codec = kCodecEAC3;
        num_channels = entry.channelcount;
        sampling_frequency = entry.samplerate;
      } else {
        LOG(ERROR) << "Unsupported audio object type 0x"
                   << std::hex << audio_type << " in esds.";
        return false;
      }

      bool is_encrypted = entry.sinf.info.track_encryption.is_encrypted;
      DVLOG(1) << "is_audio_track_encrypted_: " << is_encrypted;
      streams.push_back(new AudioStreamInfo(
          track->header.track_id,
          timescale,
          duration,
          codec,
          AudioStreamInfo::GetCodecString(codec, audio_object_type),
          track->media.header.language,
          entry.samplesize,
          num_channels,
          sampling_frequency,
          extra_data.size() ? &extra_data[0] : NULL,
          extra_data.size(),
          is_encrypted));
    }

    if (track->media.handler.type == kVideo) {
      RCHECK(!samp_descr.video_entries.empty());
      if (desc_idx >= samp_descr.video_entries.size())
        desc_idx = 0;
      const VideoSampleEntry& entry = samp_descr.video_entries[desc_idx];

      if (!(entry.format == FOURCC_AVC1 ||
            (entry.format == FOURCC_ENCV &&
             entry.sinf.format.format == FOURCC_AVC1))) {
        LOG(ERROR) << "Unsupported video format 0x"
                   << std::hex << entry.format << " in stsd box.";
        return false;
      }

      const std::string codec_string =
          VideoStreamInfo::GetCodecString(kCodecH264,
                                          entry.avcc.profile_indication,
                                          entry.avcc.profile_compatibility,
                                          entry.avcc.avc_level);

      bool is_encrypted = entry.sinf.info.track_encryption.is_encrypted;
      DVLOG(1) << "is_video_track_encrypted_: " << is_encrypted;
      streams.push_back(new VideoStreamInfo(track->header.track_id,
                                            timescale,
                                            duration,
                                            kCodecH264,
                                            codec_string,
                                            track->media.header.language,
                                            entry.width,
                                            entry.height,
                                            0,  // trick_play_rate
                                            entry.avcc.length_size,
                                            &entry.avcc.data[0],
                                            entry.avcc.data.size(),
                                            is_encrypted));
    }
  }

  init_cb_.Run(streams);
  if (!FetchKeysIfNecessary(moov_->pssh))
    return false;
  runs_.reset(new TrackRunIterator(moov_.get()));
  RCHECK(runs_->Init());
  ChangeState(kEmittingSamples);
  return true;
}

bool MP4MediaParser::ParseMoof(BoxReader* reader) {
  // Must already have initialization segment.
  RCHECK(moov_.get());
  MovieFragment moof;
  RCHECK(moof.Parse(reader));
  if (!runs_)
    runs_.reset(new TrackRunIterator(moov_.get()));
  RCHECK(runs_->Init(moof));
  if (!FetchKeysIfNecessary(moof.pssh))
    return false;
  ChangeState(kEmittingSamples);
  return true;
}

bool MP4MediaParser::FetchKeysIfNecessary(
    const std::vector<ProtectionSystemSpecificHeader>& headers) {
  if (headers.empty())
    return true;

  // An error will be returned later if the samples need to be decrypted.
  if (!decryption_key_source_)
    return true;

  // TODO(tinskip): Pass in raw 'pssh' boxes to FetchKeys. This will allow
  // supporting multiple keysystems. Move this to KeySource.
  std::vector<uint8_t> widevine_system_id;
  base::HexStringToBytes(kWidevineKeySystemId, &widevine_system_id);
  for (std::vector<ProtectionSystemSpecificHeader>::const_iterator iter =
           headers.begin(); iter != headers.end(); ++iter) {
    if (iter->system_id == widevine_system_id) {
      Status status = decryption_key_source_->FetchKeys(iter->data);
      if (!status.ok()) {
        LOG(ERROR) << "Error fetching decryption keys: " << status;
        return false;
      }
      return true;
    }
  }

  LOG(ERROR) << "No viable 'pssh' box found for content decryption.";
  return false;
}

bool MP4MediaParser::EnqueueSample(bool* err) {
  if (!runs_->IsRunValid()) {
    // Remain in kEnqueueingSamples state, discarding data, until the end of
    // the current 'mdat' box has been appended to the queue.
    if (!queue_.Trim(mdat_tail_))
      return false;

    ChangeState(kParsingBoxes);
    return true;
  }

  if (!runs_->IsSampleValid()) {
    runs_->AdvanceRun();
    return true;
  }

  DCHECK(!(*err));

  const uint8_t* buf;
  int buf_size;
  queue_.Peek(&buf, &buf_size);
  if (!buf_size)
    return false;

  // Skip this entire track if it is not audio nor video.
  if (!runs_->is_audio() && !runs_->is_video())
    runs_->AdvanceRun();

  // Attempt to cache the auxiliary information first. Aux info is usually
  // placed in a contiguous block before the sample data, rather than being
  // interleaved. If we didn't cache it, this would require that we retain the
  // start of the segment buffer while reading samples. Aux info is typically
  // quite small compared to sample data, so this pattern is useful on
  // memory-constrained devices where the source buffer consumes a substantial
  // portion of the total system memory.
  if (runs_->AuxInfoNeedsToBeCached()) {
    queue_.PeekAt(runs_->aux_info_offset() + moof_head_, &buf, &buf_size);
    if (buf_size < runs_->aux_info_size())
      return false;
    *err = !runs_->CacheAuxInfo(buf, buf_size);
    return !*err;
  }

  int64_t sample_offset = runs_->sample_offset() + moof_head_;
  queue_.PeekAt(sample_offset, &buf, &buf_size);
  if (buf_size < runs_->sample_size()) {
    if (sample_offset < queue_.head()) {
      LOG(ERROR) << "Incorrect sample offset " << sample_offset
                 << " < " << queue_.head();
      *err = true;
    }
    return false;
  }

  scoped_refptr<MediaSample> stream_sample(MediaSample::CopyFrom(
      buf, runs_->sample_size(), runs_->is_keyframe()));
  if (runs_->is_encrypted()) {
    scoped_ptr<DecryptConfig> decrypt_config = runs_->GetDecryptConfig();
    if (!decrypt_config ||
        !DecryptSampleBuffer(decrypt_config.get(),
                             stream_sample->writable_data(),
                             stream_sample->data_size())) {
      *err = true;
      LOG(ERROR) << "Cannot decrypt samples.";
      return false;
    }
  }

  stream_sample->set_dts(runs_->dts());
  stream_sample->set_pts(runs_->cts());
  stream_sample->set_duration(runs_->duration());

  DVLOG(3) << "Pushing frame: "
           << ", key=" << runs_->is_keyframe()
           << ", dur=" << runs_->duration()
           << ", dts=" << runs_->dts()
           << ", cts=" << runs_->cts()
           << ", size=" << runs_->sample_size();

  if (!new_sample_cb_.Run(runs_->track_id(), stream_sample)) {
    *err = true;
    LOG(ERROR) << "Failed to process the sample.";
    return false;
  }

  runs_->AdvanceSample();
  return true;
}

bool MP4MediaParser::DecryptSampleBuffer(const DecryptConfig* decrypt_config,
                                         uint8_t* buffer,
                                         size_t buffer_size) {
  DCHECK(decrypt_config);
  DCHECK(buffer);

  if (!decryption_key_source_) {
    LOG(ERROR) << "Encrypted media sample encountered, but decryption is not "
                  "enabled";
    return false;
  }

  // Get the encryptor object.
  AesCtrEncryptor* encryptor;
  DecryptorMap::iterator found = decryptor_map_.find(decrypt_config->key_id());
  if (found == decryptor_map_.end()) {
    // Create new AesCtrEncryptor
    EncryptionKey key;
    Status status(decryption_key_source_->GetKey(decrypt_config->key_id(),
                                                 &key));
    if (!status.ok()) {
      LOG(ERROR) << "Error retrieving decryption key: " << status;
      return false;
    }
    scoped_ptr<AesCtrEncryptor> new_encryptor(new AesCtrEncryptor);
    if (!new_encryptor->InitializeWithIv(key.key, decrypt_config->iv())) {
      LOG(ERROR) << "Failed to initialize AesCtrEncryptor for decryption.";
      return false;
    }
    encryptor = new_encryptor.release();
    decryptor_map_[decrypt_config->key_id()] = encryptor;
  } else {
    encryptor = found->second;
  }
  if (!encryptor->SetIv(decrypt_config->iv())) {
    LOG(ERROR) << "Invalid initialization vector.";
    return false;
  }

  if (decrypt_config->subsamples().empty()) {
    // Sample not encrypted using subsample encryption. Decrypt whole.
    if (!encryptor->Decrypt(buffer, buffer_size, buffer)) {
      LOG(ERROR) << "Error during bulk sample decryption.";
      return false;
    }
    return true;
  }

  // Subsample decryption.
  const std::vector<SubsampleEntry>& subsamples = decrypt_config->subsamples();
  uint8_t* current_ptr = buffer;
  const uint8_t* buffer_end = buffer + buffer_size;
  current_ptr += decrypt_config->data_offset();
  if (current_ptr > buffer_end) {
    LOG(ERROR) << "Subsample data_offset too large.";
    return false;
  }
  for (std::vector<SubsampleEntry>::const_iterator iter = subsamples.begin();
       iter != subsamples.end();
       ++iter) {
    if ((current_ptr + iter->clear_bytes + iter->cipher_bytes) > buffer_end) {
      LOG(ERROR) << "Subsamples overflow sample buffer.";
      return false;
    }
    current_ptr += iter->clear_bytes;
    if (!encryptor->Decrypt(current_ptr, iter->cipher_bytes, current_ptr)) {
      LOG(ERROR) << "Error decrypting subsample buffer.";
      return false;
    }
    current_ptr += iter->cipher_bytes;
  }
  return true;
}

bool MP4MediaParser::ReadAndDiscardMDATsUntil(const int64_t offset) {
  bool err = false;
  while (mdat_tail_ < offset) {
    const uint8_t* buf;
    int size;
    queue_.PeekAt(mdat_tail_, &buf, &size);

    FourCC type;
    uint64_t box_sz;
    if (!BoxReader::StartTopLevelBox(buf, size, &type, &box_sz, &err))
      break;

    mdat_tail_ += box_sz;
  }
  queue_.Trim(std::min(mdat_tail_, offset));
  return !err;
}

void MP4MediaParser::ChangeState(State new_state) {
  DVLOG(2) << "Changing state: " << new_state;
  state_ = new_state;
}

}  // namespace mp4
}  // namespace media
}  // namespace edash_packager
