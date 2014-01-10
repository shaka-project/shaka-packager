
#include "media/event/vod_mpd_notify_muxer_listener.h"

#include <cmath>

#include "base/logging.h"
#include "media/base/audio_stream_info.h"
#include "media/base/video_stream_info.h"
#include "mpd/base/media_info.pb.h"
#include "mpd/base/mpd_notifier.h"

namespace media {
namespace event {

namespace {

using dash_packager::MediaInfo;
using dash_packager::MediaInfo_AudioInfo;
using dash_packager::MediaInfo_VideoInfo;
using dash_packager::Range;

// This will return a positive value, given that |file_size| and
// |duration_seconds| are positive.
uint32 EstimateRequiredBandwidth(uint64 file_size, float duration_seconds) {
  const uint32 file_size_bits = file_size * 8;
  const float bits_per_second = file_size_bits / duration_seconds;

  // Note that casting |bits_per_second| to an integer might make it 0. Take the
  // ceiling and make sure that it returns a positive value.
  return static_cast<uint32>(ceil(bits_per_second));
}

void SetRange(uint32 begin, uint32 end, Range* range) {
  DCHECK(range);
  range->set_begin(begin);
  range->set_end(end);
}

void SetMediaInfoCommonInfo(bool has_init_range,
                            uint32 init_range_start,
                            uint32 init_range_end,
                            bool has_index_range,
                            uint32 index_range_start,
                            uint32 index_range_end,
                            float duration_seconds,
                            uint64 file_size,
                            MediaInfo* media_info) {
  DCHECK(media_info);
  DCHECK_GT(file_size, 0);
  DCHECK_GT(duration_seconds, 0.0f);

  media_info->set_media_duration_seconds(duration_seconds);
  media_info->set_bandwidth(
      EstimateRequiredBandwidth(file_size, duration_seconds));

  if (has_init_range) {
    SetRange(
        init_range_start, init_range_end, media_info->mutable_init_range());
  }

  if (has_index_range) {
    SetRange(
        index_range_start, index_range_end, media_info->mutable_index_range());
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
  // TODO(rkuroiwa): Get frame duration.
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
  audio_info->set_language(audio_stream_info->language());

  const std::vector<uint8>& extra_data = audio_stream_info->extra_data();
  if (!extra_data.empty()) {
    audio_info->set_decoder_config(&extra_data[0], extra_data.size());
  }
}

void SetMediaInfoStreamInfo(const std::list<StreamInfo*>& stream_info_list,
                            MediaInfo* media_info) {
  typedef std::list<StreamInfo*>::const_iterator StreamInfoIterator;
  for (StreamInfoIterator it = stream_info_list.begin();
       it != stream_info_list.end();
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

}  // namespace

VodMpdNotifyMuxerListener::VodMpdNotifyMuxerListener(
    dash_packager::MpdNotifier* mpd_notifier)
    : mpd_notifier_(mpd_notifier) {
  DCHECK(mpd_notifier);
}

VodMpdNotifyMuxerListener::~VodMpdNotifyMuxerListener() {}

void VodMpdNotifyMuxerListener::OnMediaStart(
    const MuxerOptions& muxer_options,
    const std::list<StreamInfo*>& stream_info_list) {}

void VodMpdNotifyMuxerListener::OnMediaEnd(
    const std::list<StreamInfo*>& stream_info_list,
    bool has_init_range,
    uint32 init_range_start,
    uint32 init_range_end,
    bool has_index_range,
    uint32 index_range_start,
    uint32 index_range_end,
    float duration_seconds,
    uint64 file_size) {
  if (file_size == 0) {
    // TODO(rkurowia); bandwidth is a required field for MPD. But without the
    // file size, AFAIK there's not much I can do. Fail silently?
    LOG(ERROR) << "File size not specified";
    return;
  }

  if (duration_seconds <= 0.0f) {
    // Non positive second media must be invalid media.
    LOG(ERROR) << "Duration is not positive: " << duration_seconds;
    return;
  }

  dash_packager::MediaInfo media_info;
  SetMediaInfoCommonInfo(has_init_range, init_range_start, init_range_end,
                         has_index_range, index_range_start, index_range_end,
                         duration_seconds,
                         file_size,
                         &media_info);
  SetMediaInfoStreamInfo(stream_info_list, &media_info);

  uint32 id;  // Result unused.
  mpd_notifier_->NotifyNewContainer(media_info, &id);
}

void VodMpdNotifyMuxerListener::OnNewSegment(uint32 time_scale,
                                             uint64 start_time,
                                             uint64 duration,
                                             uint64 segment_file_size) {
  NOTIMPLEMENTED();
}

}  // namespace event
}  // namespace media
