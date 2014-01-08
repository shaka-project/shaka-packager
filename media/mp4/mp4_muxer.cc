// Copyright (c) 2013 Google Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mp4/mp4_muxer.h"

#include "base/time/time.h"
#include "media/base/aes_encryptor.h"
#include "media/base/audio_stream_info.h"
#include "media/base/encryptor_source.h"
#include "media/base/media_sample.h"
#include "media/base/media_stream.h"
#include "media/base/video_stream_info.h"
#include "media/file/file.h"
#include "media/mp4/box_definitions.h"
#include "media/mp4/es_descriptor.h"
#include "media/mp4/fourccs.h"
#include "media/mp4/mp4_general_segmenter.h"
#include "media/mp4/mp4_vod_segmenter.h"

namespace {
// The version of cenc implemented here. CENC 4.
const int kCencSchemeVersion = 0x00010000;

// Get time in seconds since midnight, Jan. 1, 1904, in UTC Time.
uint64 IsoTimeNow() {
  // Time in seconds from Jan. 1, 1904 to epoch time, i.e. Jan. 1, 1970.
  const uint64 kIsomTimeOffset = 2082844800l;
  return kIsomTimeOffset + base::Time::Now().ToDoubleT();
}
}  // namespace

namespace media {
namespace mp4 {

MP4Muxer::MP4Muxer(const MuxerOptions& options,
                   EncryptorSource* encryptor_source)
    : Muxer(options, encryptor_source) {}
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

  if (IsEncryptionRequired()) {
    moov->pssh.resize(1);
    GeneratePssh(&moov->pssh[0]);
  }

  if (options().single_segment) {
    segmenter_.reset(
        new MP4VODSegmenter(options(), ftyp.Pass(), moov.Pass()));
  } else {
    segmenter_.reset(
        new MP4GeneralSegmenter(options(), ftyp.Pass(), moov.Pass()));
  }
  return segmenter_->Initialize(encryptor_source(), streams());
}

Status MP4Muxer::Finalize() {
  DCHECK(segmenter_ != NULL);
  return segmenter_->Finalize();
}

Status MP4Muxer::AddSample(const MediaStream* stream,
                           scoped_refptr<MediaSample> sample) {
  DCHECK(segmenter_ != NULL);
  return segmenter_->AddSample(stream, sample);
}

void MP4Muxer::InitializeTrak(const StreamInfo* info, Track* trak) {
  uint64 now = IsoTimeNow();
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

  if (IsEncryptionRequired()) {
    DCHECK(encryptor_source() != NULL);
    // Add a second entry for clear content if needed.
    if (encryptor_source()->clear_milliseconds() > 0)
      sample_description.video_entries.push_back(video);

    VideoSampleEntry& encrypted_video = sample_description.video_entries[0];
    GenerateSinf(&encrypted_video.sinf, encrypted_video.format);
    encrypted_video.format = FOURCC_ENCV;
  }
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

  if (IsEncryptionRequired()) {
    DCHECK(encryptor_source() != NULL);
    // Add a second entry for clear content if needed.
    if (encryptor_source()->clear_milliseconds() > 0)
      sample_description.audio_entries.push_back(audio);

    AudioSampleEntry& encrypted_audio = sample_description.audio_entries[0];
    GenerateSinf(&encrypted_audio.sinf, encrypted_audio.format);
    encrypted_audio.format = FOURCC_ENCA;
  }
}

void MP4Muxer::GeneratePssh(ProtectionSystemSpecificHeader* pssh) {
  DCHECK(encryptor_source() != NULL);
  pssh->system_id = encryptor_source()->key_system_id();
  pssh->data = encryptor_source()->pssh();
}

void MP4Muxer::GenerateSinf(ProtectionSchemeInfo* sinf, FourCC old_type) {
  DCHECK(encryptor_source() != NULL);
  DCHECK(encryptor_source()->encryptor() != NULL);
  sinf->format.format = old_type;
  sinf->type.type = FOURCC_CENC;
  sinf->type.version = kCencSchemeVersion;
  sinf->info.track_encryption.is_encrypted = true;
  sinf->info.track_encryption.default_iv_size =
      encryptor_source()->encryptor()->iv().size();
  sinf->info.track_encryption.default_kid = encryptor_source()->key_id();
}

}  // namespace mp4
}  // namespace media
