// Copyright 2024 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_FORMATS_AAC_AAC_MEDIA_PARSER_H_
#define PACKAGER_MEDIA_FORMATS_AAC_AAC_MEDIA_PARSER_H_

#include <cstdint>
#include <memory>

#include <packager/macros/classes.h>
#include <packager/media/base/byte_queue.h>
#include <packager/media/base/media_parser.h>

namespace shaka {
namespace media {

class AudioStreamInfo;
class AudioTimestampHelper;

namespace mp2t {
class AdtsHeader;
}  // namespace mp2t

namespace aac {

/// Parser for raw AAC elementary streams packaged as a sequence of ADTS
/// frames (a.k.a. "packed audio", see
/// https://tools.ietf.org/html/draft-pantos-http-live-streaming-23#section-3.4).
/// The stream contains a single audio track. Each ADTS frame is emitted as one
/// media sample with the ADTS header stripped, matching the behavior of the
/// MPEG-2 TS elementary stream parser.
class AacMediaParser : public MediaParser {
 public:
  AacMediaParser();
  ~AacMediaParser() override;

  /// @name MediaParser implementation overrides.
  /// @{
  void Init(const InitCB& init_cb,
            const NewMediaSampleCB& new_media_sample_cb,
            const NewTextSampleCB& new_text_sample_cb,
            KeySource* decryption_key_source) override;
  [[nodiscard]] bool Flush() override;
  [[nodiscard]] bool Parse(const uint8_t* buf, int size) override;
  /// @}

 private:
  AacMediaParser(const AacMediaParser&) = delete;
  AacMediaParser& operator=(const AacMediaParser&) = delete;

  enum State {
    kWaitingForInit,
    kParsing,
    kError,
  };

  // Emit as many complete ADTS frames as are currently buffered. Returns false
  // on a fatal parsing error.
  bool ParseInternal();

  // Create/publish the audio stream info from the first ADTS frame parsed.
  bool EmitStreamInfo(const mp2t::AdtsHeader& adts_header);

  State state_ = kWaitingForInit;

  InitCB init_cb_;
  NewMediaSampleCB new_media_sample_cb_;

  // The (single) track id assigned to the AAC stream.
  static constexpr uint32_t kAacTrackId = 0;

  // Buffered, not-yet-consumed elementary stream bytes.
  ByteQueue byte_queue_;

  std::shared_ptr<AudioStreamInfo> audio_stream_info_;
  std::unique_ptr<AudioTimestampHelper> timestamp_helper_;
};

}  // namespace aac
}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_FORMATS_AAC_AAC_MEDIA_PARSER_H_
