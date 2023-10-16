// Copyright 2014 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/media/event/muxer_listener_internal.h>

#include <cmath>

#include <absl/log/check.h>
#include <absl/log/log.h>
#include <absl/strings/escaping.h>
#include <google/protobuf/util/message_differencer.h>

#include <packager/macros/logging.h>
#include <packager/media/base/audio_stream_info.h>
#include <packager/media/base/muxer_options.h>
#include <packager/media/base/protection_system_specific_info.h>
#include <packager/media/base/text_stream_info.h>
#include <packager/media/base/video_stream_info.h>
#include <packager/media/codecs/ac4_audio_util.h>
#include <packager/media/codecs/ec3_audio_util.h>
#include <packager/mpd/base/media_info.pb.h>
#include <packager/utils/bytes_to_string_view.h>

using ::google::protobuf::util::MessageDifferencer;

namespace shaka {
namespace media {
namespace internal {

namespace {

// TODO(rkuroiwa): There is shaka::Range in MediaInfo proto and
// shaka::media::Range in media/base. Find better names.
void SetRange(uint64_t begin, uint64_t end, shaka::Range* range) {
  DCHECK(range);
  range->set_begin(begin);
  range->set_end(end);
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
    case MuxerListener::kContainerPackedAudio:
      media_info->set_container_type(MediaInfo::CONTAINER_PACKED_AUDIO);
      break;
    case MuxerListener::kContainerWebM:
      media_info->set_container_type(MediaInfo::CONTAINER_WEBM);
      break;
    case MuxerListener::kContainerText:
      media_info->set_container_type(MediaInfo::CONTAINER_TEXT);
      break;
    default:
      NOTIMPLEMENTED() << "Unknown container type " << container_type;
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

  const std::vector<uint8_t>& codec_config = video_stream_info->codec_config();
  if (!codec_config.empty()) {
    video_info->set_decoder_config(&codec_config[0], codec_config.size());
  }

  if (video_stream_info->playback_rate() > 0) {
    video_info->set_playback_rate(video_stream_info->playback_rate());
  }
  if (video_stream_info->transfer_characteristics() > 0) {
    video_info->set_transfer_characteristics(
        video_stream_info->transfer_characteristics());
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

  const std::vector<uint8_t>& codec_config = audio_stream_info->codec_config();
  if (!codec_config.empty()) {
    audio_info->set_decoder_config(&codec_config[0], codec_config.size());
  }

  if (audio_stream_info->codec_string() == "ec-3") {
    uint32_t ec3_channel_map;
    if (!CalculateEC3ChannelMap(codec_config, &ec3_channel_map)) {
      LOG(ERROR) << "Failed to calculate EC3 channel map.";
      return;
    }
    auto* codec_data = audio_info->mutable_codec_specific_data();
    codec_data->set_channel_mask(ec3_channel_map);
    uint32_t ec3_channel_mpeg_value;
    if (!CalculateEC3ChannelMPEGValue(codec_config, &ec3_channel_mpeg_value)) {
        LOG(ERROR) << "Failed to calculate EC3 channel configuration "
                   << "descriptor value with MPEG scheme.";
        return;
    }
    codec_data->set_channel_mpeg_value(ec3_channel_mpeg_value);
    uint32_t ec3_joc_complexity = 0;
    if (!GetEc3JocComplexity(codec_config, &ec3_joc_complexity)) {
      LOG(ERROR) << "Failed to obtain DD+JOC Information.";
      return;
    }
    codec_data->set_ec3_joc_complexity(ec3_joc_complexity);
  }

  if (audio_stream_info->codec() == kCodecAC4) {
    uint32_t ac4_channel_mask;
    if (!CalculateAC4ChannelMask(codec_config, &ac4_channel_mask)) {
      LOG(ERROR) << "Failed to calculate AC4 channel mask.";
      return;
    }
    auto* codec_data = audio_info->mutable_codec_specific_data();
    codec_data->set_channel_mask(ac4_channel_mask);
    uint32_t ac4_channel_mpeg_value;
    if (!CalculateAC4ChannelMPEGValue(codec_config, &ac4_channel_mpeg_value)) {
      LOG(ERROR) << "Failed to calculate AC4 channel configuration "
                 << "descriptor value with MPEG scheme.";
      return;
    }
    codec_data->set_channel_mpeg_value(ac4_channel_mpeg_value);
    bool ac4_ims_flag;
    bool ac4_cbi_flag;
    if (!GetAc4ImmersiveInfo(codec_config, &ac4_ims_flag, &ac4_cbi_flag)) {
      LOG(ERROR) << "Failed to obtain AC4 IMS flag and CBI flag.";
      return;
    }
    codec_data->set_ac4_ims_flag(ac4_ims_flag);
    codec_data->set_ac4_cbi_flag(ac4_cbi_flag);
  }
}

void AddTextInfo(const TextStreamInfo& text_stream_info,
                 MediaInfo* media_info) {
  // TODO(modmaker): Set kind.
  MediaInfo::TextInfo* text_info = media_info->mutable_text_info();
  text_info->set_codec(text_stream_info.codec_string());
  text_info->set_language(text_stream_info.language());
}

void SetMediaInfoStreamInfo(const StreamInfo& stream_info,
                            MediaInfo* media_info) {
  if (stream_info.stream_type() == kStreamAudio) {
    AddAudioInfo(static_cast<const AudioStreamInfo*>(&stream_info), media_info);
  } else if (stream_info.stream_type() == kStreamText) {
    AddTextInfo(static_cast<const TextStreamInfo&>(stream_info), media_info);
  } else {
    DCHECK_EQ(stream_info.stream_type(), kStreamVideo);
    AddVideoInfo(static_cast<const VideoStreamInfo*>(&stream_info), media_info);
  }
  if (stream_info.duration() > 0) {
    // |stream_info.duration()| contains the media duration from the original
    // media header, which is usually good enough.
    media_info->set_media_duration_seconds(
        static_cast<double>(stream_info.duration()) / stream_info.time_scale());
  }
}

void SetMediaInfoMuxerOptions(const MuxerOptions& muxer_options,
                              MediaInfo* media_info) {
  DCHECK(media_info);
  if (muxer_options.segment_template.empty()) {
    media_info->set_media_file_name(muxer_options.output_file_name);
  } else {
    if (!muxer_options.output_file_name.empty())
      media_info->set_init_segment_name(muxer_options.output_file_name);
    media_info->set_segment_template(muxer_options.segment_template);
  }
}

// Adjust MediaInfo for compatibility comparison. MediaInfos are considered to
// be compatible if codec and container are the same.
MediaInfo GetCompatibleComparisonMediaInfo(const MediaInfo& media_info) {
  MediaInfo adjusted_media_info;
  adjusted_media_info.set_reference_time_scale(
      media_info.reference_time_scale());
  adjusted_media_info.set_container_type(media_info.container_type());
  if (media_info.has_video_info()) {
    *adjusted_media_info.mutable_video_info() = media_info.video_info();
    adjusted_media_info.mutable_video_info()->clear_frame_duration();
  }
  if (media_info.has_audio_info()) {
    *adjusted_media_info.mutable_audio_info() = media_info.audio_info();
  }
  if (media_info.has_text_info()) {
    *adjusted_media_info.mutable_text_info() = media_info.text_info();
  }
  return adjusted_media_info;
}

}  // namespace

bool GenerateMediaInfo(const MuxerOptions& muxer_options,
                       const StreamInfo& stream_info,
                       int32_t reference_time_scale,
                       MuxerListener::ContainerType container_type,
                       MediaInfo* media_info) {
  DCHECK(media_info);

  SetMediaInfoMuxerOptions(muxer_options, media_info);
  SetMediaInfoStreamInfo(stream_info, media_info);
  SetMediaInfoContainerType(container_type, media_info);
  if (reference_time_scale > 0)
    media_info->set_reference_time_scale(reference_time_scale);
  if (muxer_options.bandwidth > 0)
    media_info->set_bandwidth(muxer_options.bandwidth);

  return true;
}

bool IsMediaInfoCompatible(const MediaInfo& media_info1,
                           const MediaInfo& media_info2) {
  return MessageDifferencer::Equals(
      GetCompatibleComparisonMediaInfo(media_info1),
      GetCompatibleComparisonMediaInfo(media_info2));
}

bool SetVodInformation(const MuxerListener::MediaRanges& media_ranges,
                       float duration_seconds,
                       bool use_segment_lists,
                       MediaInfo* media_info) {
  DCHECK(media_info);

  if (duration_seconds <= 0.0f) {
    // Non positive second media must be invalid media.
    LOG(ERROR) << "Duration is not positive: " << duration_seconds;
    return false;
  }

  if (media_ranges.init_range) {
    SetRange(media_ranges.init_range->start, media_ranges.init_range->end,
             media_info->mutable_init_range());
  }

  if (media_ranges.index_range) {
    SetRange(media_ranges.index_range->start, media_ranges.index_range->end,
             media_info->mutable_index_range());
  }

  if (use_segment_lists) {
    for (const auto& range : media_ranges.subsegment_ranges) {
      SetRange(range.start, range.end, media_info->add_subsegment_ranges());
    }
  }

  media_info->set_media_duration_seconds(duration_seconds);

  return true;
}

void SetContentProtectionFields(
    FourCC protection_scheme,
    const std::vector<uint8_t>& default_key_id,
    const std::vector<ProtectionSystemSpecificInfo>& key_system_info,
    MediaInfo* media_info) {
  DCHECK(media_info);
  MediaInfo::ProtectedContent* protected_content =
      media_info->mutable_protected_content();
  protected_content->set_protection_scheme(FourCCToString(protection_scheme));

  if (!default_key_id.empty()) {
    protected_content->set_default_key_id(default_key_id.data(),
                                          default_key_id.size());
  }

  for (const ProtectionSystemSpecificInfo& info : key_system_info) {
    MediaInfo::ProtectedContent::ContentProtectionEntry* entry =
        protected_content->add_content_protection_entry();
    entry->set_uuid(CreateUUIDString(info.system_id));

    const std::vector<uint8_t>& pssh = info.psshs;
    entry->set_pssh(pssh.data(), pssh.size());
  }
}

std::string CreateUUIDString(const std::vector<uint8_t>& data) {
  DCHECK_EQ(16u, data.size());
  std::string uuid = absl::AsciiStrToLower(
      absl::BytesToHexString(byte_vector_to_string_view(data)));
  uuid.insert(20, "-");
  uuid.insert(16, "-");
  uuid.insert(12, "-");
  uuid.insert(8, "-");
  return uuid;
}

}  // namespace internal
}  // namespace media
}  // namespace shaka
