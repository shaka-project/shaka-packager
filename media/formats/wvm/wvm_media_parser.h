// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Media parser for a Widevine Media Format (WVM) file.

#ifndef MEDIA_FORMATS_WVM_WVM_MEDIA_PARSER_H_
#define MEDIA_FORMATS_WVM_WVM_MEDIA_PARSER_H_

#include <deque>
#include <map>
#include <string>
#include <vector>
#include "base/memory/scoped_ptr.h"
#include "media/base/media_parser.h"
#include "media/base/audio_stream_info.h"
#include "media/base/video_stream_info.h"
#include "media/base/media_sample.h"
#include "media/base/network_util.h"
#include "media/filters/h264_byte_to_unit_stream_converter.h"
#include "openssl/sha.h"

namespace media {
namespace wvm {

struct DemuxStreamIdMediaSample {
  uint32 demux_stream_id;
  uint32 parsed_audio_or_video_stream_id;
  scoped_refptr<MediaSample> media_sample;
};

struct PrevSampleData {
 public:
  PrevSampleData() { Reset(); }
  void Reset() {
    audio_sample = video_sample = NULL;
    audio_stream_id = video_stream_id = 0;
    audio_sample_duration = video_sample_duration = 0;
  }
  scoped_refptr<MediaSample> audio_sample;
  scoped_refptr<MediaSample> video_sample;
  uint32 audio_stream_id;
  uint32 video_stream_id;
  int64 audio_sample_duration;
  int64 video_sample_duration;
};

class WvmMediaParser : public MediaParser {
 public:
  WvmMediaParser();
  virtual ~WvmMediaParser() {}

  // MediaParser implementation overrides.
  virtual void Init(const InitCB& init_cb,
                    const NewSampleCB& new_sample_cb,
                    KeySource* decryption_key_source) OVERRIDE;

  virtual void Flush() OVERRIDE;

  virtual bool Parse(const uint8* buf, int size) OVERRIDE;

 private:
  enum Tag {
    CypherVersion = 0,
    TrackOffset = 1,
    TrackSize = 2,
    TrackDuration = 3,
    TrackBitRate = 4,
    TrackTrickPlayRate = 5,
    TrackAdaptationInterval = 6,
    TrackFlags = 7,
    VideoType = 8,
    VideoProfile = 9,
    VideoLevel = 10,
    VideoWidth = 11,
    VideoHeight = 12,
    VideoTicksPerFrame = 13,
    VideoBitRate = 14,
    AudioType = 15,
    AudioProfile = 16,
    AudioNumChannels = 17,
    AudioSampleFrequency = 18,
    AudioBitRate = 19,
    TrackVersion = 20,
    Title = 21,
    Copyright = 22,
    ChapterIndex = 23,
    TimeIndex = 24,
    Thumbnail = 25,
    ObjectSeqNum = 26,
    ThumbnailOffset = 27,
    ThumbnailSize = 28,
    NumEntries = 29,
    Chapters = 30,
    VideoPixelWidth = 31,
    VideoPixelHeight = 32,
    FileSize = 33,
    SparseDownloadUrl = 34,
    SparseDownloadRangeTranslations = 35,
    SparseDownloadMap = 36,
    AudioSampleSize = 37,
    Audio_EsDescriptor = 38,
    AVCDecoderConfigurationRecord = 39,
    Audio_EC3SpecificData = 40,
    AudioIdentifier = 41,
    VideoStreamId = 42,
    VideoStreamType = 43,
    AudioStreamId = 44,
    AudioStreamType = 45,
    Audio_DtsSpecificData = 46,
    Audio_AC3SpecificData = 47,
    Unset = 48
  };

