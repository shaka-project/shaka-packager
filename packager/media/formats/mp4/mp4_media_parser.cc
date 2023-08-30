// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "packager/media/formats/mp4/mp4_media_parser.h"

#include <algorithm>
#include <limits>

#include "packager/base/callback.h"
#include "packager/base/callback_helpers.h"
#include "packager/base/logging.h"
#include "packager/base/strings/string_number_conversions.h"
#include "packager/file/file.h"
#include "packager/file/file_closer.h"
#include "packager/media/base/audio_stream_info.h"
#include "packager/media/base/buffer_reader.h"
#include "packager/media/base/decrypt_config.h"
#include "packager/media/base/key_source.h"
#include "packager/media/base/macros.h"
#include "packager/media/base/media_sample.h"
#include "packager/media/base/rcheck.h"
#include "packager/media/base/video_stream_info.h"
#include "packager/media/base/video_util.h"
#include "packager/media/codecs/ac3_audio_util.h"
#include "packager/media/codecs/av1_codec_configuration_record.h"
#include "packager/media/codecs/avc_decoder_configuration_record.h"
#include "packager/media/codecs/dovi_decoder_configuration_record.h"
#include "packager/media/codecs/ec3_audio_util.h"
#include "packager/media/codecs/ac4_audio_util.h"
#include "packager/media/codecs/es_descriptor.h"
#include "packager/media/codecs/hevc_decoder_configuration_record.h"
#include "packager/media/codecs/vp_codec_configuration_record.h"
#include "packager/media/formats/mp4/box_definitions.h"
#include "packager/media/formats/mp4/box_reader.h"
#include "packager/media/formats/mp4/track_run_iterator.h"

