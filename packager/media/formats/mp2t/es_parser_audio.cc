// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <packager/media/formats/mp2t/es_parser_audio.h>

#include <algorithm>
#include <cstdint>
#include <list>

#include <absl/log/check.h>
#include <absl/log/log.h>
#include <absl/strings/escaping.h>
#include <absl/strings/numbers.h>

#include <packager/macros/logging.h>
#include <packager/media/base/audio_timestamp_helper.h>
#include <packager/media/base/bit_reader.h>
#include <packager/media/base/media_sample.h>
#include <packager/media/base/timestamp.h>
#include <packager/media/formats/mp2t/ac3_header.h>
#include <packager/media/formats/mp2t/adts_header.h>
#include <packager/media/formats/mp2t/mp2t_common.h>
#include <packager/media/formats/mp2t/mpeg1_header.h>
#include <packager/media/formats/mp2t/ts_stream_type.h>

namespace shaka {
namespace media {
namespace mp2t {

// Look for a syncword.
// |new_pos| returns
// - either the byte position of the frame (if found)
// - or the byte position of 1st byte that was not processed (if not found).
// In every case, the returned value in |new_pos| is such that new_pos >= pos
// |audio_header| is updated with the new audio frame info if a syncword is
// found.
// Return whether a syncword was found.
static bool LookForSyncWord(const uint8_t* raw_es,
                            int raw_es_size,
                            int pos,
                            int* new_pos,
                            AudioHeader* audio_header) {
  DCHECK_GE(pos, 0);
  DCHECK_LE(pos, raw_es_size);

  const int max_offset =
      raw_es_size - static_cast<int>(audio_header->GetMinFrameSize());
  if (pos >= max_offset) {
    // Do not change the position if:
    // - max_offset < 0: not enough bytes to get a full header
    //   Since pos >= 0, this is a subcase of the next condition.
    // - pos >= max_offset: might be the case after reading one full frame,
    //   |pos| is then incremented by the frame size and might then point
    //   to the end of the buffer.
    *new_pos = pos;
    return false;
  }

  for (int offset = pos; offset < max_offset; offset++) {
    const uint8_t* cur_buf = &raw_es[offset];

    if (!audio_header->IsSyncWord(cur_buf))
      continue;

    const size_t remaining_size = static_cast<size_t>(raw_es_size - offset);
    const int kSyncWordSize = 2;
    const size_t frame_size =
        audio_header->GetFrameSizeWithoutParsing(cur_buf, remaining_size);
    if (frame_size < audio_header->GetMinFrameSize())
      // Too short to be a valid frame.
      continue;
    if (remaining_size < frame_size)
      // Not a full frame: will resume when we have more data.
      return false;
    // Check whether there is another frame |size| apart from the current one.
    if (remaining_size >= frame_size + kSyncWordSize &&
        !audio_header->IsSyncWord(&cur_buf[frame_size])) {
      continue;
    }

    if (!audio_header->Parse(cur_buf, frame_size))
      continue;

    *new_pos = offset;
    return true;
  }

  *new_pos = max_offset;
  return false;
}

EsParserAudio::EsParserAudio(uint32_t pid,
                             TsStreamType stream_type,
                             const NewStreamInfoCB& new_stream_info_cb,
                             const EmitSampleCB& emit_sample_cb,
                             bool sbr_in_mimetype)
    : EsParser(pid),
      stream_type_(stream_type),
      new_stream_info_cb_(new_stream_info_cb),
      emit_sample_cb_(emit_sample_cb),
      sbr_in_mimetype_(sbr_in_mimetype) {
  if (stream_type == TsStreamType::kAc3) {
    audio_header_.reset(new Ac3Header);
  } else if (stream_type == TsStreamType::kMpeg1Audio) {
    audio_header_.reset(new Mpeg1Header);
  } else {
    DCHECK_EQ(static_cast<int>(stream_type),
              static_cast<int>(TsStreamType::kAdtsAac));
    audio_header_.reset(new AdtsHeader);
  }
}

EsParserAudio::~EsParserAudio() {}

bool EsParserAudio::Parse(const uint8_t* buf,
                          int size,
                          int64_t pts,
                          int64_t dts) {
  int raw_es_size;
  const uint8_t* raw_es;

  // The incoming PTS applies to the access unit that comes just after
  // the beginning of |buf|.
  if (pts != kNoTimestamp) {
    es_byte_queue_.Peek(&raw_es, &raw_es_size);
    pts_list_.push_back(EsPts(raw_es_size, pts));
  }

  // Copy the input data to the ES buffer.
  es_byte_queue_.Push(buf, static_cast<int>(size));
  es_byte_queue_.Peek(&raw_es, &raw_es_size);

  // Look for every frame in the ES buffer starting at offset = 0
  int es_position = 0;
  while (LookForSyncWord(raw_es, raw_es_size, es_position, &es_position,
                         audio_header_.get())) {
    const uint8_t* frame_ptr = raw_es + es_position;
    DVLOG(LOG_LEVEL_ES) << "syncword @ pos=" << es_position
                        << " frame_size=" << audio_header_->GetFrameSize();
    DVLOG(LOG_LEVEL_ES) << "header: "
                        << absl::BytesToHexString(absl::string_view(
                               reinterpret_cast<const char*>(frame_ptr),
                               audio_header_->GetHeaderSize()));

    // Do not process the frame if this one is a partial frame.
    int remaining_size = raw_es_size - es_position;
    if (static_cast<int>(audio_header_->GetFrameSize()) > remaining_size)
      break;

    // Update the audio configuration if needed.
    if (!UpdateAudioConfiguration(*audio_header_))
      return false;

    // Get the PTS & the duration of this access unit.
    while (!pts_list_.empty() && pts_list_.front().first <= es_position) {
      audio_timestamp_helper_->SetBaseTimestamp(pts_list_.front().second);
      pts_list_.pop_front();
    }

    int64_t current_pts = audio_timestamp_helper_->GetTimestamp();
    int64_t frame_duration = audio_timestamp_helper_->GetFrameDuration(
        audio_header_->GetSamplesPerFrame());

    // Emit an audio frame.
    bool is_key_frame = true;

    std::shared_ptr<MediaSample> sample = MediaSample::CopyFrom(
        frame_ptr + audio_header_->GetHeaderSize(),
        audio_header_->GetFrameSize() - audio_header_->GetHeaderSize(),
        is_key_frame);
    sample->set_pts(current_pts);
    sample->set_dts(current_pts);
    sample->set_duration(frame_duration);
    emit_sample_cb_(sample);

    // Update the PTS of the next frame.
    audio_timestamp_helper_->AddFrames(audio_header_->GetSamplesPerFrame());

    // Skip the current frame.
    es_position += static_cast<int>(audio_header_->GetFrameSize());
  }

  // Discard all the bytes that have been processed.
  DiscardEs(es_position);

  return true;
}

bool EsParserAudio::Flush() {
  return true;
}

void EsParserAudio::Reset() {
  es_byte_queue_.Reset();
  pts_list_.clear();
  last_audio_decoder_config_ = std::shared_ptr<AudioStreamInfo>();
}

bool EsParserAudio::UpdateAudioConfiguration(const AudioHeader& audio_header) {
  const uint8_t kAacSampleSizeBits(16);

  std::vector<uint8_t> audio_specific_config;
  audio_header.GetAudioSpecificConfig(&audio_specific_config);

  if (last_audio_decoder_config_) {
    // Verify that the audio decoder config has not changed.
    if (last_audio_decoder_config_->codec_config() == audio_specific_config) {
      // Audio configuration has not changed.
      return true;
    }
    NOTIMPLEMENTED() << "Varying audio configurations are not supported.";
    return false;
  }

  // The following code is written according to ISO 14496 Part 3 Table 1.11 and
  // Table 1.22. (Table 1.11 refers to the capping to 48000, Table 1.22 refers
  // to SBR doubling the AAC sample rate.)
  int samples_per_second = audio_header.GetSamplingFrequency();
  // TODO(kqyang): Review if it makes sense to have |sbr_in_mimetype_| in
  // es_parser.
  int extended_samples_per_second =
      sbr_in_mimetype_ ? std::min(2 * samples_per_second, 48000)
                       : samples_per_second;

  const Codec codec =
      stream_type_ == TsStreamType::kAc3
          ? kCodecAC3
          : (stream_type_ == TsStreamType::kMpeg1Audio ? kCodecMP3 : kCodecAAC);
  last_audio_decoder_config_ = std::make_shared<AudioStreamInfo>(
      pid(), kMpeg2Timescale, kInfiniteDuration, codec,
      AudioStreamInfo::GetCodecString(codec, audio_header.GetObjectType()),
      audio_specific_config.data(), audio_specific_config.size(),
      kAacSampleSizeBits, audio_header.GetNumChannels(),
      extended_samples_per_second, 0 /* seek preroll */, 0 /* codec delay */,
      0 /* max bitrate */, 0 /* avg bitrate */, std::string(), false);

  DVLOG(1) << "Sampling frequency: " << samples_per_second;
  DVLOG(1) << "Extended sampling frequency: " << extended_samples_per_second;
  DVLOG(1) << "Channel config: "
           << static_cast<int>(audio_header.GetNumChannels());
  DVLOG(1) << "Object type: " << static_cast<int>(audio_header.GetObjectType());
  // Reset the timestamp helper to use a new sampling frequency.
  if (audio_timestamp_helper_) {
    int64_t base_timestamp = audio_timestamp_helper_->GetTimestamp();
    audio_timestamp_helper_.reset(
        new AudioTimestampHelper(kMpeg2Timescale, samples_per_second));
    audio_timestamp_helper_->SetBaseTimestamp(base_timestamp);
  } else {
    audio_timestamp_helper_.reset(
        new AudioTimestampHelper(kMpeg2Timescale, extended_samples_per_second));
  }

  // Audio config notification.
  new_stream_info_cb_(last_audio_decoder_config_);

  return true;
}

void EsParserAudio::DiscardEs(int nbytes) {
  DCHECK_GE(nbytes, 0);
  if (nbytes <= 0)
    return;

  // Adjust the ES position of each PTS.
  for (EsPtsList::iterator it = pts_list_.begin(); it != pts_list_.end(); ++it)
    it->first -= nbytes;

  // Discard |nbytes| of ES.
  es_byte_queue_.Pop(nbytes);
}

}  // namespace mp2t
}  // namespace media
}  // namespace shaka