  enum State {
    StartCode1 = 0,
    StartCode2,
    StartCode3,
    StartCode4,
    PackHeader1,
    PackHeader2,
    PackHeader3,
    PackHeader4,
    PackHeader5,
    PackHeader6,
    PackHeader7,
    PackHeader8,
    PackHeader9,
    PackHeader10,
    PackHeaderStuffingSkip,
    SystemHeader1,
    SystemHeader2,
    SystemHeaderSkip,
    PesStreamId,
    PesPacketLength1,
    PesPacketLength2,
    PesExtension1,
    PesExtension2,
    PesExtension3,
    Pts1,
    Pts2,
    Pts3,
    Pts4,
    Pts5,
    Dts1,
    Dts2,
    Dts3,
    Dts4,
    Dts5,
    PesHeaderData,
    PesPayload,
    EsPayload,
    PsmPayload,
    EcmPayload,
    IndexPayload,
    Padding,
    ProgramEnd
  };

  bool DecryptCBC(void* data, uint32 length, uint32 bytesRemaining,
                  uint32& bytesDecrypted) {
     return(true);
   }

  bool ProcessEcm(void* ecm, uint32 size) {
    return(true);
  }

  // Index denotes 'search index' in the WVM content.
  bool ParseIndexEntry();

  bool DemuxNextPes(uint8* start, bool is_program_end);

  void StartMediaSampleDemux(uint8* start);

  template<typename T>
  Tag GetTag(const uint8& tag, const uint32& length,
             const uint8* start_index, T* value) {
    if (length == sizeof(uint8)) {
      *value = (uint8)(*start_index);
    } else if (length == sizeof(int8)) {
      *value = (int8)(*start_index);
    } else if (length == sizeof(uint16)) {
      *value = (uint16)(ntohsFromBuffer(start_index));
    } else if (length == sizeof(int16)) {
      *value = (int16)(ntohsFromBuffer(start_index));
    } else if (length == sizeof(uint32)) {
      *value = (uint32)(ntohlFromBuffer(start_index));
    } else if (length == sizeof(int32)) {
      *value = (int32)(ntohlFromBuffer(start_index));
    } else if (length == sizeof(uint64)) {
      *value = (uint64)(ntohllFromBuffer(start_index));
    } else if (length == sizeof(int64)) {
      *value = (int64)(ntohllFromBuffer(start_index));
    } else {
      *value = 0;
    }
    return Tag(tag);
  }

  bool Output();

  // Callback invoked by the ES media parser
  // to emit a new audio/video access unit.
  void EmitSample(
      uint32 parsed_audio_or_video_stream_id, uint32 stream_id,
      scoped_refptr<MediaSample>& new_sample, bool isLastSample);

  void EmitPendingSamples();

  bool EmitLastSample(uint32 stream_id, scoped_refptr<MediaSample>& new_sample);

  // List of callbacks.t
  InitCB init_cb_;
  NewSampleCB new_sample_cb_;

  // Whether |init_cb_| has been invoked.
  bool is_initialized_;
  // Internal content parsing state.
  State parse_state_;

  bool is_demuxing_sample_;
  bool is_first_pack_;

  bool is_psm_needed_;
  uint32 skip_bytes_;
  bool metadata_is_complete_;
  uint8 current_program_id_;
  uint32 pes_stream_id_;
  uint32 prev_pes_stream_id_;
  uint16 pes_packet_bytes_;
  uint8 pes_flags_1_;
  uint8 pes_flags_2_;
  uint8 pes_header_data_bytes_;
  uint64 timestamp_;
  uint64 pts_;
  uint64 dts_;
  uint8 index_program_id_;

  SHA256_CTX* sha_context_;
  scoped_refptr<MediaSample> media_sample_;
  PrevSampleData prev_media_sample_data_;

  H264ByteToUnitStreamConverter byte_to_unit_stream_converter_;

  std::vector<uint8, std::allocator<uint8> > ecm_;
  std::vector<uint8> psm_data_;
  std::vector<uint8> index_data_;
  std::map<std::string, uint32> program_demux_stream_map_;
  int stream_id_count_;
  std::vector<scoped_refptr<StreamInfo> > stream_infos_;
  std::deque<DemuxStreamIdMediaSample> media_sample_queue_;
  std::vector<uint8> sample_data_;

  DISALLOW_COPY_AND_ASSIGN(WvmMediaParser);
};

}  // namespace wvm
}  // namespace media

#endif  // MEDIA_FORMATS_WVM_WVM_MEDIA_PARSER_H_
