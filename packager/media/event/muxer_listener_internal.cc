// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/event/muxer_listener_internal.h"

#include <math.h>

#include "packager/base/logging.h"
#include "packager/base/strings/string_number_conversions.h"
#include "packager/base/strings/string_util.h"
#include "packager/media/base/audio_stream_info.h"
#include "packager/media/base/muxer_options.h"
#include "packager/media/base/protection_system_specific_info.h"
#include "packager/media/base/video_stream_info.h"
#include "packager/media/filters/ec3_audio_util.h"
#include "packager/mpd/base/media_info.pb.h"

namespace edash_packager {
namespace media {
namespace internal {

namespace {

// This will return a positive value, given that |file_size| and
// |duration_seconds| are positive.
uint32_t EstimateRequiredBandwidth(uint64_t file_size, float duration_seconds) {
  const uint64_t file_size_bits = file_size * 8;
  const float bits_per_second = file_size_bits / duration_seconds;

  // Note that casting |bits_per_second| to an integer might make it 0. Take the
  // ceiling and make sure that it returns a positive value.
  return static_cast<uint32_t>(ceil(bits_per_second));
}

void SetRange(uint64_t begin, uint64_t end, Range* range) {
  DCHECK(range);
  range->set_begin(begin);
  range->set_end(end);
}

void SetMediaInfoRanges(bool has_init_range,
                        uint64_t init_range_start,
                        uint64_t init_range_end,
                        bool has_index_range,
                        uint64_t index_range_start,
                        uint64_t index_range_end,
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
  MediaInfo_VideoInfo* video_info = media_info->mutable_video_info();
  video_info->set_codec(video_stream_info->codec_string());
  video_info->set_width(video_stream_info->width());
  video_info->set_height(video_stream_info->height());
  video_info->set_time_scale(video_stream_info->time_scale());

  if (video_stream_info->pixel_width() > 0)
    video_info->set_pixel_width(video_stream_info->pixel_width());

  if (video_stream_info->pixel_height() > 0)
    video_info->set_pixel_height(video_stream_info->pixel_height());

  const std::vector<uint8_t>& extra_data = video_stream_info->extra_data();
  if (!extra_data.empty()) {
    video_info->set_decoder_config(&extra_data[0], extra_data.size());
  }
}

void AddAudioInfo(const AudioStreamInfo* audio_stream_info,
                  MediaInfo* media_info) {
  DCHECK(audio_stream_info);
  DCHECK(media_info);
  MediaInfo_AudioInfo* audio_info = media_info->mutable_audio_info();
  audio_info->set_codec(audio_stream_info->codec_string());
  audio_info->set_sampling_frequency(audio_stream_info->sampling_frequency());
  audio_info->set_time_scale(audio_stream_info->time_scale());
  audio_info->set_num_channels(audio_stream_info->num_channels());

  const std::string& language = audio_stream_info->language();
  // ISO-639-2/T defines language "und" which we also want to ignore.
  if (!language.empty() && language != "und") {
    audio_info->set_language(language);
  }

  const std::vector<uint8_t>& extra_data = audio_stream_info->extra_data();
  if (!extra_data.empty()) {
    audio_info->set_decoder_config(&extra_data[0], extra_data.size());
  }

  if (audio_stream_info->codec_string() == "ec-3") {
    uint32_t ec3_channel_map;
    if (!CalculateEC3ChannelMap(extra_data, &ec3_channel_map)) {
      LOG(ERROR) << "Failed to calculate EC3 channel map.";
      return;
    }
    audio_info->mutable_codec_specific_data()->set_ec3_channel_map(
        ec3_channel_map);
  }
}

void SetMediaInfoStreamInfo(const StreamInfo& stream_info,
                            MediaInfo* media_info) {
  if (stream_info.stream_type() == kStreamAudio) {
    AddAudioInfo(static_cast<const AudioStreamInfo*>(&stream_info),
                 media_info);
  } else {
    DCHECK_EQ(stream_info.stream_type(), kStreamVideo);
    AddVideoInfo(static_cast<const VideoStreamInfo*>(&stream_info),
                 media_info);
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
                       const StreamInfo& stream_info,
                       uint32_t reference_time_scale,
                       MuxerListener::ContainerType container_type,
                       MediaInfo* media_info) {
  DCHECK(media_info);

  SetMediaInfoMuxerOptions(muxer_options, media_info);
  SetMediaInfoStreamInfo(stream_info, media_info);
  media_info->set_reference_time_scale(reference_time_scale);
  SetMediaInfoContainerType(container_type, media_info);
  if (muxer_options.bandwidth > 0)
    media_info->set_bandwidth(muxer_options.bandwidth);

  return true;
}

bool SetVodInformation(bool has_init_range,
                       uint64_t init_range_start,
                       uint64_t init_range_end,
                       bool has_index_range,
                       uint64_t index_range_start,
                       uint64_t index_range_end,
                       float duration_seconds,
                       uint64_t file_size,
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

  if (!media_info->has_bandwidth()) {
    media_info->set_bandwidth(
        EstimateRequiredBandwidth(file_size, duration_seconds));
  }
  return true;
}

void SetContentProtectionFields(
    const std::string& default_key_id,
    const std::vector<ProtectionSystemSpecificInfo>& key_system_info,
    MediaInfo* media_info) {
  DCHECK(media_info);
  MediaInfo::ProtectedContent* protected_content =
      media_info->mutable_protected_content();

  if (!default_key_id.empty())
    protected_content->set_default_key_id(default_key_id);

  for (const ProtectionSystemSpecificInfo& info : key_system_info) {
    MediaInfo::ProtectedContent::ContentProtectionEntry* entry =
        protected_content->add_content_protection_entry();
    if (!info.system_id().empty())
      entry->set_uuid(CreateUUIDString(info.system_id()));

    const std::vector<uint8_t> pssh = info.CreateBox();
    entry->set_pssh(pssh.data(), pssh.size());
  }
}

std::string CreateUUIDString(const std::vector<uint8_t>& data) {
  DCHECK_EQ(16u, data.size());
  std::string uuid = base::HexEncode(data.data(), data.size());
  base::StringToLowerASCII(&uuid);
  uuid.insert(20, "-");
  uuid.insert(16, "-");
  uuid.insert(12, "-");
  uuid.insert(8, "-");
  return uuid;
}

}  // namespace internal
}  // namespace media
}  // namespace edash_packager
