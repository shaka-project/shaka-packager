// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MP4_BOX_DEFINITIONS_H_
#define MEDIA_MP4_BOX_DEFINITIONS_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "media/base/media_export.h"
#include "media/mp4/aac.h"
#include "media/mp4/avc.h"
#include "media/mp4/box_reader.h"
#include "media/mp4/fourccs.h"

namespace media {
namespace mp4 {

enum TrackType {
  kInvalid = 0,
  kVideo,
  kAudio,
  kHint
};

#define DECLARE_BOX_METHODS(T) \
  T(); \
  virtual ~T(); \
  virtual bool Parse(BoxReader* reader) OVERRIDE; \
  virtual FourCC BoxType() const OVERRIDE; \

struct MEDIA_EXPORT FileType : Box {
  DECLARE_BOX_METHODS(FileType);

  FourCC major_brand;
  uint32 minor_version;
};

struct MEDIA_EXPORT ProtectionSystemSpecificHeader : Box {
  DECLARE_BOX_METHODS(ProtectionSystemSpecificHeader);

  std::vector<uint8> system_id;
  std::vector<uint8> raw_box;
};

struct MEDIA_EXPORT SampleAuxiliaryInformationOffset : Box {
  DECLARE_BOX_METHODS(SampleAuxiliaryInformationOffset);

  std::vector<uint64> offsets;
};

struct MEDIA_EXPORT SampleAuxiliaryInformationSize : Box {
  DECLARE_BOX_METHODS(SampleAuxiliaryInformationSize);

  uint8 default_sample_info_size;
  uint32 sample_count;
  std::vector<uint8> sample_info_sizes;
};

struct MEDIA_EXPORT OriginalFormat : Box {
  DECLARE_BOX_METHODS(OriginalFormat);

  FourCC format;
};

struct MEDIA_EXPORT SchemeType : Box {
  DECLARE_BOX_METHODS(SchemeType);

  FourCC type;
  uint32 version;
};

struct MEDIA_EXPORT TrackEncryption : Box {
  DECLARE_BOX_METHODS(TrackEncryption);

  // Note: this definition is specific to the CENC protection type.
  bool is_encrypted;
  uint8 default_iv_size;
  std::vector<uint8> default_kid;
};

struct MEDIA_EXPORT SchemeInfo : Box {
  DECLARE_BOX_METHODS(SchemeInfo);

  TrackEncryption track_encryption;
};

struct MEDIA_EXPORT ProtectionSchemeInfo : Box {
  DECLARE_BOX_METHODS(ProtectionSchemeInfo);

  OriginalFormat format;
  SchemeType type;
  SchemeInfo info;
};

struct MEDIA_EXPORT MovieHeader : Box {
  DECLARE_BOX_METHODS(MovieHeader);

  uint64 creation_time;
  uint64 modification_time;
  uint32 timescale;
  uint64 duration;
  int32 rate;
  int16 volume;
  uint32 next_track_id;
};

struct MEDIA_EXPORT TrackHeader : Box {
  DECLARE_BOX_METHODS(TrackHeader);

  uint64 creation_time;
  uint64 modification_time;
  uint32 track_id;
  uint64 duration;
  int16 layer;
  int16 alternate_group;
  int16 volume;
  uint32 width;
  uint32 height;
};

struct MEDIA_EXPORT EditListEntry {
  uint64 segment_duration;
  int64 media_time;
  int16 media_rate_integer;
  int16 media_rate_fraction;
};

struct MEDIA_EXPORT EditList : Box {
  DECLARE_BOX_METHODS(EditList);

  std::vector<EditListEntry> edits;
};

struct MEDIA_EXPORT Edit : Box {
  DECLARE_BOX_METHODS(Edit);

  EditList list;
};

struct MEDIA_EXPORT HandlerReference : Box {
  DECLARE_BOX_METHODS(HandlerReference);

  TrackType type;
};

struct MEDIA_EXPORT AVCDecoderConfigurationRecord : Box {
  DECLARE_BOX_METHODS(AVCDecoderConfigurationRecord);

  uint8 version;
  uint8 profile_indication;
  uint8 profile_compatibility;
  uint8 avc_level;
  uint8 length_size;

  typedef std::vector<uint8> SPS;
  typedef std::vector<uint8> PPS;

  std::vector<SPS> sps_list;
  std::vector<PPS> pps_list;
};

struct MEDIA_EXPORT PixelAspectRatioBox : Box {
  DECLARE_BOX_METHODS(PixelAspectRatioBox);

  uint32 h_spacing;
  uint32 v_spacing;
};

struct MEDIA_EXPORT VideoSampleEntry : Box {
  DECLARE_BOX_METHODS(VideoSampleEntry);

  FourCC format;
  uint16 data_reference_index;
  uint16 width;
  uint16 height;

  PixelAspectRatioBox pixel_aspect;
  ProtectionSchemeInfo sinf;