namespace shaka {
namespace media {
namespace mp4 {
namespace {

int64_t Rescale(int64_t time_in_old_scale,
                int32_t old_scale,
                int32_t new_scale) {
  return (static_cast<double>(time_in_old_scale) / old_scale) * new_scale;
}

H26xStreamFormat GetH26xStreamFormat(FourCC fourcc) {
  switch (fourcc) {
    case FOURCC_avc1:
    case FOURCC_dvh1:
    case FOURCC_hvc1:
      return H26xStreamFormat::kNalUnitStreamWithoutParameterSetNalus;
    case FOURCC_avc3:
    case FOURCC_dvhe:
    case FOURCC_hev1:
      return H26xStreamFormat::kNalUnitStreamWithParameterSetNalus;
    default:
      return H26xStreamFormat::kUnSpecified;
  }
}

Codec FourCCToCodec(FourCC fourcc) {
  switch (fourcc) {
    case FOURCC_av01:
      return kCodecAV1;
    case FOURCC_avc1:
    case FOURCC_avc3:
      return kCodecH264;
    case FOURCC_dvh1:
    case FOURCC_dvhe:
      return kCodecH265DolbyVision;
    case FOURCC_hev1:
    case FOURCC_hvc1:
      return kCodecH265;
    case FOURCC_vp08:
      return kCodecVP8;
    case FOURCC_vp09:
      return kCodecVP9;
    case FOURCC_Opus:
      return kCodecOpus;
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
    case FOURCC_ac_4:
      return kCodecAC4;
    case FOURCC_fLaC:
      return kCodecFlac;
    case FOURCC_mha1:
      return kCodecMha1;
    case FOURCC_mhm1:
      return kCodecMhm1;
    default:
      return kUnknownCodec;
  }
}

Codec ObjectTypeToCodec(ObjectType object_type) {
  switch (object_type) {
    case ObjectType::kISO_14496_3:
    case ObjectType::kISO_13818_7_AAC_LC:
      return kCodecAAC;
    case ObjectType::kDTSC:
      return kCodecDTSC;
    case ObjectType::kDTSE:
      return kCodecDTSE;
    case ObjectType::kDTSH:
      return kCodecDTSH;
    case ObjectType::kDTSL:
      return kCodecDTSL;
    default:
      return kUnknownCodec;
  }
}

std::vector<uint8_t> GetDOVIDecoderConfig(
    const std::vector<CodecConfiguration>& configs) {
  for (const CodecConfiguration& config : configs) {
    if (config.box_type == FOURCC_dvcC || config.box_type == FOURCC_dvvC) {
      return config.data;
    }
  }
  return std::vector<uint8_t>();
}

bool UpdateCodecStringForDolbyVision(
    FourCC actual_format,
    const std::vector<CodecConfiguration>& configs,
    std::string* codec_string) {
  DOVIDecoderConfigurationRecord dovi_config;
  if (!dovi_config.Parse(GetDOVIDecoderConfig(configs))) {
    LOG(ERROR) << "Failed to parse Dolby Vision decoder "
                  "configuration record.";
    return false;
  }
  switch (actual_format) {
    case FOURCC_dvh1:
    case FOURCC_dvhe:
      // Non-Backward compatibility mode. Replace the code string with
      // Dolby Vision only.
      *codec_string = dovi_config.GetCodecString(actual_format);
      break;
    case FOURCC_hev1:
      // Backward compatibility mode. Two codecs are signalled: base codec
      // without Dolby Vision and HDR with Dolby Vision.
      *codec_string += ";" + dovi_config.GetCodecString(FOURCC_dvhe);
      break;
    case FOURCC_hvc1:
      // See above.
      *codec_string += ";" + dovi_config.GetCodecString(FOURCC_dvh1);
      break;
    default:
      LOG(ERROR) << "Unsupported format with extra codec "
                 << FourCCToString(actual_format);
      return false;
  }
  return true;
}

const uint64_t kNanosecondsPerSecond = 1000000000ull;

}  // namespace

MP4MediaParser::MP4MediaParser()
    : state_(kWaitingForInit),
      decryption_key_source_(NULL),
      moof_head_(0),
      mdat_tail_(0) {}

MP4MediaParser::~MP4MediaParser() {}

void MP4MediaParser::Init(const InitCB& init_cb,
                          const NewMediaSampleCB& new_media_sample_cb,
                          const NewTextSampleCB& new_text_sample_cb,
                          KeySource* decryption_key_source) {
  DCHECK_EQ(state_, kWaitingForInit);
  DCHECK(init_cb_.is_null());
  DCHECK(!init_cb.is_null());
  DCHECK(!new_media_sample_cb.is_null());

  ChangeState(kParsingBoxes);
  init_cb_ = init_cb;
  new_sample_cb_ = new_media_sample_cb;
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
  std::unique_ptr<File, FileCloser> file(
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
    if (!BoxReader::StartBox(&buffer[0], kBoxHeaderReadSize, &box_type,
                             &box_size, &err)) {
      LOG(ERROR) << "Could not start box from file '" << file_path << "'";
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

  std::unique_ptr<BoxReader> reader(BoxReader::ReadBox(buf, size, err));
  if (reader.get() == NULL)
    return false;

  if (reader->type() == FOURCC_mdat) {
    if (!moov_) {
      // For seekable files, we seek to the 'moov' and load the 'moov' first
      // then seek back (see LoadMoov function for details); we do not support
      // having 'mdat' before 'moov' for non-seekable files. The code ends up
      // here only if it is a non-seekable file.
      NOTIMPLEMENTED() << " Non-seekable Files with 'mdat' box before 'moov' "
                          "box is not supported.";
      *err = true;
      return false;
    } else {
      // This can happen if there are unused 'mdat' boxes, which is unusual
      // but allowed by the spec. Ignore the 'mdat' and proceed.
      LOG(INFO)
          << "Ignore unused 'mdat' box - this could be as a result of extra "
             "not usable 'mdat' or 'mdat' associated with unrecognized track.";
    }
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

  queue_.Pop(static_cast<int>(reader->size()));
  return !(*err);
}

bool MP4MediaParser::ParseMoov(BoxReader* reader) {
  if (moov_)
    return true;  // Already parsed the 'moov' box.

  moov_.reset(new Movie);
  RCHECK(moov_->Parse(reader));
  runs_.reset();

  std::vector<std::shared_ptr<StreamInfo>> streams;

  for (std::vector<Track>::const_iterator track = moov_->tracks.begin();
       track != moov_->tracks.end(); ++track) {
    const int32_t timescale = track->media.header.timescale;

    // Calculate duration (based on timescale).
    int64_t duration = 0;
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
      Codec codec = FourCCToCodec(actual_format);
      uint8_t num_channels = entry.channelcount;
      uint32_t sampling_frequency = entry.samplerate;
      uint64_t codec_delay_ns = 0;
      uint8_t audio_object_type = 0;
      uint32_t max_bitrate = 0;
      uint32_t avg_bitrate = 0;
      std::vector<uint8_t> codec_config;

      switch (actual_format) {
        case FOURCC_mp4a: {
          const DecoderConfigDescriptor& decoder_config =
              entry.esds.es_descriptor.decoder_config_descriptor();
          max_bitrate = decoder_config.max_bitrate();
          avg_bitrate = decoder_config.avg_bitrate();

          codec = ObjectTypeToCodec(decoder_config.object_type());
          if (codec == kCodecAAC) {
            const AACAudioSpecificConfig& aac_audio_specific_config =
                entry.esds.aac_audio_specific_config;
            num_channels = aac_audio_specific_config.GetNumChannels();
            sampling_frequency =
                aac_audio_specific_config.GetSamplesPerSecond();
            audio_object_type = aac_audio_specific_config.GetAudioObjectType();
            codec_config =
                decoder_config.decoder_specific_info_descriptor().data();
          } else if (codec == kUnknownCodec) {
            // Intentionally not to fail in the parser as there may be multiple
            // streams in the source content, which allows the supported stream
            // to be packaged. An error will be returned if the unsupported
            // stream is passed to the muxer.
            LOG(WARNING) << "Unsupported audio object type "
                         << static_cast<int>(decoder_config.object_type())
                         << " in stsd.es_desriptor.";
          }
          break;
        }
        case FOURCC_dtsc:
          FALLTHROUGH_INTENDED;
        case FOURCC_dtse:
          FALLTHROUGH_INTENDED;
        case FOURCC_dtsh:
          FALLTHROUGH_INTENDED;
        case FOURCC_dtsl:
          FALLTHROUGH_INTENDED;
        case FOURCC_dtsm:
          codec_config = entry.ddts.extra_data;
          max_bitrate = entry.ddts.max_bitrate;
          avg_bitrate = entry.ddts.avg_bitrate;
          break;
        case FOURCC_ac_3:
          codec_config = entry.dac3.data;
          num_channels = static_cast<uint8_t>(GetAc3NumChannels(codec_config));
          break;
        case FOURCC_ec_3:
          codec_config = entry.dec3.data;
          num_channels = static_cast<uint8_t>(GetEc3NumChannels(codec_config));
          break;
        case FOURCC_ac_4:
          codec_config = entry.dac4.data;
          // Stop the process if have errors when parsing AC-4 dac4 box,
          // bitstream version 0 (has beed deprecated) and contains multiple
          // presentations in single AC-4 stream (only used for broadcast).
          if (!GetAc4CodecInfo(codec_config, &audio_object_type)) {
            LOG(ERROR) << "Failed to parse dac4.";
            return false;
          }
          break;
        case FOURCC_fLaC:
          codec_config = entry.dfla.data;
          break;
        case FOURCC_Opus:
          codec_config = entry.dops.opus_identification_header;
          codec_delay_ns =
              entry.dops.preskip * kNanosecondsPerSecond / sampling_frequency;
          break;
        case FOURCC_mha1:
        case FOURCC_mhm1:
          codec_config = entry.mhac.data;
          audio_object_type = entry.mhac.mpeg_h_3da_profile_level_indication;
          break;
        default:
          // Intentionally not to fail in the parser as there may be multiple
          // streams in the source content, which allows the supported stream to
          // be packaged.
          // An error will be returned if the unsupported stream is passed to
          // the muxer.
          LOG(WARNING) << "Unsupported audio format '"
                       << FourCCToString(actual_format) << "' in stsd box.";
          break;
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

      // The stream will be decrypted if a |decryptor_source_| is available.
      const bool is_encrypted =
          decryptor_source_
              ? false
              : entry.sinf.info.track_encryption.default_is_protected == 1;
      DVLOG(1) << "is_audio_track_encrypted_: " << is_encrypted;
      streams.emplace_back(new AudioStreamInfo(
          track->header.track_id, timescale, duration, codec,
          AudioStreamInfo::GetCodecString(codec, audio_object_type),
          codec_config.data(), codec_config.size(), entry.samplesize,
          num_channels, sampling_frequency, seek_preroll_ns, codec_delay_ns,
          max_bitrate, avg_bitrate, track->media.header.language.code,
          is_encrypted));
    }

    if (samp_descr.type == kVideo) {
      RCHECK(!samp_descr.video_entries.empty());
      if (desc_idx >= samp_descr.video_entries.size())
        desc_idx = 0;
      const VideoSampleEntry& entry = samp_descr.video_entries[desc_idx];
      std::vector<uint8_t> codec_configuration_data =
          entry.codec_configuration.data;

      uint32_t coded_width = entry.width;
      uint32_t coded_height = entry.height;
      uint32_t pixel_width = entry.pixel_aspect.h_spacing;
      uint32_t pixel_height = entry.pixel_aspect.v_spacing;
      if (pixel_width == 0 && pixel_height == 0) {
        DerivePixelWidthHeight(coded_width, coded_height, track->header.width,
                               track->header.height, &pixel_width,
                               &pixel_height);
      }
      std::string codec_string;
      uint8_t nalu_length_size = 0;
      uint8_t transfer_characteristics = 0;

      const FourCC actual_format = entry.GetActualFormat();
      const Codec video_codec = FourCCToCodec(actual_format);
      switch (actual_format) {
        case FOURCC_av01: {
          AV1CodecConfigurationRecord av1_config;
          if (!av1_config.Parse(codec_configuration_data)) {
            LOG(ERROR) << "Failed to parse av1c.";
            return false;
          }
          // Generate the full codec string if the colr atom is present.
          if (entry.colr.color_parameter_type != FOURCC_NULL) {
            codec_string = av1_config.GetCodecString(
                entry.colr.color_primaries, entry.colr.transfer_characteristics,
                entry.colr.matrix_coefficients,
                entry.colr.video_full_range_flag);
          } else {
            codec_string = av1_config.GetCodecString();
          }
          break;
        }
        case FOURCC_avc1:
        case FOURCC_avc3: {
          AVCDecoderConfigurationRecord avc_config;
          if (!avc_config.Parse(codec_configuration_data)) {
            LOG(ERROR) << "Failed to parse avcc.";
            return false;
          }
          codec_string = avc_config.GetCodecString(actual_format);
          nalu_length_size = avc_config.nalu_length_size();
          transfer_characteristics = avc_config.transfer_characteristics();

          // Use configurations from |avc_config| if it is valid.
          if (avc_config.coded_width() != 0) {
            DCHECK_NE(avc_config.coded_height(), 0u);
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

            DCHECK_NE(avc_config.pixel_width(), 0u);
            DCHECK_NE(avc_config.pixel_height(), 0u);
            if (pixel_width != avc_config.pixel_width() ||
                pixel_height != avc_config.pixel_height()) {
              LOG_IF(WARNING, pixel_width != 1 || pixel_height != 1)
                  << "Pixel aspect ratio in PASP box (" << pixel_width << ","
                  << pixel_height
                  << ") does not match with SAR in "
                     "AVCDecoderConfigurationRecord "
                     "("
                  << avc_config.pixel_width() << ","
                  << avc_config.pixel_height()
                  << "). Use AVCDecoderConfigurationRecord.";
              pixel_width = avc_config.pixel_width();
              pixel_height = avc_config.pixel_height();
            }
          }
          break;
        }
        case FOURCC_dvh1:
        case FOURCC_dvhe:
        case FOURCC_hev1:
        case FOURCC_hvc1: {
          HEVCDecoderConfigurationRecord hevc_config;
          if (!hevc_config.Parse(codec_configuration_data)) {
            LOG(ERROR) << "Failed to parse hevc.";
            return false;
          }
          codec_string = hevc_config.GetCodecString(actual_format);
          nalu_length_size = hevc_config.nalu_length_size();
          transfer_characteristics = hevc_config.transfer_characteristics();

          if (!entry.extra_codec_configs.empty()) {
            // |extra_codec_configs| is present only for Dolby Vision.
            if (!UpdateCodecStringForDolbyVision(
                    actual_format, entry.extra_codec_configs, &codec_string)) {
              return false;
            }
          }
          break;
        }
        case FOURCC_vp08:
        case FOURCC_vp09: {
          VPCodecConfigurationRecord vp_config;
          if (!vp_config.ParseMP4(codec_configuration_data)) {
            LOG(ERROR) << "Failed to parse vpcc.";
            return false;
          }
          if (actual_format == FOURCC_vp09 &&
              (!vp_config.is_level_set() || vp_config.level() == 0)) {
            const double kUnknownSampleDuration = 0.0;
            vp_config.SetVP9Level(coded_width, coded_height,
                                  kUnknownSampleDuration);
            vp_config.WriteMP4(&codec_configuration_data);
          }
          codec_string = vp_config.GetCodecString(video_codec);
          break;
        }
        default:
          // Intentionally not to fail in the parser as there may be multiple
          // streams in the source content, which allows the supported stream to
          // be packaged.
          // An error will be returned if the unsupported stream is passed to
          // the muxer.
          LOG(WARNING) << "Unsupported video format '"
                       << FourCCToString(actual_format) << "' in stsd box.";
          break;
      }

      // The stream will be decrypted if a |decryptor_source_| is available.
      const bool is_encrypted =
          decryptor_source_
              ? false
              : entry.sinf.info.track_encryption.default_is_protected == 1;
      DVLOG(1) << "is_video_track_encrypted_: " << is_encrypted;
      std::shared_ptr<VideoStreamInfo> video_stream_info(new VideoStreamInfo(
          track->header.track_id, timescale, duration, video_codec,
          GetH26xStreamFormat(actual_format), codec_string,
          codec_configuration_data.data(), codec_configuration_data.size(),
          coded_width, coded_height, pixel_width, pixel_height,
          transfer_characteristics,
          0,  // trick_play_factor
          nalu_length_size, track->media.header.language.code, is_encrypted));
      video_stream_info->set_extra_config(entry.ExtraCodecConfigsAsVector());
      video_stream_info->set_colr_data((entry.colr.raw_box).data(),
                                       (entry.colr.raw_box).size());

      // Set pssh raw data if it has.
      if (moov_->pssh.size() > 0) {
        std::vector<uint8_t> pssh_raw_data;
        for (const auto& pssh : moov_->pssh) {
          pssh_raw_data.insert(pssh_raw_data.end(), pssh.raw_box.begin(),
                               pssh.raw_box.end());
        }
        video_stream_info->set_eme_init_data(pssh_raw_data.data(),
                                             pssh_raw_data.size());
      }

      streams.push_back(video_stream_info);
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

  std::vector<uint8_t> pssh_raw_data;
  for (const auto& header : headers) {
    pssh_raw_data.insert(pssh_raw_data.end(), header.raw_box.begin(),
                         header.raw_box.end());
  }
  Status status =
      decryption_key_source_->FetchKeys(EmeInitDataType::CENC, pssh_raw_data);
  if (!status.ok()) {
    LOG(ERROR) << "Error fetching decryption keys: " << status;
    return false;
  }
  return true;
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

  const uint8_t* media_data = buf;
  const size_t media_data_size = runs_->sample_size();
  // Use a dummy data size of 0 to avoid copying overhead.
  // Actual media data is set later.
  const size_t kDummyDataSize = 0;
  std::shared_ptr<MediaSample> stream_sample(
      MediaSample::CopyFrom(media_data, kDummyDataSize, runs_->is_keyframe()));

  if (runs_->is_encrypted()) {
    std::shared_ptr<uint8_t> decrypted_media_data(
        new uint8_t[media_data_size], std::default_delete<uint8_t[]>());
    std::unique_ptr<DecryptConfig> decrypt_config = runs_->GetDecryptConfig();
    if (!decrypt_config) {
      *err = true;
      LOG(ERROR) << "Missing decrypt config.";
      return false;
    }

    if (!decryptor_source_) {
      stream_sample->SetData(media_data, media_data_size);
      // If the demuxer does not have the decryptor_source_, store
      // decrypt_config so that the demuxed sample can be decrypted later.
      stream_sample->set_decrypt_config(std::move(decrypt_config));
      stream_sample->set_is_encrypted(true);
    } else {
      if (!decryptor_source_->DecryptSampleBuffer(decrypt_config.get(),
                                                  media_data, media_data_size,
                                                  decrypted_media_data.get())) {
        *err = true;
        LOG(ERROR) << "Cannot decrypt samples.";
        return false;
      }
      stream_sample->TransferData(std::move(decrypted_media_data),
                                  media_data_size);
    }
  } else {
    stream_sample->SetData(media_data, media_data_size);
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
    if (!BoxReader::StartBox(buf, size, &type, &box_sz, &err))
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
