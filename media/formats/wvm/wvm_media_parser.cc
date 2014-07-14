// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>
#include <stdlib.h>
#include <map>
#include <sstream>
#include <vector>

#include "base/strings/string_number_conversions.h"
#include "media/formats/mp2t/adts_header.h"
#include "media/formats/wvm/wvm_media_parser.h"


#define HAS_HEADER_EXTENSION(x) ((x != 0xBC) && (x != 0xBE) && (x != 0xBF) \
         && (x != 0xF0) && (x != 0xF2) && (x != 0xF8) \
         && (x != 0xFF))

namespace {
  const uint32 kMpeg2ClockRate = 90000;
  const uint32 kPesOptPts = 0x80;
  const uint32 kPesOptDts = 0x40;
  const uint32 kPesOptAlign = 0x04;
  const uint32 kPsmStreamId = 0xBC;
  const uint32 kPaddingStreamId = 0xBE;
  const uint32 kIndexMagic = 0x49444d69;
  const uint32 kIndexStreamId = 0xBF;  // private_stream_2
  const uint32 kIndexVersion4HeaderSize = 12;
  const uint32 kEcmStreamId = 0xF0;
  const uint32 kV2MetadataStreamId = 0xF1;  // EMM_stream
  const uint32 kScramblingBitsMask = 0x30;
  const uint32 kEncryptedOddKey  = 0x30;
  const uint32 kStartCode1 = 0x00;
  const uint32 kStartCode2 = 0x00;
  const uint32 kStartCode3 = 0x01;
  const uint32 kStartCode4Pack = 0xBA;
  const uint32 kStartCode4System = 0xBB;
  const uint32 kStartCode4ProgramEnd = 0xB9;
  const uint32 kPesStreamIdVideoMask = 0xF0;
  const uint32 kPesStreamIdVideo = 0xE0;
  const uint32 kPesStreamIdAudioMask = 0xE0;
  const uint32 kPesStreamIdAudio = 0xC0;
  const uint32 kVersion4 = 4;
  const int kAdtsHeaderMinSize = 7;
  const uint8 kAacSampleSizeBits = 16;
  // Applies to all video streams.
  const uint8 kNaluLengthSize = 4; // unit is bytes.
  // Placeholder sampling frequency for all audio streams, which
  // will be overwritten after filter parsing.
  const uint32 kDefaultSamplingFrequency = 100;

  enum Type {
    Type_void = 0,
    Type_uint8 = 1,
    Type_int8 = 2,
    Type_uint16 = 3,
    Type_int16 = 4,
    Type_uint32 = 5,
    Type_int32 = 6,
    Type_uint64 = 7,
    Type_int64 = 8,
    Type_string = 9,
    Type_BinaryData = 10
  };
}

