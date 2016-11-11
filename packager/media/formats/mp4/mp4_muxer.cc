// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/formats/mp4/mp4_muxer.h"

#include "packager/base/time/clock.h"
#include "packager/base/time/time.h"
#include "packager/media/base/aes_encryptor.h"
#include "packager/media/base/audio_stream_info.h"
#include "packager/media/base/fourccs.h"
#include "packager/media/base/key_source.h"
#include "packager/media/base/media_sample.h"
#include "packager/media/base/media_stream.h"
#include "packager/media/base/video_stream_info.h"
#include "packager/media/codecs/es_descriptor.h"
#include "packager/media/event/muxer_listener.h"
#include "packager/media/file/file.h"
#include "packager/media/formats/mp4/box_definitions.h"
#include "packager/media/formats/mp4/multi_segment_segmenter.h"
#include "packager/media/formats/mp4/single_segment_segmenter.h"

namespace shaka {
namespace media {
namespace mp4 {

namespace {

// Sets the range start and end value from offset and size.
// |start| and |end| are for byte-range-spec specified in RFC2616.
void SetStartAndEndFromOffsetAndSize(size_t offset,
                                     size_t size,
                                     uint32_t* start,
                                     uint32_t* end) {
  DCHECK(start && end);
  *start = static_cast<uint32_t>(offset);
  // Note that ranges are inclusive. So we need - 1.
  *end = *start + static_cast<uint32_t>(size) - 1;
}

FourCC CodecToFourCC(Codec codec) {
  switch (codec) {
    case kCodecH264:
      return FOURCC_avc1;
    case kCodecHEV1:
      return FOURCC_hev1;
    case kCodecHVC1:
      return FOURCC_hvc1;
    case kCodecVP8:
      return FOURCC_vp08;
    case kCodecVP9:
      return FOURCC_vp09;
    case kCodecVP10:
      return FOURCC_vp10;
    case kCodecAAC:
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
    case kCodecOpus:
      return FOURCC_Opus;
    default:
      return FOURCC_NULL;
  }
}

}  // namespace

MP4Muxer::MP4Muxer(const MuxerOptions& options) : Muxer(options) {}
MP4Muxer::~MP4Muxer() {}

Status MP4Muxer::Initialize() {
  DCHECK(!streams().empty());

  std::unique_ptr<FileType> ftyp(new FileType);
  std::unique_ptr<Movie> moov(new Movie);

  ftyp->major_brand = FOURCC_dash;
  ftyp->compatible_brands.push_back(FOURCC_iso6);
  ftyp->compatible_brands.push_back(FOURCC_mp41);
  if (streams().size() == 1 &&
      streams()[0]->info()->stream_type() == kStreamVideo) {
    const FourCC codec_fourcc = CodecToFourCC(
        static_cast<VideoStreamInfo*>(streams()[0]->info().get())->codec());
    if (codec_fourcc != FOURCC_NULL)
      ftyp->compatible_brands.push_back(codec_fourcc);
  }

  moov->header.creation_time = IsoTimeNow();
  moov->header.modification_time = IsoTimeNow();
  moov->header.next_track_id = streams().size() + 1;

  moov->tracks.resize(streams().size());
  moov->extends.tracks.resize(streams().size());

  // Initialize tracks.
  for (uint32_t i = 0; i < streams().size(); ++i) {
    Track& trak = moov->tracks[i];
    trak.header.track_id = i + 1;

    TrackExtends& trex = moov->extends.tracks[i];
    trex.track_id = trak.header.track_id;
    trex.default_sample_description_index = 1;

    switch (streams()[i]->info()->stream_type()) {
      case kStreamVideo:
        GenerateVideoTrak(
            static_cast<VideoStreamInfo*>(streams()[i]->info().get()),
            &trak,
            i + 1);
        break;
      case kStreamAudio:
        GenerateAudioTrak(
            static_cast<AudioStreamInfo*>(streams()[i]->info().get()),
            &trak,
            i + 1);
        break;
      default:
        NOTIMPLEMENTED() << "Not implemented for stream type: "
                         << streams()[i]->info()->stream_type();
    }
  }

  if (options().single_segment) {
    segmenter_.reset(new SingleSegmentSegmenter(options(), std::move(ftyp),
                                                std::move(moov)));
  } else {
    segmenter_.reset(
        new MultiSegmentSegmenter(options(), std::move(ftyp), std::move(moov)));
  }

  const Status segmenter_initialized = segmenter_->Initialize(
      streams(), muxer_listener(), progress_listener(), encryption_key_source(),
      max_sd_pixels(), max_hd_pixels(), max_uhd1_pixels(),
      clear_lead_in_seconds(), crypto_period_duration_in_seconds(),
      protection_scheme());

  if (!segmenter_initialized.ok())
    return segmenter_initialized;

  FireOnMediaStartEvent();
  return Status::OK;
}

Status MP4Muxer::Finalize() {
  DCHECK(segmenter_);
  Status segmenter_finalized = segmenter_->Finalize();

  if (!segmenter_finalized.ok())
    return segmenter_finalized;

  FireOnMediaEndEvent();
  LOG(INFO) << "MP4 file '" << options().output_file_name << "' finalized.";
  return Status::OK;
}

Status MP4Muxer::DoAddSample(const MediaStream* stream,
                             scoped_refptr<MediaSample> sample) {
  DCHECK(segmenter_);
  return segmenter_->AddSample(stream, sample);
}

void MP4Muxer::InitializeTrak(const StreamInfo* info, Track* trak) {
  int64_t now = IsoTimeNow();
  trak->header.creation_time = now;
  trak->header.modification_time = now;
  trak->header.duration = 0;
  trak->media.header.creation_time = now;
  trak->media.header.modification_time = now;
  trak->media.header.timescale = info->time_scale();
  trak->media.header.duration = 0;
  if (!info->language().empty()) {
    // Strip off the subtag, if any.
    std::string main_language = info->language();
    size_t dash = main_language.find('-');
    if (dash != std::string::npos) {
      main_language.erase(dash);
    }

    // ISO-639-2/T main language code should be 3 characters.
    if (main_language.size() != 3) {
      LOG(WARNING) << "'" << main_language << "' is not a valid ISO-639-2 "
                   << "language code, ignoring.";
    } else {
      trak->media.header.language.code = main_language;
    }
  }
}

void MP4Muxer::GenerateVideoTrak(const VideoStreamInfo* video_info,
                                 Track* trak,
                                 uint32_t track_id) {
  InitializeTrak(video_info, trak);

  // width and height specify the track's visual presentation size as
  // fixed-point 16.16 values.
  uint32_t pixel_width = video_info->pixel_width();
  uint32_t pixel_height = video_info->pixel_height();
  if (pixel_width == 0 || pixel_height == 0) {
    LOG(WARNING) << "pixel width/height are not set. Assuming 1:1.";
    pixel_width = 1;
    pixel_height = 1;
  }
  const double sample_aspect_ratio =
      static_cast<double>(pixel_width) / pixel_height;
  trak->header.width = video_info->width() * sample_aspect_ratio * 0x10000;
  trak->header.height = video_info->height() * 0x10000;

  VideoSampleEntry video;
  video.format = CodecToFourCC(video_info->codec());
  video.width = video_info->width();
  video.height = video_info->height();
  video.codec_configuration.data = video_info->codec_config();
  if (pixel_width != 1 || pixel_height != 1) {
    video.pixel_aspect.h_spacing = pixel_width;
    video.pixel_aspect.v_spacing = pixel_height;
  }

  SampleDescription& sample_description =
      trak->media.information.sample_table.description;
  sample_description.type = kVideo;
  sample_description.video_entries.push_back(video);
}

void MP4Muxer::GenerateAudioTrak(const AudioStreamInfo* audio_info,
                                 Track* trak,
                                 uint32_t track_id) {
  InitializeTrak(audio_info, trak);

  trak->header.volume = 0x100;

  AudioSampleEntry audio;
  audio.format = CodecToFourCC(audio_info->codec());
  switch(audio_info->codec()){
    case kCodecAAC:
      audio.esds.es_descriptor.set_object_type(kISO_14496_3);  // MPEG4 AAC.
      audio.esds.es_descriptor.set_esid(track_id);
      audio.esds.es_descriptor.set_decoder_specific_info(
          audio_info->codec_config());
      audio.esds.es_descriptor.set_max_bitrate(audio_info->max_bitrate());
      audio.esds.es_descriptor.set_avg_bitrate(audio_info->avg_bitrate());
      break;
    case kCodecDTSC:
    case kCodecDTSH:
    case kCodecDTSL:
    case kCodecDTSE:
    case kCodecDTSM:
      audio.ddts.extra_data = audio_info->codec_config();
      audio.ddts.max_bitrate = audio_info->max_bitrate();
      audio.ddts.avg_bitrate = audio_info->avg_bitrate();
      audio.ddts.sampling_frequency = audio_info->sampling_frequency();
      audio.ddts.pcm_sample_depth = audio_info->sample_bits();
      break;
    case kCodecAC3:
      audio.dac3.data = audio_info->codec_config();
      break;
    case kCodecEAC3:
      audio.dec3.data = audio_info->codec_config();
      break;
    case kCodecOpus:
      audio.dops.opus_identification_header = audio_info->codec_config();
      break;
    default:
      NOTIMPLEMENTED();
      break;
  }

  audio.channelcount = audio_info->num_channels();
  audio.samplesize = audio_info->sample_bits();
  audio.samplerate = audio_info->sampling_frequency();
  SampleTable& sample_table = trak->media.information.sample_table;
  SampleDescription& sample_description = sample_table.description;
  sample_description.type = kAudio;
  sample_description.audio_entries.push_back(audio);

  // Opus requires at least one sample group description box and at least one
  // sample to group box with grouping type 'roll' within sample table box.
  if (audio_info->codec() == kCodecOpus) {
    sample_table.sample_group_descriptions.resize(1);
    SampleGroupDescription& sample_group_description =
        sample_table.sample_group_descriptions.back();
    sample_group_description.grouping_type = FOURCC_roll;
    sample_group_description.audio_roll_recovery_entries.resize(1);
    // The roll distance is expressed in sample units and always takes negative
    // values.
    const uint64_t kNanosecondsPerSecond = 1000000000ull;
    sample_group_description.audio_roll_recovery_entries[0].roll_distance =
        (0 - (audio_info->seek_preroll_ns() * audio.samplerate +
              kNanosecondsPerSecond / 2)) /
        kNanosecondsPerSecond;

    sample_table.sample_to_groups.resize(1);
    SampleToGroup& sample_to_group = sample_table.sample_to_groups.back();
    sample_to_group.grouping_type = FOURCC_roll;

    sample_to_group.entries.resize(1);
    SampleToGroupEntry& sample_to_group_entry = sample_to_group.entries.back();
    // All samples are in track fragments.
    sample_to_group_entry.sample_count = 0;
    sample_to_group_entry.group_description_index =
        SampleToGroupEntry::kTrackGroupDescriptionIndexBase + 1;
  } else if (audio_info->seek_preroll_ns() != 0) {
    LOG(WARNING) << "Unexpected seek preroll for codec " << audio_info->codec();
    return;
  }
}

bool MP4Muxer::GetInitRangeStartAndEnd(uint32_t* start, uint32_t* end) {
  DCHECK(start && end);
  size_t range_offset = 0;
  size_t range_size = 0;
  const bool has_range = segmenter_->GetInitRange(&range_offset, &range_size);

  if (!has_range)
    return false;

  SetStartAndEndFromOffsetAndSize(range_offset, range_size, start, end);
  return true;
}

bool MP4Muxer::GetIndexRangeStartAndEnd(uint32_t* start, uint32_t* end) {
  DCHECK(start && end);
  size_t range_offset = 0;
  size_t range_size = 0;
  const bool has_range = segmenter_->GetIndexRange(&range_offset, &range_size);

  if (!has_range)
    return false;

  SetStartAndEndFromOffsetAndSize(range_offset, range_size, start, end);
  return true;
}

void MP4Muxer::FireOnMediaStartEvent() {
  if (!muxer_listener())
    return;

  if (streams().size() > 1) {
    LOG(ERROR) << "MuxerListener cannot take more than 1 stream.";
    return;
  }
  DCHECK(!streams().empty()) << "Media started without a stream.";

  const uint32_t timescale = segmenter_->GetReferenceTimeScale();
  muxer_listener()->OnMediaStart(options(),
                                 *streams().front()->info(),
                                 timescale,
                                 MuxerListener::kContainerMp4);
}

void MP4Muxer::FireOnMediaEndEvent() {
  if (!muxer_listener())
    return;

  uint32_t init_range_start = 0;
  uint32_t init_range_end = 0;
  const bool has_init_range =
      GetInitRangeStartAndEnd(&init_range_start, &init_range_end);

  uint32_t index_range_start = 0;
  uint32_t index_range_end = 0;
  const bool has_index_range =
      GetIndexRangeStartAndEnd(&index_range_start, &index_range_end);

  const float duration_seconds = static_cast<float>(segmenter_->GetDuration());

  const int64_t file_size =
      File::GetFileSize(options().output_file_name.c_str());
  if (file_size <= 0) {
    LOG(ERROR) << "Invalid file size: " << file_size;
    return;
  }

  muxer_listener()->OnMediaEnd(has_init_range,
                               init_range_start,
                               init_range_end,
                               has_index_range,
                               index_range_start,
                               index_range_end,
                               duration_seconds,
                               file_size);
}

uint64_t MP4Muxer::IsoTimeNow() {
  // Time in seconds from Jan. 1, 1904 to epoch time, i.e. Jan. 1, 1970.
  const uint64_t kIsomTimeOffset = 2082844800l;
  return kIsomTimeOffset +
         (clock() ? clock()->Now() : base::Time::Now()).ToDoubleT();
}

}  // namespace mp4
}  // namespace media
}  // namespace shaka
