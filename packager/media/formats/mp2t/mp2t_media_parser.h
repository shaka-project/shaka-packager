// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PACKAGER_MEDIA_FORMATS_MP2T_MP2T_MEDIA_PARSER_H_
#define PACKAGER_MEDIA_FORMATS_MP2T_MP2T_MEDIA_PARSER_H_

#include <bitset>
#include <deque>
#include <map>
#include <memory>
#include <unordered_set>

#include "packager/media/base/byte_queue.h"
#include "packager/media/base/media_parser.h"
#include "packager/media/base/stream_info.h"
#include "packager/media/formats/mp2t/ts_stream_type.h"

namespace shaka {
namespace media {

class MediaSample;

namespace mp2t {

class PidState;
class TsPacket;
class TsSection;

class Mp2tMediaParser : public MediaParser {
 public:
  Mp2tMediaParser();
  ~Mp2tMediaParser() override;

  /// @name MediaParser implementation overrides.
  /// @{
  void Init(const InitCB& init_cb,
            const NewMediaSampleCB& new_media_sample_cb,
            const NewTextSampleCB& new_text_sample_cb,
            KeySource* decryption_key_source) override;
  bool Flush() override WARN_UNUSED_RESULT;
  bool Parse(const uint8_t* buf, int size) override WARN_UNUSED_RESULT;
  /// @}

 private:
  // Callback invoked to register a Program Map Table.
  // Note: Does nothing if the PID is already registered.
  void RegisterPmt(int program_number, int pmt_pid);

  // Callback invoked to register a PES pid.
  // Possible values for |media_type| are defined in:
  // ISO-13818.1 / ITU H.222 Table 2.34 "Media type assignments".
  // |pes_pid| is part of the Program Map Table refered by |pmt_pid|.
  void RegisterPes(int pmt_pid,
                   int pes_pid,
                   TsStreamType media_type,
                   const uint8_t* descriptor,
                   size_t descriptor_length);

  // Callback invoked each time the audio/video decoder configuration is
  // changed.
  void OnNewStreamInfo(uint32_t pes_pid,
                       std::shared_ptr<StreamInfo> new_stream_info);

  // Callback invoked by the ES media parser
  // to emit a new audio/video access unit.
  void OnEmitMediaSample(uint32_t pes_pid,
                         std::shared_ptr<MediaSample> new_sample);
  void OnEmitTextSample(uint32_t pes_pid,
                        std::shared_ptr<TextSample> new_sample);

  // Invoke the initialization callback if needed.
  bool FinishInitializationIfNeeded();

  bool EmitRemainingSamples();

  /// Set the value of the "SBR in mime-type" flag which leads to sample rate
  /// doubling. Default value is false.
  void set_sbr_in_mime_type(bool sbr_in_mimetype) {
    sbr_in_mimetype_ = sbr_in_mimetype;
  }

  void update_biggest_pts(int64_t pts);
  std::unordered_set<int> text_pids_;

  // List of callbacks.
  InitCB init_cb_;
  NewMediaSampleCB new_media_sample_cb_;
  NewTextSampleCB new_text_sample_cb_;

  bool sbr_in_mimetype_;

  // Bytes of the TS media.
  ByteQueue ts_byte_queue_;

  // Map of PIDs and their states.  Use an ordered map so manifest generation
  // has a deterministic order.
  std::map<int, std::unique_ptr<PidState>> pids_;

  // Whether |init_cb_| has been invoked.
  bool is_initialized_;

  // A map used to track unsupported stream types and make sure the error is
  // only logged once.
  std::bitset<256> stream_type_logged_once_;

  // PTS used to update text generation if needed
  int64_t biggest_pts_ = 0;

  DISALLOW_COPY_AND_ASSIGN(Mp2tMediaParser);
};

}  // namespace mp2t
}  // namespace media
}  // namespace shaka

#endif
