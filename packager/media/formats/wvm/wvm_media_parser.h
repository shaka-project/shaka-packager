// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Media parser for a Widevine Media Format (WVM) file.

#ifndef PACKAGER_MEDIA_FORMATS_WVM_WVM_MEDIA_PARSER_H_
#define PACKAGER_MEDIA_FORMATS_WVM_WVM_MEDIA_PARSER_H_

#include <cstdint>
#include <deque>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <absl/base/internal/endian.h>

#include <packager/macros/classes.h>
#include <packager/media/base/media_parser.h>
#include <packager/media/codecs/h264_byte_to_unit_stream_converter.h>

namespace shaka {
namespace media {

class AesCbcDecryptor;
class KeySource;
struct EncryptionKey;

namespace wvm {

struct DemuxStreamIdMediaSample {
 public:
  DemuxStreamIdMediaSample();
  ~DemuxStreamIdMediaSample();
  uint32_t demux_stream_id;
  uint32_t parsed_audio_or_video_stream_id;
  std::shared_ptr<MediaSample> media_sample;
};

struct PrevSampleData {
 public:
  PrevSampleData();
  ~PrevSampleData();
  void Reset();
  std::shared_ptr<MediaSample> audio_sample;
  std::shared_ptr<MediaSample> video_sample;
  uint32_t audio_stream_id;
  uint32_t video_stream_id;
  int64_t audio_sample_duration;
  int64_t video_sample_duration;
};

class WvmMediaParser : public MediaParser {
 public:
  WvmMediaParser();
  ~WvmMediaParser() override;

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
  enum Tag {
    CypherVersion = 0,
    TrackOffset = 1,
    TrackSize = 2,
    TrackDuration = 3,
    TrackBitRate = 4,
    TrackTrickPlayFactor = 5,
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
    Video_AVCDecoderConfigurationRecord = 39,
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

  bool ProcessEcm();

  // Index denotes 'search index' in the WVM content.
  bool ParseIndexEntry();

  bool DemuxNextPes(bool is_program_end);

  void StartMediaSampleDemux();

  template <typename T>
  Tag GetTag(const uint8_t& tag,
             const uint32_t& length,
             const uint8_t* start_index,
             T* value) {
    if (length == sizeof(uint8_t)) {
      *value = *start_index;
    } else if (length == sizeof(int8_t)) {
      *value = (int8_t)(*start_index);
    } else if (length == sizeof(uint16_t)) {
      *value = absl::big_endian::Load16(start_index);
    } else if (length == sizeof(int16_t)) {
      *value = (int16_t)(absl::big_endian::Load16(start_index));
    } else if (length == sizeof(uint32_t)) {
      *value = absl::big_endian::Load32(start_index);
    } else if (length == sizeof(int32_t)) {
      *value = (int32_t)(absl::big_endian::Load32(start_index));
    } else if (length == sizeof(uint64_t)) {
      *value = absl::big_endian::Load64(start_index);
    } else if (length == sizeof(int64_t)) {
      *value = (int64_t)(absl::big_endian::Load64(start_index));
    } else {
      *value = 0;
    }
    return Tag(tag);
  }

  // |must_process_encrypted| setting determines if Output() should attempt
  // to ouput media sample as encrypted.
  bool Output(bool must_process_encrypted);

  bool GetAssetKey(const uint8_t* asset_id, EncryptionKey* encryption_key);

  // Callback invoked by the ES media parser
  // to emit a new audio/video access unit.
  bool EmitSample(uint32_t parsed_audio_or_video_stream_id,
                  uint32_t stream_id,
                  const std::shared_ptr<MediaSample>& new_sample,
                  bool isLastSample);

  bool EmitPendingSamples();

  bool EmitLastSample(uint32_t stream_id,
                      const std::shared_ptr<MediaSample>& new_sample);

  // List of callbacks.t
  InitCB init_cb_;
  NewMediaSampleCB new_sample_cb_;

  // Whether |init_cb_| has been invoked.
  bool is_initialized_;
  // Internal content parsing state.
  State parse_state_;

  uint32_t skip_bytes_;
  bool metadata_is_complete_;
  uint8_t current_program_id_;
  uint32_t pes_stream_id_;
  uint32_t prev_pes_stream_id_;
  size_t pes_packet_bytes_;
  uint8_t pes_flags_1_;
  uint8_t pes_flags_2_;
  uint8_t prev_pes_flags_1_;
  size_t pes_header_data_bytes_;
  int64_t timestamp_;
  int64_t pts_;
  uint64_t dts_;
  uint8_t index_program_id_;
  std::shared_ptr<MediaSample> media_sample_;
  size_t crypto_unit_start_pos_;
  PrevSampleData prev_media_sample_data_;
  H264ByteToUnitStreamConverter byte_to_unit_stream_converter_;

  std::vector<uint8_t, std::allocator<uint8_t>> ecm_;
  std::vector<uint8_t> psm_data_;
  std::vector<uint8_t> index_data_;
  std::map<std::string, uint32_t> program_demux_stream_map_;
  int stream_id_count_;
  std::vector<std::shared_ptr<StreamInfo>> stream_infos_;
  std::deque<DemuxStreamIdMediaSample> media_sample_queue_;
  std::vector<uint8_t> sample_data_;
  KeySource* decryption_key_source_;
  std::unique_ptr<AesCbcDecryptor> content_decryptor_;

  DISALLOW_COPY_AND_ASSIGN(WvmMediaParser);
};

}  // namespace wvm
}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_FORMATS_WVM_WVM_MEDIA_PARSER_H_
