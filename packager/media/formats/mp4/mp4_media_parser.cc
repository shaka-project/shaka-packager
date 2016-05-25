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
#include "packager/media/base/audio_stream_info.h"
#include "packager/media/base/buffer_reader.h"
#include "packager/media/base/decrypt_config.h"
#include "packager/media/base/key_source.h"
#include "packager/media/base/macros.h"
#include "packager/media/base/media_sample.h"
#include "packager/media/base/rcheck.h"
#include "packager/media/base/video_stream_info.h"
#include "packager/media/file/file.h"
#include "packager/media/file/file_closer.h"
#include "packager/media/filters/avc_decoder_configuration.h"
#include "packager/media/filters/hevc_decoder_configuration.h"
#include "packager/media/filters/vp_codec_configuration.h"
#include "packager/media/formats/mp4/box_definitions.h"
#include "packager/media/formats/mp4/box_reader.h"
#include "packager/media/formats/mp4/es_descriptor.h"
#include "packager/media/formats/mp4/track_run_iterator.h"

namespace shaka {
namespace media {
namespace mp4 {
namespace {

uint64_t Rescale(uint64_t time_in_old_scale,
                 uint32_t old_scale,
                 uint32_t new_scale) {
  return (static_cast<double>(time_in_old_scale) / old_scale) * new_scale;
}

VideoCodec FourCCToVideoCodec(FourCC fourcc) {
  switch (fourcc) {
    case FOURCC_avc1:
      return kCodecH264;
    case FOURCC_hev1:
      return kCodecHEV1;
    case FOURCC_hvc1:
      return kCodecHVC1;
    case FOURCC_vp08:
      return kCodecVP8;
    case FOURCC_vp09:
      return kCodecVP9;
    case FOURCC_vp10:
      return kCodecVP10;
    default:
      return kUnknownVideoCodec;
  }
}

AudioCodec FourCCToAudioCodec(FourCC fourcc) {
  switch(fourcc) {
    case FOURCC_dtsc:
      return kCodecDTSC;
    case FOURCC_dtsh:
      return kCodecDTSH;
    case FOURCC_dtsl:
      return kCodecDTSL;
    case FOURCC_dtse:
      return kCodecDTSE;
    case FOURCC_dtsp:
      return kCodecDTSP;
    case FOURCC_dtsm:
      return kCodecDTSM;
    case FOURCC_ac_3:
      return kCodecAC3;
    case FOURCC_ec_3:
      return kCodecEAC3;
    default:
      return kUnknownAudioCodec;
  }
}

// Default DTS audio number of channels for 5.1 channel layout.
const uint8_t kDtsAudioNumChannels = 6;
const uint64_t kNanosecondsPerSecond = 1000000000ull;

}  // namespace

MP4MediaParser::MP4MediaParser()
    : state_(kWaitingForInit),
      decryption_key_source_(NULL),
      moof_head_(0),
      mdat_tail_(0) {}

MP4MediaParser::~MP4MediaParser() {}

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
  if (decryption_key_source)
    decryptor_source_.reset(new DecryptorSource(decryption_key_source));
}

void MP4MediaParser::Reset() {
  queue_.Reset();
  runs_.reset();
  moof_head_ = 0;
  mdat_tail_ = 0;
}

bool MP4MediaParser::Flush() {
  DCHECK_NE(state_, kWaitingForInit);
  Reset();
  ChangeState(kParsingBoxes);
  return true;
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
    if (box_type == FOURCC_mdat) {
      mdat_seen = true;
    } else if (box_type == FOURCC_moov) {
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

  if (reader->type() == FOURCC_mdat) {
    // The code ends up here only if a MOOV box is not yet seen.
    DCHECK(!moov_);

    NOTIMPLEMENTED() << " Files with MDAT before MOOV is not supported yet.";
    *err = true;
    return false;
  }

  // Set up mdat offset for ReadMDATsUntil().
  mdat_tail_ = queue_.head() + reader->size();

  if (reader->type() == FOURCC_moov) {
    *err = !ParseMoov(reader.get());
  } else if (reader->type() == FOURCC_moof) {
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

    if (samp_descr.type == kAudio) {
      RCHECK(!samp_descr.audio_entries.empty());

      // It is not uncommon to find otherwise-valid files with incorrect sample
      // description indices, so we fail gracefully in that case.
      if (desc_idx >= samp_descr.audio_entries.size())
        desc_idx = 0;

      const AudioSampleEntry& entry = samp_descr.audio_entries[desc_idx];
      const FourCC actual_format = entry.GetActualFormat();
      AudioCodec codec = FourCCToAudioCodec(actual_format);
      uint8_t num_channels = 0;
      uint32_t sampling_frequency = 0;
      uint64_t codec_delay_ns = 0;
      uint8_t audio_object_type = 0;
      uint32_t max_bitrate = 0;
      uint32_t avg_bitrate = 0;
      std::vector<uint8_t> extra_data;

      switch (actual_format) {
        case FOURCC_mp4a:
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
            break;
          } else if (entry.esds.es_descriptor.IsDTS()) {
            ObjectType audio_type = entry.esds.es_descriptor.object_type();
            switch (audio_type) {
              case kDTSC:
                codec = kCodecDTSC;
                break;
              case kDTSE:
                codec = kCodecDTSE;
                break;
              case kDTSH:
                codec = kCodecDTSH;
                break;
              case kDTSL:
                codec = kCodecDTSL;
                break;
              default:
                LOG(ERROR) << "Unsupported audio type " << audio_type
                           << " in stsd box.";
                return false;
            }
            num_channels = entry.esds.aac_audio_specific_config.num_channels();
            // For dts audio in esds, current supported number of channels is 6
            // as the only supported channel layout is 5.1.
            if (num_channels != kDtsAudioNumChannels) {
              LOG(ERROR) << "Unsupported channel count " << num_channels
                         << " for audio type " << audio_type << ".";
              return false;
            }
            sampling_frequency = entry.samplerate;
            max_bitrate = entry.esds.es_descriptor.max_bitrate();
            avg_bitrate = entry.esds.es_descriptor.avg_bitrate();
          } else {
            LOG(ERROR) << "Unsupported audio format 0x" << std::hex
                       << actual_format << " in stsd box.";
            return false;
          }
          break;
        case FOURCC_dtsc:
          FALLTHROUGH_INTENDED;
        case FOURCC_dtsh:
          FALLTHROUGH_INTENDED;
        case FOURCC_dtsl:
          FALLTHROUGH_INTENDED;
        case FOURCC_dtse:
          FALLTHROUGH_INTENDED;
        case FOURCC_dtsm:
          extra_data = entry.ddts.extra_data;
          max_bitrate = entry.ddts.max_bitrate;
          avg_bitrate = entry.ddts.avg_bitrate;
          num_channels = entry.channelcount;
          sampling_frequency = entry.samplerate;
          break;
        case FOURCC_ac_3:
          extra_data = entry.dac3.data;
          num_channels = entry.channelcount;
          sampling_frequency = entry.samplerate;
          break;
        case FOURCC_ec_3:
          extra_data = entry.dec3.data;
          num_channels = entry.channelcount;
          sampling_frequency = entry.samplerate;
          break;
        case FOURCC_Opus:
          extra_data = entry.dops.opus_identification_header;
          num_channels = entry.channelcount;
          sampling_frequency = entry.samplerate;
          RCHECK(sampling_frequency != 0);
          codec_delay_ns =
              entry.dops.preskip * kNanosecondsPerSecond / sampling_frequency;
          break;
        default:
          LOG(ERROR) << "Unsupported audio format 0x" << std::hex
                     << actual_format << " in stsd box.";
          return false;
      }

      // Extract possible seek preroll.
      uint64_t seek_preroll_ns = 0;
      for (const auto& sample_group_description :
           track->media.information.sample_table.sample_group_descriptions) {
        if (sample_group_description.grouping_type != FOURCC_roll)
          continue;
        const auto& audio_roll_recovery_entries =
            sample_group_description.audio_roll_recovery_entries;
        if (audio_roll_recovery_entries.size() != 1) {
          LOG(WARNING) << "Unexpected number of entries in "
                          "SampleGroupDescription table with grouping type "
                          "'roll'.";
          break;
        }
        const int16_t roll_distance_in_samples =
            audio_roll_recovery_entries[0].roll_distance;
        if (roll_distance_in_samples < 0) {
          RCHECK(sampling_frequency != 0);
          seek_preroll_ns = kNanosecondsPerSecond *
                            (-roll_distance_in_samples) / sampling_frequency;
        } else {
          LOG(WARNING)
              << "Roll distance is supposed to be negative, but seeing "
              << roll_distance_in_samples;
        }
        break;
      }

      const bool is_encrypted =
          entry.sinf.info.track_encryption.default_is_protected == 1;
      DVLOG(1) << "is_audio_track_encrypted_: " << is_encrypted;
      streams.push_back(new AudioStreamInfo(
          track->header.track_id,
          timescale,
          duration,
          codec,
          AudioStreamInfo::GetCodecString(codec, audio_object_type),
          track->media.header.language.code,
          entry.samplesize,
          num_channels,
          sampling_frequency,
          seek_preroll_ns,
          codec_delay_ns,
          max_bitrate,
          avg_bitrate,
          extra_data.data(),
          extra_data.size(),
          is_encrypted));
    }

    if (samp_descr.type == kVideo) {
      RCHECK(!samp_descr.video_entries.empty());
      if (desc_idx >= samp_descr.video_entries.size())
        desc_idx = 0;
      const VideoSampleEntry& entry = samp_descr.video_entries[desc_idx];

      uint32_t coded_width = entry.width;
      uint32_t coded_height = entry.height;
      uint32_t pixel_width = entry.pixel_aspect.h_spacing;
      uint32_t pixel_height = entry.pixel_aspect.v_spacing;
      if (pixel_width == 0 && pixel_height == 0) {
        pixel_width = 1;
        pixel_height = 1;
      }
      std::string codec_string;
      uint8_t nalu_length_size = 0;

      const FourCC actual_format = entry.GetActualFormat();
      const VideoCodec video_codec = FourCCToVideoCodec(actual_format);
      switch (actual_format) {
        case FOURCC_avc1: {
          AVCDecoderConfiguration avc_config;
          if (!avc_config.Parse(entry.codec_configuration.data)) {
            LOG(ERROR) << "Failed to parse avcc.";
            return false;
          }
          codec_string = avc_config.GetCodecString();
          nalu_length_size = avc_config.nalu_length_size();

          if (coded_width != avc_config.coded_width() ||
              coded_height != avc_config.coded_height()) {
            LOG(WARNING) << "Resolution in VisualSampleEntry (" << coded_width
                         << "," << coded_height
                         << ") does not match with resolution in "
                            "AVCDecoderConfigurationRecord ("
                         << avc_config.coded_width() << ","
                         << avc_config.coded_height()
                         << "). Use AVCDecoderConfigurationRecord.";
            coded_width = avc_config.coded_width();
            coded_height = avc_config.coded_height();
          }

          if (pixel_width != avc_config.pixel_width() ||
              pixel_height != avc_config.pixel_height()) {
            LOG_IF(WARNING, pixel_width != 1 || pixel_height != 1)
                << "Pixel aspect ratio in PASP box (" << pixel_width << ","
                << pixel_height
                << ") does not match with SAR in AVCDecoderConfigurationRecord "
                   "("
                << avc_config.pixel_width() << "," << avc_config.pixel_height()
                << "). Use AVCDecoderConfigurationRecord.";
            pixel_width = avc_config.pixel_width();
            pixel_height = avc_config.pixel_height();
          }
          break;
        }
        case FOURCC_hev1:
        case FOURCC_hvc1: {
          HEVCDecoderConfiguration hevc_config;
          if (!hevc_config.Parse(entry.codec_configuration.data)) {
            LOG(ERROR) << "Failed to parse hevc.";
            return false;
          }
          codec_string = hevc_config.GetCodecString(video_codec);
          nalu_length_size = hevc_config.nalu_length_size();
          break;
        }
        case FOURCC_vp08:
        case FOURCC_vp09:
        case FOURCC_vp10: {
          VPCodecConfiguration vp_config;
          if (!vp_config.Parse(entry.codec_configuration.data)) {
            LOG(ERROR) << "Failed to parse vpcc.";
            return false;
          }
          codec_string = vp_config.GetCodecString(video_codec);
          break;
        }
        default:
          LOG(ERROR) << "Unsupported video format "
                     << FourCCToString(actual_format) << " in stsd box.";
        return false;
      }

      const bool is_encrypted =
          entry.sinf.info.track_encryption.default_is_protected == 1;
      DVLOG(1) << "is_video_track_encrypted_: " << is_encrypted;
      streams.push_back(new VideoStreamInfo(
          track->header.track_id, timescale, duration, video_codec,
          codec_string, track->media.header.language.code, coded_width,
          coded_height, pixel_width, pixel_height,
          0,  // trick_play_rate
          nalu_length_size, entry.codec_configuration.data.data(),
          entry.codec_configuration.data.size(), is_encrypted));
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

  Status status;
  for (std::vector<ProtectionSystemSpecificHeader>::const_iterator iter =
           headers.begin(); iter != headers.end(); ++iter) {
    status = decryption_key_source_->FetchKeys(iter->raw_box);
    if (!status.ok()) {
      // If there is an error, try using the next PSSH box and report if none
      // work.
      VLOG(1) << "Unable to fetch decryption keys: " << status
              << ", trying the next PSSH box";
      continue;
    }
    return true;
  }

  if (!status.ok()) {
    LOG(ERROR) << "Error fetching decryption keys: " << status;
    return false;
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
    if (!decryptor_source_) {
      *err = true;
      LOG(ERROR) << "Encrypted media sample encountered, but decryption is not "
                    "enabled";
      return false;
    }

    scoped_ptr<DecryptConfig> decrypt_config = runs_->GetDecryptConfig();
    if (!decrypt_config ||
        !decryptor_source_->DecryptSampleBuffer(decrypt_config.get(),
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
}  // namespace shaka
