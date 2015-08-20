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
#include "packager/media/base/key_source.h"
#include "packager/media/base/media_sample.h"
#include "packager/media/base/media_stream.h"
#include "packager/media/base/video_stream_info.h"
#include "packager/media/event/muxer_listener.h"
#include "packager/media/file/file.h"
#include "packager/media/formats/mp4/box_definitions.h"
#include "packager/media/formats/mp4/es_descriptor.h"
#include "packager/media/formats/mp4/fourccs.h"
#include "packager/media/formats/mp4/multi_segment_segmenter.h"
#include "packager/media/formats/mp4/single_segment_segmenter.h"

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
}  // namespace

namespace edash_packager {
namespace media {
namespace mp4 {

MP4Muxer::MP4Muxer(const MuxerOptions& options) : Muxer(options) {}
MP4Muxer::~MP4Muxer() {}

Status MP4Muxer::Initialize() {
  DCHECK(!streams().empty());

  scoped_ptr<FileType> ftyp(new FileType);
  scoped_ptr<Movie> moov(new Movie);

  ftyp->major_brand = FOURCC_DASH;
  ftyp->compatible_brands.push_back(FOURCC_ISO6);
  ftyp->compatible_brands.push_back(FOURCC_MP41);
  if (streams().size() == 1 &&
      streams()[0]->info()->stream_type() == kStreamVideo)
    ftyp->compatible_brands.push_back(FOURCC_AVC1);

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
    segmenter_.reset(
        new SingleSegmentSegmenter(options(), ftyp.Pass(), moov.Pass()));
  } else {
    segmenter_.reset(
        new MultiSegmentSegmenter(options(), ftyp.Pass(), moov.Pass()));
  }

  Status segmenter_initialized =
      segmenter_->Initialize(streams(),
                             muxer_listener(),
                             progress_listener(),
                             encryption_key_source(),
                             max_sd_pixels(),
                             clear_lead_in_seconds(),
                             crypto_period_duration_in_seconds());

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
    const size_t language_size = arraysize(trak->media.header.language) - 1;
    if (info->language().size() != language_size) {
      LOG(WARNING) << "'" << info->language() << "' is not a valid ISO-639-2 "
                   << "language code, ignoring.";
    } else {
      memcpy(trak->media.header.language, info->language().c_str(),
             language_size + 1);
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

  trak->media.handler.type = kVideo;

  VideoSampleEntry video;
  video.format = FOURCC_AVC1;
  video.width = video_info->width();
  video.height = video_info->height();
  video.avcc.data = video_info->extra_data();
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
  trak->media.handler.type = kAudio;

  AudioSampleEntry audio;
  audio.format = FOURCC_MP4A;
  audio.channelcount = audio_info->num_channels();
  audio.samplesize = audio_info->sample_bits();
  audio.samplerate = audio_info->sampling_frequency();

  if (kCodecAAC) {
    audio.esds.es_descriptor.set_object_type(kISO_14496_3);  // MPEG4 AAC.
  } else {
    // Do we need to support other types?
    NOTIMPLEMENTED();
  }
  audio.esds.es_descriptor.set_esid(track_id);
  audio.esds.es_descriptor.set_decoder_specific_info(audio_info->extra_data());

  SampleDescription& sample_description =
      trak->media.information.sample_table.description;
  sample_description.type = kAudio;
  sample_description.audio_entries.push_back(audio);
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
}  // namespace edash_packager
