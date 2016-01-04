// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "packager/media/formats/mp2t/es_parser_adts.h"

#include <stdint.h>

#include <list>

#include "packager/base/logging.h"
#include "packager/base/strings/string_number_conversions.h"
#include "packager/media/base/audio_timestamp_helper.h"
#include "packager/media/base/bit_reader.h"
#include "packager/media/base/media_sample.h"
#include "packager/media/base/timestamp.h"
#include "packager/media/formats/mp2t/adts_header.h"
#include "packager/media/formats/mp2t/mp2t_common.h"
#include "packager/media/formats/mpeg/adts_constants.h"

namespace edash_packager {
namespace media {

// Return true if buf corresponds to an ADTS syncword.
// |buf| size must be at least 2.
static bool isAdtsSyncWord(const uint8_t* buf) {
  return (buf[0] == 0xff) && ((buf[1] & 0xf6) == 0xf0);
}

// Look for an ADTS syncword.
// |new_pos| returns
// - either the byte position of the ADTS frame (if found)
// - or the byte position of 1st byte that was not processed (if not found).
// In every case, the returned value in |new_pos| is such that new_pos >= pos
// |frame_sz| returns the size of the ADTS frame (if found).
// Return whether a syncword was found.
static bool LookForSyncWord(const uint8_t* raw_es,
                            int raw_es_size,
                            int pos,
                            int* new_pos,
                            int* frame_sz) {
  DCHECK_GE(pos, 0);
  DCHECK_LE(pos, raw_es_size);

  int max_offset = raw_es_size - kAdtsHeaderMinSize;
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

    if (!isAdtsSyncWord(cur_buf))
      // The first 12 bits must be 1.
      // The layer field (2 bits) must be set to 0.
      continue;

    int frame_size =
        mp2t::AdtsHeader::GetAdtsFrameSize(cur_buf, kAdtsHeaderMinSize);
    if (frame_size < kAdtsHeaderMinSize) {
      // Too short to be an ADTS frame.
      continue;
    }

    // Check whether there is another frame
    // |size| apart from the current one.
    int remaining_size = raw_es_size - offset;
    if (remaining_size >= frame_size + 2 &&
        !isAdtsSyncWord(&cur_buf[frame_size])) {
      continue;
    }

    *new_pos = offset;
    *frame_sz = frame_size;
    return true;
  }