namespace media {
namespace wvm {

WvmMediaParser::WvmMediaParser() : is_initialized_(false),
                                   parse_state_(StartCode1),
                                   is_demuxing_sample_(true), // Check this.
                                   is_first_pack_(true),
                                   is_psm_needed_(true),
                                   skip_bytes_(0),
                                   metadata_is_complete_(false),
                                   current_program_id_(0),
                                   pes_stream_id_(0),
                                   prev_pes_stream_id_(0),
                                   pes_packet_bytes_(0),
                                   pes_flags_1_(0),
                                   pes_flags_2_(0),
                                   pes_header_data_bytes_(0),
                                   timestamp_(0),
                                   pts_(0),
                                   dts_(0),
                                   index_program_id_(0),
                                   sha_context_(new SHA256_CTX()),
                                   media_sample_(NULL),
                                   stream_id_count_(0) {
  SHA256_Init(sha_context_);
}

void WvmMediaParser::Init(const InitCB& init_cb,
                          const NewSampleCB& new_sample_cb,
                          KeySource* decryption_key_source) {
  DCHECK(!is_initialized_);
  DCHECK(!init_cb.is_null());
  DCHECK(!new_sample_cb.is_null());

  init_cb_ = init_cb;
  new_sample_cb_ = new_sample_cb;
}

bool WvmMediaParser::Parse(const uint8* buf, int size) {
  uint32 num_bytes, prev_size;
  num_bytes = prev_size = 0;
  uint8* read_ptr = (uint8*)(&buf[0]);
  uint8* end = read_ptr + size;

  while (read_ptr < end) {
    switch(parse_state_) {
      case StartCode1:
        if (*read_ptr == kStartCode1) {
          parse_state_ = StartCode2;
        }
        break;
      case StartCode2:
        if (*read_ptr == kStartCode2) {
          parse_state_ = StartCode3;
        } else {
          parse_state_ = StartCode1;
        }
        break;
      case StartCode3:
        if (*read_ptr == kStartCode3) {
          parse_state_ = StartCode4;
        } else {
          parse_state_ = StartCode1;
        }
        break;
      case StartCode4:
        switch (*read_ptr) {
          case kStartCode4Pack:
            parse_state_ = PackHeader1;
            break;
          case kStartCode4System:
            parse_state_ = SystemHeader1;
            break;
          case kStartCode4ProgramEnd:
            parse_state_ = ProgramEnd;
            continue;
          default:
            parse_state_ = PesStreamId;
            continue;
        }
        break;
      case PackHeader1:
        parse_state_ = PackHeader2;
        break;
      case PackHeader2:
        parse_state_ = PackHeader3;
        break;
      case PackHeader3:
        parse_state_ = PackHeader4;
        break;
      case PackHeader4:
        parse_state_ = PackHeader5;
        break;
      case PackHeader5:
        parse_state_ = PackHeader6;
        break;
      case PackHeader6:
        parse_state_ = PackHeader7;
        break;
      case PackHeader7:
        parse_state_ = PackHeader8;
        break;
      case PackHeader8:
        parse_state_ = PackHeader9;
        break;
      case PackHeader9:
        parse_state_ = PackHeader10;
        break;
      case PackHeader10:
        skip_bytes_ = *read_ptr & 0x07;
        parse_state_ = PackHeaderStuffingSkip;
        break;
      case SystemHeader1:
        skip_bytes_ = *read_ptr;
        skip_bytes_ <<= 8;
        parse_state_ = SystemHeader2;
        break;
      case SystemHeader2:
        skip_bytes_ |= *read_ptr;
        parse_state_ = SystemHeaderSkip;
        break;
      case PackHeaderStuffingSkip:
        if ((end - read_ptr) >= (int32)skip_bytes_) {
          read_ptr += skip_bytes_;
          skip_bytes_ = 0;
          parse_state_ = StartCode1;
        } else {
          skip_bytes_ -= (end - read_ptr);
          read_ptr = end;
        }
        continue;
      case SystemHeaderSkip:
        if ((end - read_ptr) >= (int32)skip_bytes_) {
          read_ptr += skip_bytes_;
          skip_bytes_ = 0;
          parse_state_ = StartCode1;
        } else {
          uint32 remaining_size = end - read_ptr;
          skip_bytes_ -= remaining_size;
          read_ptr = end;
        }
        continue;
      case PesStreamId:
        pes_stream_id_ = *read_ptr;
        if (!metadata_is_complete_ &&
            (pes_stream_id_ != kPsmStreamId) &&
            (pes_stream_id_ != kIndexStreamId) &&
            (pes_stream_id_ != kEcmStreamId) &&
            (pes_stream_id_ != kV2MetadataStreamId) &&
            (pes_stream_id_ != kPaddingStreamId)) {
          metadata_is_complete_ = true;
        }
        parse_state_ = PesPacketLength1;
        break;
      case PesPacketLength1:
        pes_packet_bytes_ = *read_ptr;
        pes_packet_bytes_ <<= 8;
        parse_state_ = PesPacketLength2;
        break;
      case PesPacketLength2:
        pes_packet_bytes_ |= *read_ptr;
        if (HAS_HEADER_EXTENSION(pes_stream_id_)) {
          parse_state_ = PesExtension1;
        } else {
          pes_flags_1_ = pes_flags_2_ = 0;
          pes_header_data_bytes_ = 0;
          parse_state_ = PesPayload;
        }
        break;
      case PesExtension1:
        pes_flags_1_ = *read_ptr;
        // TODO(ramjic): Check if enable_decryption_ is needed.
        *read_ptr &= ~kScramblingBitsMask;
        --pes_packet_bytes_;
        parse_state_ = PesExtension2;
        break;
      case PesExtension2:
        pes_flags_2_ = *read_ptr;
        --pes_packet_bytes_;
        parse_state_ = PesExtension3;
        break;
      case PesExtension3:
        pes_header_data_bytes_ = *read_ptr;
        --pes_packet_bytes_;
        if (pes_flags_2_ & kPesOptPts) {
          parse_state_ = Pts1;
        } else {
          parse_state_ = PesHeaderData;
        }
        break;
      case Pts1:
        timestamp_ = (*read_ptr & 0x0E);
        --pes_header_data_bytes_;
        --pes_packet_bytes_;
        parse_state_ = Pts2;
        break;
      case Pts2:
        timestamp_ <<= 7;
        timestamp_ |= *read_ptr;
        --pes_header_data_bytes_;
        --pes_packet_bytes_;
        parse_state_ = Pts3;
        break;
      case Pts3:
        timestamp_ <<= 7;
        timestamp_ |= *read_ptr >> 1;
        --pes_header_data_bytes_;
        --pes_packet_bytes_;
        parse_state_ = Pts4;
        break;
      case Pts4:
        timestamp_ <<= 8;
        timestamp_ |= *read_ptr;
        --pes_header_data_bytes_;
        --pes_packet_bytes_;
        parse_state_ = Pts5;
        break;
      case Pts5:
        timestamp_ <<= 7;
        timestamp_ |= *read_ptr >> 1;
        pts_ = timestamp_;
        --pes_header_data_bytes_;
        --pes_packet_bytes_;
        if (pes_flags_2_ & kPesOptDts) {
          parse_state_ = Dts1;
        } else {
          dts_ = pts_;
          parse_state_ = PesHeaderData;
        }
        break;
      case Dts1:
        timestamp_ = (*read_ptr & 0x0E);
        --pes_header_data_bytes_;
        --pes_packet_bytes_;
        parse_state_ = Dts2;
        break;
      case Dts2:
        timestamp_ <<= 7;
        timestamp_ |= *read_ptr;
        --pes_header_data_bytes_;
        --pes_packet_bytes_;
        parse_state_ = Dts3;
        break;
      case Dts3:
        timestamp_ <<= 7;
        timestamp_ |= *read_ptr  >> 1;
        --pes_header_data_bytes_;
        --pes_packet_bytes_;
        parse_state_ = Dts4;
        break;
      case Dts4:
        timestamp_ <<= 8;
        timestamp_ |= *read_ptr;
        --pes_header_data_bytes_;
        --pes_packet_bytes_;
        parse_state_ = Dts5;
        break;
      case Dts5:
        timestamp_ <<= 7;
        timestamp_ |= *read_ptr >> 1;
        dts_ = timestamp_;
        --pes_header_data_bytes_;
        --pes_packet_bytes_;
        parse_state_ = PesHeaderData;
        break;
      case PesHeaderData:
        num_bytes = end - read_ptr;
        if (num_bytes >= pes_header_data_bytes_) {
          num_bytes = pes_header_data_bytes_;
          parse_state_ = PesPayload;
        }
        pes_header_data_bytes_ -= num_bytes;
        pes_packet_bytes_ -= num_bytes;
        read_ptr += num_bytes;
        continue;
      case PesPayload:
        switch (pes_stream_id_) {
          case kPsmStreamId:
            psm_data_.clear();
            parse_state_ = PsmPayload;
            continue;
          case kPaddingStreamId:
            parse_state_ = Padding;
            continue;
          case kEcmStreamId:
            ecm_.clear();
            parse_state_ = EcmPayload;
            continue;
          case kIndexStreamId:
            parse_state_ = IndexPayload;
            continue;
          default:
            if (!DemuxNextPes(read_ptr, false)) {
              return false;
            }
            parse_state_ = EsPayload;
        }
        continue;
      case PsmPayload:
        num_bytes = end - read_ptr;
        if (num_bytes >= pes_packet_bytes_) {
          num_bytes = pes_packet_bytes_;
          parse_state_ = StartCode1;
        }
        if (num_bytes > 0) {
          pes_packet_bytes_ -= num_bytes;
          prev_size = psm_data_.size();
          psm_data_.resize(prev_size + num_bytes);
          memcpy(&psm_data_[prev_size], read_ptr, num_bytes);
        }
        read_ptr += num_bytes;
        continue;
      case EcmPayload:
        num_bytes = end - read_ptr;
        if (num_bytes >= pes_packet_bytes_) {
          num_bytes = pes_packet_bytes_;
          parse_state_ = StartCode1;
        }
        if (num_bytes > 0) {
          pes_packet_bytes_ -= num_bytes;
          prev_size = ecm_.size();
          ecm_.resize(prev_size + num_bytes);
          memcpy(&ecm_[prev_size], read_ptr, num_bytes);
        }
        if ((pes_packet_bytes_ == 0) && !ecm_.empty()) {
          if (!ProcessEcm(&ecm_[0], ecm_.size())) {
            return(false);
          }
        }
        read_ptr += num_bytes;
        continue;
      case IndexPayload:
        num_bytes = end - read_ptr;
        if (num_bytes >= pes_packet_bytes_) {
          num_bytes = pes_packet_bytes_;
          parse_state_ = StartCode1;
        }
        if (num_bytes > 0) {
          pes_packet_bytes_ -= num_bytes;
          prev_size = index_data_.size();
          index_data_.resize(prev_size + num_bytes);
          memcpy(&index_data_[prev_size], read_ptr, num_bytes);
        }
        if (pes_packet_bytes_ == 0 && !index_data_.empty()) {
          if (!metadata_is_complete_) {
            if (!ParseIndexEntry()) {
              return false;
            }
            index_program_id_++;
            index_data_.clear();
          }
        }
        read_ptr += num_bytes;
        continue;
      case EsPayload:
        num_bytes = end - read_ptr;
        if (num_bytes >= pes_packet_bytes_) {
          num_bytes = pes_packet_bytes_;
          parse_state_ = StartCode1;
        }
        pes_packet_bytes_ -= num_bytes;
        if (pes_stream_id_ !=  kV2MetadataStreamId) {
          sample_data_.resize(sample_data_.size() + num_bytes);
          memcpy(&sample_data_[sample_data_.size() - num_bytes], read_ptr,
                 num_bytes);
        }
        prev_pes_stream_id_ = pes_stream_id_;
        read_ptr += num_bytes;
        continue;
      case Padding:
        num_bytes = end - read_ptr;
        if (num_bytes >= pes_packet_bytes_) {
          num_bytes = pes_packet_bytes_;
          parse_state_ = StartCode1;
        }
        pes_packet_bytes_ -= num_bytes;
        read_ptr += num_bytes;
        continue;
      case ProgramEnd:
        parse_state_ = StartCode1;
        metadata_is_complete_ = true;
        if (!DemuxNextPes(read_ptr, true)) {
          return false;
        }
        Flush();
        // Reset.
        dts_ = pts_ = 0;
        parse_state_ = StartCode1;
        prev_media_sample_data_.Reset();
        current_program_id_++;
        break;
      default:
        break;
    }
    ++read_ptr;
  }
  return true;
}

bool WvmMediaParser::EmitLastSample(uint32 stream_id,
                                    scoped_refptr<MediaSample>& new_sample) {
  std::string key =  base::UintToString(current_program_id_).append(":")
      .append(base::UintToString(stream_id));
  std::map<std::string, uint32>::iterator it =
      program_demux_stream_map_.find(key);
  if (it != program_demux_stream_map_.end()) {
      EmitSample(stream_id, (*it).second, new_sample, true);
  } else {
    return false;
  }
  return true;
}

void WvmMediaParser::EmitPendingSamples() {
  // Emit queued samples which were built when not initialized.
  while (!media_sample_queue_.empty()) {
    DemuxStreamIdMediaSample& demux_stream_media_sample =
        media_sample_queue_.front();
    EmitSample(
        demux_stream_media_sample.parsed_audio_or_video_stream_id,
        demux_stream_media_sample.demux_stream_id,
        demux_stream_media_sample.media_sample, false);
    media_sample_queue_.pop_front();
  }
}

void WvmMediaParser::Flush() {
  // Flush the last audio and video sample for current program.
  // Reset the streamID when successfully emitted.
  if (prev_media_sample_data_.audio_sample != NULL) {
    if (!EmitLastSample(prev_pes_stream_id_,
                        prev_media_sample_data_.audio_sample)) {
      LOG(ERROR) << "Did not emit last sample for audio stream with ID = "
                 << prev_pes_stream_id_;
    }
  }
  if (prev_media_sample_data_.video_sample != NULL) {
    if (!EmitLastSample(prev_pes_stream_id_,
                        prev_media_sample_data_.video_sample)) {
      LOG(ERROR) << "Did not emit last sample for video stream with ID = "
                 << prev_pes_stream_id_;
    }
  }
}

bool WvmMediaParser::ParseIndexEntry() {
  // Do not parse index entry at the beginning of any track *after* the first
  // track.
  if (current_program_id_ > 0) {
    return true;
  }
  uint32 index_size = 0;
  if (index_data_.size() < kIndexVersion4HeaderSize) {
    return false;
  }
  if (sha_context_ != NULL) {
    if (SHA256_Update(sha_context_, &index_data_[0], index_data_.size()) != 1) {
      return false;
    }
  }

  const uint8* read_ptr_index = &index_data_[0];
  if (ntohlFromBuffer(read_ptr_index) != kIndexMagic) {
    index_data_.clear();
    return false;
  }
  read_ptr_index += 4;

  uint32 version = ntohlFromBuffer(read_ptr_index);
  read_ptr_index += 4;
  if (version == kVersion4) {
    index_size = kIndexVersion4HeaderSize + ntohlFromBuffer(read_ptr_index);
    if (index_data_.size() < index_size) {
      return false;
    }
    read_ptr_index += sizeof(uint32);

    // Index metadata
    uint32 index_metadata_max_size = index_size - kIndexVersion4HeaderSize;
    if (index_metadata_max_size < sizeof(uint8)) {
      index_data_.clear();
      return false;
    }

    uint64 track_duration = 0;
    uint32 sampling_frequency = kDefaultSamplingFrequency;
    uint32 time_scale = kMpeg2ClockRate;
    uint16 video_width = 0;
    uint16 video_height = 0;
    uint8 nalu_length_size = kNaluLengthSize;
    uint8 num_channels = 0;
    int audio_pes_stream_id = 0;
    int video_pes_stream_id = 0;
    bool has_video = false;
    bool has_audio = false;
    std::vector<uint8> decoder_config_record;
    std::string video_codec_string;
    std::string audio_codec_string;
    uint8 num_index_entries = *read_ptr_index;
    ++read_ptr_index;
    --index_metadata_max_size;

   for (uint8 idx = 0; idx < num_index_entries; ++idx) {
     if (index_metadata_max_size < (2 * sizeof(uint8)) + sizeof(uint32)) {
       return false;
     }
     uint8 tag = *read_ptr_index;
     ++read_ptr_index;
     uint8 type = *read_ptr_index;
     ++read_ptr_index;
     uint32 length = ntohlFromBuffer(read_ptr_index);
     read_ptr_index += sizeof(uint32);
     index_metadata_max_size -= (2 * sizeof(uint8)) + sizeof(uint32);
     if (index_metadata_max_size < length) {
        return false;
     }
     int value = 0;
     Tag tagtype = Unset;
     std::vector<uint8> binary_data(length);
     switch (Type(type)) {
       case Type_uint8:
         if (length == sizeof(uint8)) {
          tagtype = GetTag(tag, length, read_ptr_index, &value);
        } else {
          return false;
        }
      break;
       case Type_int8:
         if (length == sizeof(int8)) {
           tagtype = GetTag(tag, length, read_ptr_index, &value);
         } else {
           return false;
         }
         break;
       case Type_uint16:
         if (length == sizeof(uint16)) {
           tagtype = GetTag(tag, length, read_ptr_index, &value);
         } else {
           return false;
         }
         break;
       case Type_int16:
         if (length == sizeof(int16)) {
           tagtype = GetTag(tag, length, read_ptr_index, &value);
         } else {
           return false;
         }
         break;
       case Type_uint32:
         if (length == sizeof(uint32)) {
           tagtype = GetTag(tag, length, read_ptr_index, &value);
         } else {
           return false;
         }
         break;
       case Type_int32:
         if (length == sizeof(int32)) {
           tagtype = GetTag(tag, length, read_ptr_index, &value);
         } else {
           return false;
         }
         break;
       case Type_uint64:
         if (length == sizeof(uint64)) {
           tagtype = GetTag(tag, length, read_ptr_index, &value);
         } else {
           return false;
         }
         break;
       case Type_int64:
         if (length == sizeof(int64)) {
           tagtype = GetTag(tag, length, read_ptr_index, &value);
         } else {
           return false;
         }
         break;
       case Type_string:
       case Type_BinaryData:
         memcpy(&binary_data[0], read_ptr_index, length);
         tagtype = Tag(tag);
         break;
       default:
         break;
     }

     switch (tagtype) {
       case TrackDuration:
         track_duration = value;
         break;
       case VideoStreamId:
         video_pes_stream_id = value;
         break;
       case AudioStreamId:
         audio_pes_stream_id = value;
         break;
       case VideoWidth:
         video_width = (uint16)value;
         break;
       case VideoHeight:
         video_height = (uint16)value;
         break;
       case AudioNumChannels:
         num_channels = (uint8)value;
         break;
       case VideoType:
         has_video = true;
         break;
       case AudioType:
         has_audio = true;
         break;
       default:
         break;
     }

     read_ptr_index += length;
     index_metadata_max_size -= length;
   }
   // End Index metadata
   index_size = read_ptr_index - &index_data_[0];

   // Extra data for both audio and video streams not set here, but in
   // Output().
   if (has_video) {
     VideoCodec video_codec = kCodecH264;
     stream_infos_.push_back(new VideoStreamInfo(
         stream_id_count_, time_scale, track_duration, video_codec,
         video_codec_string, std::string(), video_width, video_height,
         nalu_length_size, NULL, 0, true));
     program_demux_stream_map_[base::UintToString(index_program_id_)
                               + ":"
                               + base::UintToString(video_pes_stream_id)]
                               = stream_id_count_++;
   }
   if (has_audio) {
     AudioCodec audio_codec = kCodecAAC;
     stream_infos_.push_back(new AudioStreamInfo(
         stream_id_count_, time_scale, track_duration, audio_codec,
         audio_codec_string, std::string(), kAacSampleSizeBits, num_channels,
         sampling_frequency, NULL, 0, true));
     program_demux_stream_map_[base::UintToString(index_program_id_)
                               + ":"
                               + base::UintToString(audio_pes_stream_id)]
                               = stream_id_count_++;
   }
 }
  return true;
}

bool WvmMediaParser::DemuxNextPes(uint8* read_ptr, bool is_program_end) {
  // Demux media sample if we are at program end or if we are not at a
  // continuation PES.
  if (is_program_end || (pes_flags_2_ & kPesOptPts)) {
    if (!sample_data_.empty()) {
      if (!Output()) {
        return false;
      }
    }
    StartMediaSampleDemux(read_ptr);
  }
  return true;
}

void WvmMediaParser::StartMediaSampleDemux(uint8* read_ptr) {
  bool is_key_frame = ((pes_flags_1_ & kPesOptAlign) != 0);
  media_sample_ = MediaSample::CreateEmptyMediaSample();
  media_sample_->set_dts(dts_);
  media_sample_->set_pts(pts_);
  media_sample_->set_is_key_frame(is_key_frame);

  sample_data_.clear();
}

bool WvmMediaParser::Output() {
  if ((prev_pes_stream_id_ & kPesStreamIdVideoMask) == kPesStreamIdVideo) {
    // Set data on the video stream from the NalUnitStream.
    std::vector<uint8> nal_unit_stream;
    byte_to_unit_stream_converter_.ConvertByteStreamToNalUnitStream(
        &sample_data_[0], sample_data_.size(), &nal_unit_stream);
    media_sample_->set_data(nal_unit_stream.data(), nal_unit_stream.size());
    if (!is_initialized_) {
      // Set extra data for video stream from AVC Decoder Config Record.
      // Also, set codec string from the AVC Decoder Config Record.
      std::vector<uint8> decoder_config_record;
      byte_to_unit_stream_converter_.GetAVCDecoderConfigurationRecord(
          &decoder_config_record);
      for (uint32 i = 0; i < stream_infos_.size(); i++) {
        if (stream_infos_[i]->stream_type() == media::kStreamVideo &&
           stream_infos_[i]->extra_data().empty()) {
            stream_infos_[i]->set_extra_data(decoder_config_record);
            stream_infos_[i]->set_codec_string(VideoStreamInfo::GetCodecString(
                kCodecH264, decoder_config_record[1], decoder_config_record[2],
                decoder_config_record[3]));
        }
      }
    }
  } else if ((prev_pes_stream_id_ & kPesStreamIdAudioMask) ==
      kPesStreamIdAudio) {
      // Set data on the audio stream from AdtsHeader.
      int frame_size =
          media::mp2t::AdtsHeader::GetAdtsFrameSize(&sample_data_[0],
                                                  kAdtsHeaderMinSize);
      media::mp2t::AdtsHeader adts_header;
      const uint8* frame_ptr = &sample_data_[0];
      std::vector<uint8> extra_data;
      if (adts_header.Parse(frame_ptr, frame_size) &&
         (adts_header.GetAudioSpecificConfig(&extra_data))) {
        size_t header_size = adts_header.GetAdtsHeaderSize(frame_ptr,
                                                           frame_size);
        media_sample_->set_data(frame_ptr + header_size,
                                frame_size - header_size);
        if (!is_initialized_) {
          uint32 sampling_frequency = adts_header.GetSamplingFrequency();
          for (uint32 i = 0; i < stream_infos_.size(); i++) {
            AudioStreamInfo* audio_stream_info =
                reinterpret_cast<AudioStreamInfo*>(
                    stream_infos_[i].get());
            audio_stream_info->set_sampling_frequency(sampling_frequency);
            // Set extra data and codec string on the audio stream from the
            // AdtsHeader.
            if (stream_infos_[i]->stream_type() == media::kStreamAudio &&
                stream_infos_[i]->extra_data().empty()) {
              stream_infos_[i]->set_extra_data(extra_data);
              stream_infos_[i]->set_codec_string(
                  AudioStreamInfo::GetCodecString(
                      kCodecAAC, adts_header.GetObjectType()));
            }
          }
        }
      }
    }

  if (!is_initialized_) {
    bool is_extra_data_in_stream_infos = true;
    // Check if all collected stream infos have extra_data set.
    for (uint32 i = 0; i < stream_infos_.size(); i++) {
      if (stream_infos_[i]->extra_data().empty()) {
        is_extra_data_in_stream_infos = false;
        break;
      }
    }
    if (is_extra_data_in_stream_infos) {
      init_cb_.Run(stream_infos_);
      is_initialized_ = true;
    }
  }

  DCHECK_GT(media_sample_->data_size(), 0UL);
  std::string key =  base::UintToString(current_program_id_).append(":")
      .append(base::UintToString(prev_pes_stream_id_));
  std::map<std::string, uint32>::iterator it =
      program_demux_stream_map_.find(key);
  if (it == program_demux_stream_map_.end()) {
    // TODO(ramjic): Log error message here and in other error cases through
    // this method.
    return false;
  }
  DemuxStreamIdMediaSample demux_stream_media_sample;
  demux_stream_media_sample.parsed_audio_or_video_stream_id =
      prev_pes_stream_id_;
  demux_stream_media_sample.demux_stream_id = (*it).second;
  demux_stream_media_sample.media_sample = media_sample_;
  // Check if sample can be emitted.
  if (!is_initialized_) {
    media_sample_queue_.push_back(demux_stream_media_sample);
  } else {
    // flush the sample queue and emit all queued samples.
    while (!media_sample_queue_.empty()) {
      EmitPendingSamples();
    }
    // Emit current sample.
    EmitSample(prev_pes_stream_id_, (*it).second, media_sample_, false);
  }
  return true;
}

void WvmMediaParser::EmitSample(
    uint32 parsed_audio_or_video_stream_id, uint32 stream_id,
    scoped_refptr<MediaSample>& new_sample, bool isLastSample) {
  DCHECK(new_sample);
  if (isLastSample) {
    if ((parsed_audio_or_video_stream_id & kPesStreamIdVideoMask)
        == kPesStreamIdVideo) {
      new_sample->set_duration(prev_media_sample_data_.video_sample_duration);
    } else if ((parsed_audio_or_video_stream_id & kPesStreamIdAudioMask)
        == kPesStreamIdAudio) {
      new_sample->set_duration(prev_media_sample_data_.audio_sample_duration);
    }
    new_sample_cb_.Run(stream_id, new_sample);
    return;
  }

  // Cannot emit current sample.  Compute duration first and then,
  // emit previous sample.
  if ((parsed_audio_or_video_stream_id & kPesStreamIdVideoMask)
      == kPesStreamIdVideo) {
    if (prev_media_sample_data_.video_sample == NULL) {
      prev_media_sample_data_.video_sample = new_sample;
      prev_media_sample_data_.video_stream_id = stream_id;
      return;
    }
    prev_media_sample_data_.video_sample->set_duration(
        new_sample->dts() - prev_media_sample_data_.video_sample->dts());
    prev_media_sample_data_.video_sample_duration =
        prev_media_sample_data_.video_sample->duration();
    new_sample_cb_.Run(prev_media_sample_data_.video_stream_id,
                       prev_media_sample_data_.video_sample);
    prev_media_sample_data_.video_sample = new_sample;
    prev_media_sample_data_.video_stream_id = stream_id;
  } else if ((parsed_audio_or_video_stream_id & kPesStreamIdAudioMask)
      == kPesStreamIdAudio) {
    if (prev_media_sample_data_.audio_sample == NULL) {
      prev_media_sample_data_.audio_sample = new_sample;
      prev_media_sample_data_.audio_stream_id = stream_id;
      return;
    }
    prev_media_sample_data_.audio_sample->set_duration(
        new_sample->dts() - prev_media_sample_data_.audio_sample->dts());
    prev_media_sample_data_.audio_sample_duration =
        prev_media_sample_data_.audio_sample->duration();
    new_sample_cb_.Run(prev_media_sample_data_.audio_stream_id,
                       prev_media_sample_data_.audio_sample);
    prev_media_sample_data_.audio_sample = new_sample;
    prev_media_sample_data_.audio_stream_id = stream_id;
  }
}

}  // namespace wvm
}  // namespace media
