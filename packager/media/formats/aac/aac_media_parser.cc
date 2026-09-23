// Copyright 2024 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

// @steered SNARE-2 2026-09-22

#include <packager/media/formats/aac/aac_media_parser.h>

#include <vector>

#include <absl/log/log.h>

#include <packager/macros/logging.h>
#include <packager/media/base/audio_stream_info.h>
#include <packager/media/base/audio_timestamp_helper.h>
#include <packager/media/base/media_sample.h>
#include <packager/media/base/timestamp.h>
#include <packager/media/formats/mp2t/adts_header.h>

namespace shaka {
namespace media {
namespace aac {

namespace {
// Raw AAC/ADTS carries a single audio track. The media timescale is set to the
// stream's audio sampling frequency so that each fixed-size AAC frame (1024
// samples) has an exact integer duration and timestamps are computed without
// rounding. Downstream muxers rescale to their own required timescale (e.g. the
// MPEG-2 TS muxer to 90000) as needed.
const uint8_t kAacSampleSizeBits = 16;
}  // namespace

AacMediaParser::AacMediaParser() = default;

AacMediaParser::~AacMediaParser() = default;

void AacMediaParser::Init(const InitCB& init_cb,
                          const NewMediaSampleCB& new_media_sample_cb,
                          const NewTextSampleCB& /*new_text_sample_cb*/,
                          KeySource* /*decryption_key_source*/) {
  DCHECK_EQ(state_, kWaitingForInit);
  DCHECK(init_cb_ == nullptr);
  DCHECK(init_cb != nullptr);
  DCHECK(new_media_sample_cb != nullptr);

  state_ = kParsing;
  init_cb_ = init_cb;
  new_media_sample_cb_ = new_media_sample_cb;
}

bool AacMediaParser::Flush() {
  DVLOG(1) << "AacMediaParser::Flush";
  if (state_ == kError)
    return false;

  // Attempt to consume any complete frames still buffered. Raw ADTS has no
  // trailing framing, so anything left after this is an incomplete frame and
  // is simply dropped.
  if (!ParseInternal())
    return false;

  byte_queue_.Reset();
  timestamp_helper_.reset();
  audio_stream_info_.reset();
  return true;
}

bool AacMediaParser::Parse(const uint8_t* buf, int size) {
  if (state_ == kError)
    return false;

  byte_queue_.Push(buf, size);
  return ParseInternal();
}

bool AacMediaParser::ParseInternal() {
  const uint8_t* data = nullptr;
  int data_size = 0;
  byte_queue_.Peek(&data, &data_size);

  mp2t::AdtsHeader adts_header;
  int offset = 0;
  // Offset at which the current run of skipped (non-frame) bytes began, or -1
  // when we are aligned to a frame. Used to emit a single warning per
  // contiguous run of discarded bytes rather than one per byte.
  int skip_run_start = -1;
  while (offset + static_cast<int>(adts_header.GetMinFrameSize()) <=
         data_size) {
    const uint8_t* frame = data + offset;

    if (!adts_header.IsSyncWord(frame)) {
      // Not aligned to a frame boundary; advance one byte and resync.
      if (skip_run_start < 0)
        skip_run_start = offset;
      ++offset;
      continue;
    }

    const size_t remaining = static_cast<size_t>(data_size - offset);
    const size_t frame_size =
        adts_header.GetFrameSizeWithoutParsing(frame, remaining);
    if (frame_size < adts_header.GetMinFrameSize()) {
      // A syncword with an implausibly small frame size: likely a false
      // positive, skip this byte and continue searching.
      if (skip_run_start < 0)
        skip_run_start = offset;
      ++offset;
      continue;
    }
    if (remaining < frame_size) {
      // Incomplete frame: wait for more data.
      break;
    }

    if (!adts_header.Parse(frame, frame_size)) {
      // Malformed header: this candidate syncword did not parse as a valid
      // ADTS header, so skip a byte and keep searching.
      if (skip_run_start < 0)
        skip_run_start = offset;
      ++offset;
      continue;
    }

    // Corroborate the frame boundary. A genuine ADTS frame is immediately
    // followed by another syncword. If the next syncword is fully buffered but
    // absent, this candidate was a false positive (a random 0xFFF pattern in
    // the payload that happened to parse); skip a byte and keep searching
    // rather than emitting a corrupt sample.
    const int kSyncWordSize = 2;
    if (remaining >= frame_size + kSyncWordSize &&
        !adts_header.IsSyncWord(frame + frame_size)) {
      if (skip_run_start < 0)
        skip_run_start = offset;
      ++offset;
      continue;
    }

    // We have a confirmed frame. If we skipped bytes to get here, warn once for
    // the whole run so a corrupt or misaligned stream is visible without
    // flooding the logs.
    if (skip_run_start >= 0) {
      LOG(WARNING) << "Skipped " << (offset - skip_run_start)
                   << " byte(s) resynchronizing to an ADTS frame at offset "
                   << offset << ".";
      skip_run_start = -1;
    }

    // Publish the stream info from the first valid frame.
    if (!audio_stream_info_) {
      if (!EmitStreamInfo(adts_header)) {
        state_ = kError;
        return false;
      }
    }

    const size_t header_size = adts_header.GetHeaderSize();
    if (frame_size <= header_size) {
      ++offset;
      continue;
    }

    const int64_t pts = timestamp_helper_->GetTimestamp();
    const int64_t duration =
        timestamp_helper_->GetFrameDuration(adts_header.GetSamplesPerFrame());

    std::shared_ptr<MediaSample> sample = MediaSample::CopyFrom(
        frame + header_size, frame_size - header_size, /*is_key_frame=*/true);
    sample->set_pts(pts);
    sample->set_dts(pts);
    sample->set_duration(duration);

    if (!new_media_sample_cb_(kAacTrackId, sample)) {
      LOG(ERROR) << "Failed to process AAC sample.";
      state_ = kError;
      return false;
    }

    timestamp_helper_->AddFrames(adts_header.GetSamplesPerFrame());
    offset += static_cast<int>(frame_size);
  }

  byte_queue_.Pop(offset);
  return true;
}

bool AacMediaParser::EmitStreamInfo(const mp2t::AdtsHeader& adts_header) {
  std::vector<uint8_t> audio_specific_config;
  adts_header.GetAudioSpecificConfig(&audio_specific_config);

  const uint32_t sampling_frequency = adts_header.GetSamplingFrequency();
  if (sampling_frequency == 0) {
    LOG(ERROR) << "Invalid AAC sampling frequency.";
    return false;
  }

  audio_stream_info_ = std::make_shared<AudioStreamInfo>(
      kAacTrackId, sampling_frequency, kInfiniteDuration, kCodecAAC,
      AudioStreamInfo::GetCodecString(kCodecAAC, adts_header.GetObjectType()),
      audio_specific_config.data(), audio_specific_config.size(),
      kAacSampleSizeBits, adts_header.GetNumChannels(), sampling_frequency,
      0 /* seek preroll */, 0 /* codec delay */, 0 /* max bitrate */,
      0 /* avg bitrate */, std::string(), false /* is_encrypted */);

  timestamp_helper_ = std::make_unique<AudioTimestampHelper>(
      sampling_frequency, sampling_frequency);
  timestamp_helper_->SetBaseTimestamp(0);

  std::vector<std::shared_ptr<StreamInfo>> stream_infos;
  stream_infos.push_back(audio_stream_info_);
  init_cb_(stream_infos);

  DVLOG(1) << "AAC stream: sampling_frequency=" << sampling_frequency
           << " channels=" << static_cast<int>(adts_header.GetNumChannels())
           << " object_type=" << static_cast<int>(adts_header.GetObjectType());
  return true;
}

}  // namespace aac
}  // namespace media
}  // namespace shaka
