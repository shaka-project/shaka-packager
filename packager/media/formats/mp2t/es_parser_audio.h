// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PACKAGER_MEDIA_FORMATS_MP2T_ES_PARSER_AUDIO_H_
#define PACKAGER_MEDIA_FORMATS_MP2T_ES_PARSER_AUDIO_H_

#include <cstdint>
#include <functional>
#include <list>
#include <memory>
#include <utility>

#include <packager/macros/classes.h>
#include <packager/media/base/audio_stream_info.h>
#include <packager/media/base/byte_queue.h>
#include <packager/media/formats/mp2t/es_parser.h>
#include <packager/media/formats/mp2t/ts_stream_type.h>
#include <functional>

namespace shaka {
namespace media {
class AudioTimestampHelper;
class BitReader;

namespace mp2t {

class AudioHeader;

class EsParserAudio : public EsParser {
 public:
  EsParserAudio(uint32_t pid,
                TsStreamType stream_type,
                const NewStreamInfoCB& new_stream_info_cb,
                const EmitSampleCB& emit_sample_cb,
                bool sbr_in_mimetype);
  ~EsParserAudio() override;

  // EsParser implementation.
  bool Parse(const uint8_t* buf, int size, int64_t pts, int64_t dts) override;
  bool Flush() override;
  void Reset() override;

 private:
  EsParserAudio(const EsParserAudio&) = delete;
  EsParserAudio& operator=(const EsParserAudio&) = delete;

  // Used to link a PTS with a byte position in the ES stream.
  typedef std::pair<int, int64_t> EsPts;
  typedef std::list<EsPts> EsPtsList;

  // Signal any audio configuration change (if any).
  // Return false if the current audio config is not a supported audio config.
  bool UpdateAudioConfiguration(const AudioHeader& audio_header);

  // Discard some bytes from the ES stream.
  void DiscardEs(int nbytes);

  const TsStreamType stream_type_;
  std::unique_ptr<AudioHeader> audio_header_;

  // Callbacks:
  // - to signal a new audio configuration,
  // - to send ES buffers.
  NewStreamInfoCB new_stream_info_cb_;
  EmitSampleCB emit_sample_cb_;

  // True when AAC SBR extension is signalled in the mimetype
  // (mp4a.40.5 in the codecs parameter).
  bool sbr_in_mimetype_;

  // Bytes of the ES stream that have not been emitted yet.
  ByteQueue es_byte_queue_;

  // List of PTS associated with a position in the ES stream.
  EsPtsList pts_list_;

  // Interpolated PTS for frames that don't have one.
  std::unique_ptr<AudioTimestampHelper> audio_timestamp_helper_;

  std::shared_ptr<StreamInfo> last_audio_decoder_config_;
};

}  // namespace mp2t
}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_FORMATS_MP2T_ES_PARSER_AUDIO_H_
