// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_MP2T_ES_PARSER_ADTS_H_
#define MEDIA_FORMATS_MP2T_ES_PARSER_ADTS_H_

#include <list>
#include <utility>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/byte_queue.h"
#include "media/formats/mp2t/es_parser.h"

namespace media {
class AudioTimestampHelper;
class BitReader;
class StreamParserBuffer;
}

namespace media {
namespace mp2t {

class EsParserAdts : public EsParser {
 public:
  typedef base::Callback<void(const AudioDecoderConfig&)> NewAudioConfigCB;

  EsParserAdts(const NewAudioConfigCB& new_audio_config_cb,
               const EmitBufferCB& emit_buffer_cb,
               bool sbr_in_mimetype);
  virtual ~EsParserAdts();

  // EsParser implementation.
  virtual bool Parse(const uint8* buf, int size,
                     base::TimeDelta pts,
                     base::TimeDelta dts) OVERRIDE;
  virtual void Flush() OVERRIDE;
  virtual void Reset() OVERRIDE;

 private:
  // Used to link a PTS with a byte position in the ES stream.
  typedef std::pair<int, base::TimeDelta> EsPts;
  typedef std::list<EsPts> EsPtsList;

  // Signal any audio configuration change (if any).
  // Return false if the current audio config is not
  // a supported ADTS audio config.
  bool UpdateAudioConfiguration(const uint8* adts_header);

  // Discard some bytes from the ES stream.
  void DiscardEs(int nbytes);

  // Callbacks:
  // - to signal a new audio configuration,
  // - to send ES buffers.
  NewAudioConfigCB new_audio_config_cb_;
  EmitBufferCB emit_buffer_cb_;

  // True when AAC SBR extension is signalled in the mimetype
  // (mp4a.40.5 in the codecs parameter).
  bool sbr_in_mimetype_;

  // Bytes of the ES stream that have not been emitted yet.
  ByteQueue es_byte_queue_;

  // List of PTS associated with a position in the ES stream.
  EsPtsList pts_list_;

  // Interpolated PTS for frames that don't have one.
  scoped_ptr<AudioTimestampHelper> audio_timestamp_helper_;

  // Last audio config.
  AudioDecoderConfig last_audio_decoder_config_;

  DISALLOW_COPY_AND_ASSIGN(EsParserAdts);
};

}  // namespace mp2t
}  // namespace media

#endif

