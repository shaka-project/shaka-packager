// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "media/event/muxer_listener_internal.h"

#include <math.h>

#include "base/logging.h"
#include "media/base/audio_stream_info.h"
#include "media/base/muxer_options.h"
#include "media/base/video_stream_info.h"
#include "mpd/base/media_info.pb.h"

namespace media {
namespace event {
namespace internal {

using dash_packager::MediaInfo;
using dash_packager::MediaInfo_AudioInfo;
using dash_packager::MediaInfo_VideoInfo;
using dash_packager::Range;

namespace {

// This will return a positive value, given that |file_size| and
// |duration_seconds| are positive.
uint32 EstimateRequiredBandwidth(uint64 file_size, float duration_seconds) {
  const uint64 file_size_bits = file_size * 8;
  const float bits_per_second = file_size_bits / duration_seconds;

  // Note that casting |bits_per_second| to an integer might make it 0. Take the
  // ceiling and make sure that it returns a positive value.
  return static_cast<uint32>(ceil(bits_per_second));
}

void SetRange(uint64 begin, uint64 end, Range* range) {
  DCHECK(range);
  range->set_begin(begin);
  range->set_end(end);
}

void SetMediaInfoRanges(bool has_init_range,
                        uint64 init_range_start,
                        uint64 init_range_end,
                        bool has_index_range,
                        uint64 index_range_start,
                        uint64 index_range_end,
                        MediaInfo* media_info) {
  if (has_init_range) {
    SetRange(
        init_range_start, init_range_end, media_info->mutable_init_range());
  }

  if (has_index_range) {
    SetRange(
        index_range_start, index_range_end, media_info->mutable_index_range());
  }
}

void SetMediaInfoContainerType(MuxerListener::ContainerType container_type,
                               MediaInfo* media_info) {
  DCHECK(media_info);
  switch (container_type) {
    case MuxerListener::kContainerUnknown:
      media_info->set_container_type(MediaInfo::CONTAINER_UNKNOWN);
      break;
    case MuxerListener::kContainerMp4:
      media_info->set_container_type(MediaInfo::CONTAINER_MP4);
      break;
    case MuxerListener::kContainerMpeg2ts:
      media_info->set_container_type(MediaInfo::CONTAINER_MPEG2_TS);
      break;
    case MuxerListener::kContainerWebM:
      media_info->set_container_type(MediaInfo::CONTAINER_WEBM);
      break;
    default:
      NOTREACHED() << "Unknown container type " << container_type;
  }
}

void AddVideoInfo(const VideoStreamInfo* video_stream_info,
                  MediaInfo* media_info) {
  DCHECK(video_stream_info);
  DCHECK(media_info);
  MediaInfo_VideoInfo* video_info = media_info->add_video_info();
  video_info->set_codec(video_stream_info->codec_string());
  video_info->set_width(video_stream_info->width());
  video_info->set_height(video_stream_info->height());
  video_info->set_time_scale(video_stream_info->time_scale());

  const std::vector<uint8>& extra_data = video_stream_info->extra_data();
  if (!extra_data.empty()) {
    video_info->set_decoder_config(&extra_data[0], extra_data.size());
  }
}

void AddAudioInfo(const AudioStreamInfo* audio_stream_info,
                  MediaInfo* media_info) {
  DCHECK(audio_stream_info);
  DCHECK(media_info);
  MediaInfo_AudioInfo* audio_info = media_info->add_audio_info();
  audio_info->set_codec(audio_stream_info->codec_string());
  audio_info->set_sampling_frequency(audio_stream_info->sampling_frequency());
  audio_info->set_time_scale(audio_stream_info->time_scale());
  audio_info->set_num_channels(audio_stream_info->num_channels());

  const std::string& language = audio_stream_info->language();
  // ISO-639-2/T defines language "und" which we also want to ignore.
  if (!language.empty() && language != "und") {
    audio_info->set_language(language);
  }

  const std::vector<uint8>& extra_data = audio_stream_info->extra_data();
  if (!extra_data.empty()) {
    audio_info->set_decoder_config(&extra_data[0], extra_data.size());
  }
}

void SetMediaInfoStreamInfo(const std::vector<StreamInfo*>& stream_infos,
                            MediaInfo* media_info) {
  typedef std::vector<StreamInfo*>::const_iterator StreamInfoIterator;
  for (StreamInfoIterator it = stream_infos.begin();
       it != stream_infos.end();
       ++it) {
    const StreamInfo* stream_info = *it;
    if (!stream_info)
      continue;

    if (stream_info->stream_type() == kStreamAudio) {
      AddAudioInfo(static_cast<const AudioStreamInfo*>(stream_info),
                   media_info);
    } else {
      DCHECK_EQ(stream_info->stream_type(), kStreamVideo);
      AddVideoInfo(static_cast<const VideoStreamInfo*>(stream_info),
                   media_info);
    }
  }
}

void SetMediaInfoMuxerOptions(const MuxerOptions& muxer_options,
                              MediaInfo* media_info) {
  DCHECK(media_info);
  if (muxer_options.single_segment) {
    media_info->set_media_file_name(muxer_options.output_file_name);
    DCHECK(muxer_options.segment_template.empty());
  } else {
    media_info->set_init_segment_name(muxer_options.output_file_name);
    media_info->set_segment_template(muxer_options.segment_template);
  }
}

}  // namespace

bool GenerateMediaInfo(const MuxerOptions& muxer_options,
                       const std::vector<StreamInfo*>& stream_infos,
                       uint32 reference_time_scale,
                       MuxerListener::ContainerType container_type,
                       MediaInfo* media_info) {
  DCHECK(media_info);

  SetMediaInfoMuxerOptions(muxer_options, media_info);
  SetMediaInfoStreamInfo(stream_infos, media_info);
  media_info->set_reference_time_scale(reference_time_scale);
  SetMediaInfoContainerType(container_type, media_info);
  return true;
}

bool SetVodInformation(bool has_init_range,
                       uint64 init_range_start,
                       uint64 init_range_end,
                       bool has_index_range,
                       uint64 index_range_start,
                       uint64 index_range_end,
                       float duration_seconds,
                       uint64 file_size,
                       MediaInfo* media_info) {
  DCHECK(media_info);
  if (file_size == 0) {
    LOG(ERROR) << "File size not specified.";
    return false;
  }

  if (duration_seconds <= 0.0f) {
    // Non positive second media must be invalid media.
    LOG(ERROR) << "Duration is not positive: " << duration_seconds;
    return false;
  }

  SetMediaInfoRanges(has_init_range,
                     init_range_start,
                     init_range_end,
                     has_index_range,
                     index_range_start,
                     index_range_end,
                     media_info);

  media_info->set_media_duration_seconds(duration_seconds);
  media_info->set_bandwidth(
      EstimateRequiredBandwidth(file_size, duration_seconds));
  return true;
}

bool AddContentProtectionElements(MuxerListener::ContainerType container_type,
                                  const std::string& user_scheme_id_uri,
                                  MediaInfo* media_info) {
  DCHECK(media_info);

  const char kEncryptedMp4Uri[] = "urn:mpeg:dash:mp4protection:2011";
  const char kEncryptedMp4Value[] = "cenc";

  // DASH MPD spec specifies a default ContentProtection element for ISO BMFF
  // (MP4) files.
  const bool is_mp4_container = container_type == MuxerListener::kContainerMp4;
  if (is_mp4_container) {
    MediaInfo::ContentProtectionXml* mp4_protection =
        media_info->add_content_protections();
    mp4_protection->set_scheme_id_uri(kEncryptedMp4Uri);
    mp4_protection->set_value(kEncryptedMp4Value);
  }

  if (!user_scheme_id_uri.empty()) {
    MediaInfo::ContentProtectionXml* content_protection =
        media_info->add_content_protections();
    content_protection->set_scheme_id_uri(user_scheme_id_uri);
  } else if (is_mp4_container) {
    LOG(WARNING) << "schemeIdUri is not specified. Added default "
                    "ContentProtection only.";
  }

  if (media_info->content_protections_size() == 0) {
    LOG(ERROR) << "The stream is encrypted but no schemeIdUri specified for "
                  "ContentProtection.";
    return false;
  }

  return true;
}

}  // namespace internal
}  // namespace event
}  // namespace media