  // Currently expected to be present regardless of format.
  AVCDecoderConfigurationRecord avcc;
};

struct MEDIA_EXPORT ElementaryStreamDescriptor : Box {
  DECLARE_BOX_METHODS(ElementaryStreamDescriptor);

  uint8 object_type;
  AAC aac;
};

struct MEDIA_EXPORT AudioSampleEntry : Box {
  DECLARE_BOX_METHODS(AudioSampleEntry);

  FourCC format;
  uint16 data_reference_index;
  uint16 channelcount;
  uint16 samplesize;
  uint32 samplerate;

  ProtectionSchemeInfo sinf;
  ElementaryStreamDescriptor esds;
};

struct MEDIA_EXPORT SampleDescription : Box {
  DECLARE_BOX_METHODS(SampleDescription);

  TrackType type;
  std::vector<VideoSampleEntry> video_entries;
  std::vector<AudioSampleEntry> audio_entries;
};

struct MEDIA_EXPORT SampleTable : Box {
  DECLARE_BOX_METHODS(SampleTable);

  // Media Source specific: we ignore many of the sub-boxes in this box,
  // including some that are required to be present in the BMFF spec. This
  // includes the 'stts', 'stsc', and 'stco' boxes, which must contain no
  // samples in order to be compliant files.
  SampleDescription description;
};

struct MEDIA_EXPORT MediaHeader : Box {
  DECLARE_BOX_METHODS(MediaHeader);

  uint64 creation_time;
  uint64 modification_time;
  uint32 timescale;
  uint64 duration;
};

struct MEDIA_EXPORT MediaInformation : Box {
  DECLARE_BOX_METHODS(MediaInformation);

  SampleTable sample_table;
};

struct MEDIA_EXPORT Media : Box {
  DECLARE_BOX_METHODS(Media);

  MediaHeader header;
  HandlerReference handler;
  MediaInformation information;
};

struct MEDIA_EXPORT Track : Box {
  DECLARE_BOX_METHODS(Track);

  TrackHeader header;
  Media media;
  Edit edit;
};

struct MEDIA_EXPORT MovieExtendsHeader : Box {
  DECLARE_BOX_METHODS(MovieExtendsHeader);

  uint64 fragment_duration;
};

struct MEDIA_EXPORT TrackExtends : Box {
  DECLARE_BOX_METHODS(TrackExtends);

  uint32 track_id;
  uint32 default_sample_description_index;
  uint32 default_sample_duration;
  uint32 default_sample_size;
  uint32 default_sample_flags;
};

struct MEDIA_EXPORT MovieExtends : Box {
  DECLARE_BOX_METHODS(MovieExtends);

  MovieExtendsHeader header;
  std::vector<TrackExtends> tracks;
};

struct MEDIA_EXPORT Movie : Box {
  DECLARE_BOX_METHODS(Movie);

  bool fragmented;
  MovieHeader header;
  MovieExtends extends;
  std::vector<Track> tracks;
  std::vector<ProtectionSystemSpecificHeader> pssh;
};

struct MEDIA_EXPORT TrackFragmentDecodeTime : Box {
  DECLARE_BOX_METHODS(TrackFragmentDecodeTime);

  uint64 decode_time;
};

struct MEDIA_EXPORT MovieFragmentHeader : Box {
  DECLARE_BOX_METHODS(MovieFragmentHeader);

  uint32 sequence_number;
};

struct MEDIA_EXPORT TrackFragmentHeader : Box {
  DECLARE_BOX_METHODS(TrackFragmentHeader);

  uint32 track_id;

  uint32 sample_description_index;
  uint32 default_sample_duration;
  uint32 default_sample_size;
  uint32 default_sample_flags;

  // As 'flags' might be all zero, we cannot use zeroness alone to identify
  // when default_sample_flags wasn't specified, unlike the other values.
  bool has_default_sample_flags;
};

struct MEDIA_EXPORT TrackFragmentRun : Box {
  DECLARE_BOX_METHODS(TrackFragmentRun);

  uint32 sample_count;
  uint32 data_offset;
  std::vector<uint32> sample_flags;
  std::vector<uint32> sample_sizes;
  std::vector<uint32> sample_durations;
  std::vector<int32> sample_composition_time_offsets;
};

struct MEDIA_EXPORT TrackFragment : Box {
  DECLARE_BOX_METHODS(TrackFragment);

  TrackFragmentHeader header;
  std::vector<TrackFragmentRun> runs;
  TrackFragmentDecodeTime decode_time;
  SampleAuxiliaryInformationOffset auxiliary_offset;
  SampleAuxiliaryInformationSize auxiliary_size;
};

struct MEDIA_EXPORT MovieFragment : Box {
  DECLARE_BOX_METHODS(MovieFragment);

  MovieFragmentHeader header;
  std::vector<TrackFragment> tracks;
  std::vector<ProtectionSystemSpecificHeader> pssh;
};

#undef DECLARE_BOX

}  // namespace mp4
}  // namespace media

#endif  // MEDIA_MP4_BOX_DEFINITIONS_H_
