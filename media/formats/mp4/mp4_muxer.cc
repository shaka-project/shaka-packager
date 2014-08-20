// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "media/formats/mp4/mp4_muxer.h"

#include "base/time/clock.h"
#include "base/time/time.h"
#include "media/base/aes_encryptor.h"
#include "media/base/audio_stream_info.h"
#include "media/base/key_source.h"
#include "media/base/media_sample.h"
#include "media/base/media_stream.h"
#include "media/base/video_stream_info.h"
#include "media/event/muxer_listener.h"
#include "media/file/file.h"
#include "media/formats/mp4/box_definitions.h"
#include "media/formats/mp4/es_descriptor.h"
#include "media/formats/mp4/fourccs.h"
#include "media/formats/mp4/multi_segment_segmenter.h"
#include "media/formats/mp4/single_segment_segmenter.h"

namespace {
// Sets the range start and end value from offset and size.
// |start| and |end| are for byte-range-spec specified in RFC2616.
void SetStartAndEndFromOffsetAndSize(size_t offset,
                                     size_t size,
                                     uint32* start,
                                     uint32* end) {
  DCHECK(start && end);
  *start = static_cast<uint32>(offset);
  // Note that ranges are inclusive. So we need - 1.
  *end = *start + static_cast<uint32>(size) - 1;
}
}  // namespace

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
  for (uint32 i = 0; i < streams().size(); ++i) {
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
  return Status::OK;
}

Status MP4Muxer::DoAddSample(const MediaStream* stream,
                             scoped_refptr<MediaSample> sample) {
  DCHECK(segmenter_);
  return segmenter_->AddSample(stream, sample);
}

void MP4Muxer::InitializeTrak(const StreamInfo* info, Track* trak) {
  int64 now = IsoTimeNow();
  trak->header.creation_time = now;
  trak->header.modification_time = now;
  trak->header.duration = 0;
  trak->media.header.creation_time = now;
  trak->media.header.modification_time = now;
  trak->media.header.timescale = info->time_scale();
  trak->media.header.duration = 0;
  if (!info->language().empty()) {
    DCHECK_EQ(info->language().size(),
              arraysize(trak->media.header.language) - 1);
    strcpy(trak->media.header.language, info->language().c_str());
    trak->media.header.language[info->language().size()] = '\0';
  }
}

void MP4Muxer::GenerateVideoTrak(const VideoStreamInfo* video_info,
                                 Track* trak,
                                 uint32 track_id) {
  InitializeTrak(video_info, trak);

  trak->header.width = video_info->width();
  trak->header.height = video_info->height();
  trak->media.handler.type = kVideo;

  VideoSampleEntry video;
  video.format = FOURCC_AVC1;
  video.width = video_info->width();
  video.height = video_info->height();
  video.avcc.data = video_info->extra_data();

  SampleDescription& sample_description =
      trak->media.information.sample_table.description;
  sample_description.type = kVideo;
  sample_description.video_entries.push_back(video);
}

void MP4Muxer::GenerateAudioTrak(const AudioStreamInfo* audio_info,
                                 Track* trak,
                                 uint32 track_id) {
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

void MP4Muxer::GetStreamInfo(std::vector<StreamInfo*>* stream_infos) {
  DCHECK(stream_infos);
  const std::vector<MediaStream*>& media_stream_vec = streams();
  stream_infos->reserve(media_stream_vec.size());
  for (size_t i = 0; i < media_stream_vec.size(); ++i) {
    stream_infos->push_back(media_stream_vec[i]->info().get());
  }
}

bool MP4Muxer::GetInitRangeStartAndEnd(uint32* start, uint32* end) {
  DCHECK(start && end);
  size_t range_offset = 0;
  size_t range_size = 0;
  const bool has_range = segmenter_->GetInitRange(&range_offset, &range_size);

  if (!has_range)
    return false;

  SetStartAndEndFromOffsetAndSize(range_offset, range_size, start, end);
  return true;
}

bool MP4Muxer::GetIndexRangeStartAndEnd(uint32* start, uint32* end) {
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

  std::vector<StreamInfo*> stream_info_vec;
  GetStreamInfo(&stream_info_vec);
  const uint32 timescale = segmenter_->GetReferenceTimeScale();
  muxer_listener()->OnMediaStart(options(),
                                 stream_info_vec,
                                 timescale,
                                 event::MuxerListener::kContainerMp4,
                                 encryption_key_source());
}

void MP4Muxer::FireOnMediaEndEvent() {
  if (!muxer_listener())
    return;

  uint32 init_range_start = 0;
  uint32 init_range_end = 0;
  const bool has_init_range =
      GetInitRangeStartAndEnd(&init_range_start, &init_range_end);

  uint32 index_range_start = 0;
  uint32 index_range_end = 0;
  const bool has_index_range =
      GetIndexRangeStartAndEnd(&index_range_start, &index_range_end);

  const float duration_seconds = static_cast<float>(segmenter_->GetDuration());

  const int64 file_size = File::GetFileSize(options().output_file_name.c_str());
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

uint64 MP4Muxer::IsoTimeNow() {
  // Time in seconds from Jan. 1, 1904 to epoch time, i.e. Jan. 1, 1970.
  const uint64 kIsomTimeOffset = 2082844800l;
  return kIsomTimeOffset +
         (clock() ? clock()->Now() : base::Time::Now()).ToDoubleT();
}

}  // namespace mp4
}  // namespace media
