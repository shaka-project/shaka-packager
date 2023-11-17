// Copyright 2014 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/media/formats/mp4/mp4_init_muxer.h>

#include <chrono>

#include <absl/log/check.h>
#include <absl/strings/numbers.h>

#include <packager/file.h>
#include <packager/macros/logging.h>
#include <packager/macros/status.h>
#include <packager/media/base/audio_stream_info.h>
#include <packager/media/base/text_stream_info.h>
#include <packager/media/base/video_stream_info.h>
#include <packager/media/codecs/es_descriptor.h>
#include <packager/media/formats/mp4/box_definitions.h>
#include <packager/media/formats/mp4/low_latency_segment_segmenter.h>
#include <packager/media/formats/mp4/multi_segment_segmenter.h>
#include <packager/media/formats/mp4/single_segment_segmenter.h>

namespace shaka {
namespace media {
namespace mp4 {

namespace {

FourCC CodecToFourCC(Codec codec, H26xStreamFormat h26x_stream_format) {
  switch (codec) {
    case kCodecAV1:
      return FOURCC_av01;
    case kCodecH264:
      return h26x_stream_format ==
                     H26xStreamFormat::kNalUnitStreamWithParameterSetNalus
                 ? FOURCC_avc3
                 : FOURCC_avc1;
    case kCodecH265:
      return h26x_stream_format ==
                     H26xStreamFormat::kNalUnitStreamWithParameterSetNalus
                 ? FOURCC_hev1
                 : FOURCC_hvc1;
    case kCodecH265DolbyVision:
      return h26x_stream_format ==
                     H26xStreamFormat::kNalUnitStreamWithParameterSetNalus
                 ? FOURCC_dvhe
                 : FOURCC_dvh1;
    case kCodecVP8:
      return FOURCC_vp08;
    case kCodecVP9:
      return FOURCC_vp09;
    case kCodecAAC:
    case kCodecMP3:
      return FOURCC_mp4a;
    case kCodecAC3:
      return FOURCC_ac_3;
    case kCodecDTSC:
      return FOURCC_dtsc;
    case kCodecDTSH:
      return FOURCC_dtsh;
    case kCodecDTSL:
      return FOURCC_dtsl;
    case kCodecDTSE:
      return FOURCC_dtse;
    case kCodecDTSM:
      return FOURCC_dtsm;
    case kCodecEAC3:
      return FOURCC_ec_3;
    case kCodecAC4:
      return FOURCC_ac_4;
    case kCodecFlac:
      return FOURCC_fLaC;
    case kCodecOpus:
      return FOURCC_Opus;
    case kCodecMha1:
      return FOURCC_mha1;
    case kCodecMhm1:
      return FOURCC_mhm1;
    default:
      return FOURCC_NULL;
  }
}

}  // namespace

MP4InitMuxer::MP4InitMuxer(const MuxerOptions& options) : MP4Muxer(options) {}

MP4InitMuxer::~MP4InitMuxer() {}

Status MP4InitMuxer::Finalize() {
  if (!segmenter_) {
    DCHECK(to_be_initialized_);
    RETURN_IF_ERROR(DelayInitializeMuxer());
    LOG(INFO) << "INIT SEGMENT PROCESSING ONLY";
    LOG(INFO) << "Skip stream '" << options().output_file_name
              << "' which does not contain any sample.";
    return Status::OK;
  }
  return Status::OK;
}

Status MP4InitMuxer::DelayInitializeMuxer() {
  DCHECK(!streams().empty());

  std::unique_ptr<FileType> ftyp(new FileType);
  std::unique_ptr<Movie> moov(new Movie);

  ftyp->major_brand = FOURCC_mp41;
  ftyp->compatible_brands.push_back(FOURCC_iso8);
  ftyp->compatible_brands.push_back(FOURCC_isom);
  ftyp->compatible_brands.push_back(FOURCC_mp41);
  ftyp->compatible_brands.push_back(FOURCC_dash);

  if (streams().size() == 1) {
    FourCC codec_fourcc = FOURCC_NULL;
    if (streams()[0]->stream_type() == kStreamVideo) {
      codec_fourcc =
          CodecToFourCC(streams()[0]->codec(),
                        static_cast<const VideoStreamInfo*>(streams()[0].get())
                            ->h26x_stream_format());
      if (codec_fourcc != FOURCC_NULL)
        ftyp->compatible_brands.push_back(codec_fourcc);

      // https://professional.dolby.com/siteassets/content-creation/dolby-vision-for-content-creators/dolby_vision_bitstreams_within_the_iso_base_media_file_format_dec2017.pdf
      if (streams()[0].get()->codec_string().find("dvh") != std::string::npos)
        ftyp->compatible_brands.push_back(FOURCC_dby1);
    }

    // CMAF allows only one track/stream per file.
    // CMAF requires single initialization switching for AVC3/HEV1, which is not
    // supported yet.
    if (codec_fourcc != FOURCC_avc3 && codec_fourcc != FOURCC_hev1)
      ftyp->compatible_brands.push_back(FOURCC_cmfc);
  }

  moov->header.creation_time = IsoTimeNow();
  moov->header.modification_time = IsoTimeNow();
  moov->header.next_track_id = static_cast<uint32_t>(streams().size()) + 1;

  moov->tracks.resize(streams().size());
  moov->extends.tracks.resize(streams().size());

  // Initialize tracks.
  for (uint32_t i = 0; i < streams().size(); ++i) {
    const StreamInfo* stream = streams()[i].get();
    Track& trak = moov->tracks[i];
    trak.header.track_id = i + 1;

    TrackExtends& trex = moov->extends.tracks[i];
    trex.track_id = trak.header.track_id;
    trex.default_sample_description_index = 1;

    bool generate_trak_result = false;
    switch (stream->stream_type()) {
      case kStreamVideo:
        generate_trak_result = GenerateVideoTrak(
            static_cast<const VideoStreamInfo*>(stream), &trak);
        break;
      case kStreamAudio:
        generate_trak_result = GenerateAudioTrak(
            static_cast<const AudioStreamInfo*>(stream), &trak);
        break;
      case kStreamText:
        generate_trak_result =
            GenerateTextTrak(static_cast<const TextStreamInfo*>(stream), &trak);
        break;
      default:
        NOTIMPLEMENTED() << "Not implemented for stream type: "
                         << stream->stream_type();
    }
    if (!generate_trak_result)
      return Status(error::MUXER_FAILURE, "Failed to generate trak.");

    if (stream->is_encrypted() && options().mp4_params.include_pssh_in_stream) {
      moov->pssh.clear();
      const auto& key_system_info = stream->encryption_config().key_system_info;
      for (const ProtectionSystemSpecificInfo& system : key_system_info) {
        if (system.psshs.empty())
          continue;
        ProtectionSystemSpecificHeader pssh;
        pssh.raw_box = system.psshs;
        moov->pssh.push_back(pssh);
      }
    }
  }

  if (options().segment_template.empty()) {
    segmenter_.reset(new SingleSegmentSegmenter(options(), std::move(ftyp),
                                                std::move(moov)));
  } else if (options().mp4_params.low_latency_dash_mode) {
    segmenter_.reset(new LowLatencySegmentSegmenter(options(), std::move(ftyp),
                                                    std::move(moov)));
  } else {
    segmenter_.reset(
        new MultiSegmentSegmenter(options(), std::move(ftyp), std::move(moov)));
  }

  const Status segmenter_initialized =
      segmenter_->Initialize(streams(), muxer_listener(), progress_listener());
  if (!segmenter_initialized.ok())
    return segmenter_initialized;

  FireOnMediaStartEvent();
  return Status::OK;
}

}  // namespace mp4
}  // namespace media
}  // namespace shaka