  *new_pos = max_offset;
  return false;
}

namespace mp2t {

EsParserAdts::EsParserAdts(uint32_t pid,
                           const NewStreamInfoCB& new_stream_info_cb,
                           const EmitSampleCB& emit_sample_cb,
                           bool sbr_in_mimetype)
    : EsParser(pid),
      new_stream_info_cb_(new_stream_info_cb),
      emit_sample_cb_(emit_sample_cb),
      sbr_in_mimetype_(sbr_in_mimetype) {
}

EsParserAdts::~EsParserAdts() {
}

bool EsParserAdts::Parse(const uint8_t* buf,
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
  es_byte_queue_.Push(buf, size);
  es_byte_queue_.Peek(&raw_es, &raw_es_size);

  // Look for every ADTS frame in the ES buffer starting at offset = 0
  int es_position = 0;
  int frame_size;
  while (LookForSyncWord(raw_es, raw_es_size, es_position,
                         &es_position, &frame_size)) {
    const uint8_t* frame_ptr = raw_es + es_position;
    DVLOG(LOG_LEVEL_ES)
        << "ADTS syncword @ pos=" << es_position
        << " frame_size=" << frame_size;
    DVLOG(LOG_LEVEL_ES)
        << "ADTS header: "
        << base::HexEncode(frame_ptr, kAdtsHeaderMinSize);

    // Do not process the frame if this one is a partial frame.
    int remaining_size = raw_es_size - es_position;
    if (frame_size > remaining_size)
      break;

    size_t header_size = AdtsHeader::GetAdtsHeaderSize(frame_ptr, frame_size);

    // Update the audio configuration if needed.
    DCHECK_GE(frame_size, kAdtsHeaderMinSize);
    if (!UpdateAudioConfiguration(frame_ptr, frame_size))
      return false;

    // Get the PTS & the duration of this access unit.
    while (!pts_list_.empty() &&
           pts_list_.front().first <= es_position) {
      audio_timestamp_helper_->SetBaseTimestamp(pts_list_.front().second);
      pts_list_.pop_front();
    }

    int64_t current_pts = audio_timestamp_helper_->GetTimestamp();
    int64_t frame_duration =
        audio_timestamp_helper_->GetFrameDuration(kSamplesPerAACFrame);

    // Emit an audio frame.
    bool is_key_frame = true;

    scoped_refptr<MediaSample> sample =
        MediaSample::CopyFrom(
            frame_ptr + header_size,
            frame_size - header_size,
            is_key_frame);
    sample->set_pts(current_pts);
    sample->set_dts(current_pts);
    sample->set_duration(frame_duration);
    emit_sample_cb_.Run(pid(), sample);

    // Update the PTS of the next frame.
    audio_timestamp_helper_->AddFrames(kSamplesPerAACFrame);

    // Skip the current frame.
    es_position += frame_size;
  }

  // Discard all the bytes that have been processed.
  DiscardEs(es_position);

  return true;
}

void EsParserAdts::Flush() {
}

void EsParserAdts::Reset() {
  es_byte_queue_.Reset();
  pts_list_.clear();
  last_audio_decoder_config_ = scoped_refptr<AudioStreamInfo>();
}

bool EsParserAdts::UpdateAudioConfiguration(const uint8_t* adts_frame,
                                            size_t adts_frame_size) {
  const uint8_t kAacSampleSizeBits(16);

  AdtsHeader adts_header;
  if (!adts_header.Parse(adts_frame, adts_frame_size)) {
    LOG(ERROR) << "Error parsing ADTS frame header.";
    return false;
  }
  std::vector<uint8_t> audio_specific_config;
  if (!adts_header.GetAudioSpecificConfig(&audio_specific_config))
    return false;

  if (last_audio_decoder_config_) {
    // Verify that the audio decoder config has not changed.
    if (last_audio_decoder_config_->extra_data() == audio_specific_config) {
      // Audio configuration has not changed.
      return true;
    }
    NOTIMPLEMENTED() << "Varying audio configurations are not supported.";
    return false;
  }

  // The following code is written according to ISO 14496 Part 3 Table 1.11 and
  // Table 1.22. (Table 1.11 refers to the capping to 48000, Table 1.22 refers
  // to SBR doubling the AAC sample rate.)
  int samples_per_second = adts_header.GetSamplingFrequency();
  int extended_samples_per_second = sbr_in_mimetype_
      ? std::min(2 * samples_per_second, 48000)
      : samples_per_second;

  last_audio_decoder_config_ = scoped_refptr<StreamInfo>(
      new AudioStreamInfo(
          pid(),
          kMpeg2Timescale,
          kInfiniteDuration,
          kCodecAAC,
          AudioStreamInfo::GetCodecString(kCodecAAC,
                                          adts_header.GetObjectType()),
          std::string(),
          kAacSampleSizeBits,
          adts_header.GetNumChannels(),
          extended_samples_per_second,
          0,
          0,
          audio_specific_config.data(),
          audio_specific_config.size(),
          false));

  DVLOG(1) << "Sampling frequency: " << samples_per_second;
  DVLOG(1) << "Extended sampling frequency: " << extended_samples_per_second;
  DVLOG(1) << "Channel config: " << adts_header.GetNumChannels();
  DVLOG(1) << "Object type: " << adts_header.GetObjectType();
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
  new_stream_info_cb_.Run(last_audio_decoder_config_);

  return true;
}

void EsParserAdts::DiscardEs(int nbytes) {
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
}  // namespace edash_packager
