// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MP4_BOX_DEFINITIONS_H_
#define MEDIA_MP4_BOX_DEFINITIONS_H_

#include <string>
#include <vector>

#include "media/mp4/aac_audio_specific_config.h"
#include "media/mp4/box.h"
#include "media/mp4/es_descriptor.h"
#include "media/mp4/fourccs.h"

namespace media {

class BufferReader;

namespace mp4 {

enum TrackType {
  kInvalid = 0,
  kVideo,
  kAudio,
  kHint
};

class BoxBuffer;

#define DECLARE_BOX_METHODS(T)                        \
  T();                                                \
  virtual ~T();                                       \
  virtual bool ReadWrite(BoxBuffer* buffer) OVERRIDE; \
  virtual FourCC BoxType() const OVERRIDE;            \
  virtual uint32 ComputeSize() OVERRIDE;

struct FileType : Box {
  DECLARE_BOX_METHODS(FileType);

  FourCC major_brand;
  uint32 minor_version;
  std::vector<FourCC> compatible_brands;
};

struct SegmentType : FileType {
  DECLARE_BOX_METHODS(SegmentType);
};

struct ProtectionSystemSpecificHeader : FullBox {
  DECLARE_BOX_METHODS(ProtectionSystemSpecificHeader);

  std::vector<uint8> system_id;
  std::vector<uint8> data;
  std::vector<uint8> raw_box;
};

struct SampleAuxiliaryInformationOffset : FullBox {
  DECLARE_BOX_METHODS(SampleAuxiliaryInformationOffset);

  std::vector<uint64> offsets;
};

struct SampleAuxiliaryInformationSize : FullBox {
  DECLARE_BOX_METHODS(SampleAuxiliaryInformationSize);

  uint8 default_sample_info_size;
  uint32 sample_count;
  std::vector<uint8> sample_info_sizes;
};

struct OriginalFormat : Box {
  DECLARE_BOX_METHODS(OriginalFormat);

  FourCC format;
};

struct SchemeType : FullBox {
  DECLARE_BOX_METHODS(SchemeType);

  FourCC type;
  uint32 version;
};

struct TrackEncryption : FullBox {
  DECLARE_BOX_METHODS(TrackEncryption);

  // Note: this definition is specific to the CENC protection type.
  bool is_encrypted;
  uint8 default_iv_size;
  std::vector<uint8> default_kid;
};

struct SchemeInfo : Box {
  DECLARE_BOX_METHODS(SchemeInfo);

  TrackEncryption track_encryption;
};

struct ProtectionSchemeInfo : Box {
  DECLARE_BOX_METHODS(ProtectionSchemeInfo);

  OriginalFormat format;
  SchemeType type;
  SchemeInfo info;
};

struct MovieHeader : FullBox {
  DECLARE_BOX_METHODS(MovieHeader);

  uint64 creation_time;
  uint64 modification_time;
  uint32 timescale;
  uint64 duration;
  int32 rate;
  int16 volume;
  uint32 next_track_id;
};

struct TrackHeader : FullBox {
  enum TrackHeaderFlags {
    kTrackEnabled   = 0x000001,
    kTrackInMovie   = 0x000002,
    kTrackInPreview = 0x000004,
  };

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

struct EditListEntry {
  uint64 segment_duration;
  int64 media_time;
  int16 media_rate_integer;
  int16 media_rate_fraction;
};

struct EditList : FullBox {
  DECLARE_BOX_METHODS(EditList);

  std::vector<EditListEntry> edits;
};

struct Edit : Box {
  DECLARE_BOX_METHODS(Edit);

  EditList list;
};

struct HandlerReference : FullBox {
  DECLARE_BOX_METHODS(HandlerReference);

  TrackType type;
};

struct AVCDecoderConfigurationRecord : Box {
  DECLARE_BOX_METHODS(AVCDecoderConfigurationRecord);
  bool ParseData(BufferReader* reader);

  // Contains full avc decoder configuration record as defined in iso14496-15
  // 5.2.4.1, including possible extension bytes described in paragraph 3.
  // Known fields defined in the spec are also parsed and included in this
  // structure.
  std::vector<uint8> data;

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

struct PixelAspectRatioBox : Box {
  DECLARE_BOX_METHODS(PixelAspectRatioBox);

  uint32 h_spacing;
  uint32 v_spacing;
};

struct VideoSampleEntry : Box {
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

struct ElementaryStreamDescriptor : FullBox {
  DECLARE_BOX_METHODS(ElementaryStreamDescriptor);

  AACAudioSpecificConfig aac_audio_specific_config;
  ESDescriptor es_descriptor;
};

struct AudioSampleEntry : Box {
  DECLARE_BOX_METHODS(AudioSampleEntry);

  FourCC format;
  uint16 data_reference_index;
  uint16 channelcount;
  uint16 samplesize;
  uint32 samplerate;

  ProtectionSchemeInfo sinf;
  ElementaryStreamDescriptor esds;
};

struct SampleDescription : FullBox {
  DECLARE_BOX_METHODS(SampleDescription);

  TrackType type;
  std::vector<VideoSampleEntry> video_entries;
  std::vector<AudioSampleEntry> audio_entries;
};

struct DecodingTime {
  uint32 sample_count;
  uint32 sample_delta;
};

// stts.
struct DecodingTimeToSample : FullBox {
  DECLARE_BOX_METHODS(DecodingTimeToSample);

  std::vector<DecodingTime> decoding_time;
};

struct CompositionOffset {
  uint32 sample_count;
  // If version == 0, sample_offset is uint32;
  // If version == 1, sample_offset is int32.
  // Always use signed version, which should work unless the offset
  // exceeds 31 bits, which shouldn't happen.
  int32 sample_offset;
};

// ctts. Optional.
struct CompositionTimeToSample : FullBox {
  DECLARE_BOX_METHODS(CompositionTimeToSample);

  std::vector<CompositionOffset> composition_offset;
};

struct ChunkInfo {
  uint32 first_chunk;
  uint32 samples_per_chunk;
  uint32 sample_description_index;
};

// stsc.
struct SampleToChunk : FullBox {
  DECLARE_BOX_METHODS(SampleToChunk);

  std::vector<ChunkInfo> chunk_info;
};

// stsz.
struct SampleSize : FullBox {
  DECLARE_BOX_METHODS(SampleSize);

  uint32 sample_size;
  uint32 sample_count;
  std::vector<uint32> sizes;
};

// stz2.
struct CompactSampleSize : FullBox {
  DECLARE_BOX_METHODS(CompactSampleSize);

  uint8 field_size;
  std::vector<uint32> sizes;
};

// co64.
struct ChunkLargeOffset : FullBox {
  DECLARE_BOX_METHODS(ChunkLargeOffset);

  std::vector<uint64> offsets;
};

// stco.
struct ChunkOffset : ChunkLargeOffset {
  DECLARE_BOX_METHODS(ChunkOffset);
};

// stss. Optional.
struct SyncSample : FullBox {
  DECLARE_BOX_METHODS(SyncSample);

  std::vector<uint32> sample_number;
};

struct SampleTable : Box {
  DECLARE_BOX_METHODS(SampleTable);

  SampleDescription description;
  DecodingTimeToSample decoding_time_to_sample;
  CompositionTimeToSample composition_time_to_sample;
  SampleToChunk sample_to_chunk;
  // Either SampleSize or CompactSampleSize must present. Store in SampleSize.
  SampleSize sample_size;
  // Either ChunkOffset or ChunkLargeOffset must present. Store in
  // ChunkLargeOffset.
  ChunkLargeOffset chunk_large_offset;
  SyncSample sync_sample;
};

struct MediaHeader : FullBox {
  DECLARE_BOX_METHODS(MediaHeader);

  uint64 creation_time;
  uint64 modification_time;
  uint32 timescale;
  uint64 duration;
  // 3-char language code + 1 null terminating char.
  char language[4];
};

struct VideoMediaHeader : FullBox {
  DECLARE_BOX_METHODS(VideoMediaHeader);

  uint16 graphicsmode;
  uint16 opcolor_red;
  uint16 opcolor_green;
  uint16 opcolor_blue;
};

struct SoundMediaHeader : FullBox {
  DECLARE_BOX_METHODS(SoundMediaHeader);

  uint16 balance;
};

struct DataEntryUrl : FullBox {
  DECLARE_BOX_METHODS(DataEntryUrl);

  std::vector<uint8> location;
};

struct DataReference : FullBox {
  DECLARE_BOX_METHODS(DataReference);

  // data entry can be either url or urn box. Fix to url box for now.
  std::vector<DataEntryUrl> data_entry;
};

struct DataInformation : Box {
  DECLARE_BOX_METHODS(DataInformation);

  DataReference dref;
};

struct MediaInformation : Box {
  DECLARE_BOX_METHODS(MediaInformation);

  DataInformation dinf;
  SampleTable sample_table;
  // Exactly one specific meida header shall be present, vmhd, smhd, hmhd, nmhd.
  VideoMediaHeader vmhd;
  SoundMediaHeader smhd;
};

struct Media : Box {
  DECLARE_BOX_METHODS(Media);

  MediaHeader header;
  HandlerReference handler;
  MediaInformation information;
};

struct Track : Box {
  DECLARE_BOX_METHODS(Track);

  TrackHeader header;
  Media media;
  Edit edit;
};

struct MovieExtendsHeader : FullBox {
  DECLARE_BOX_METHODS(MovieExtendsHeader);

  uint64 fragment_duration;
};

struct TrackExtends : FullBox {
  DECLARE_BOX_METHODS(TrackExtends);

  uint32 track_id;
  uint32 default_sample_description_index;
  uint32 default_sample_duration;
  uint32 default_sample_size;
  uint32 default_sample_flags;
};

struct MovieExtends : Box {
  DECLARE_BOX_METHODS(MovieExtends);

  MovieExtendsHeader header;
  std::vector<TrackExtends> tracks;
};

struct Movie : Box {
  DECLARE_BOX_METHODS(Movie);

  MovieHeader header;
  MovieExtends extends;
  std::vector<Track> tracks;
  std::vector<ProtectionSystemSpecificHeader> pssh;
};

struct TrackFragmentDecodeTime : FullBox {
  DECLARE_BOX_METHODS(TrackFragmentDecodeTime);

  uint64 decode_time;
};

struct MovieFragmentHeader : FullBox {
  DECLARE_BOX_METHODS(MovieFragmentHeader);

  uint32 sequence_number;
};

struct TrackFragmentHeader : FullBox {
  enum TrackFragmentFlagsMasks {
    kDataOffsetPresentMask              = 0x000001,
    kSampleDescriptionIndexPresentMask  = 0x000002,
    kDefaultSampleDurationPresentMask   = 0x000008,
    kDefaultSampleSizePresentMask       = 0x000010,
    kDefaultSampleFlagsPresentMask      = 0x000020,
    kDurationIsEmptyMask                = 0x010000,
    kDefaultBaseIsMoofMask              = 0x020000,
  };

  enum SampleFlagsMasks {
    kReservedMask                  = 0xFC000000,
    kSampleDependsOnMask           = 0x03000000,
    kSampleIsDependedOnMask        = 0x00C00000,
    kSampleHasRedundancyMask       = 0x00300000,
    kSamplePaddingValueMask        = 0x000E0000,
    kNonKeySampleMask              = 0x00010000,
    kSampleDegradationPriorityMask = 0x0000FFFF,
  };

  DECLARE_BOX_METHODS(TrackFragmentHeader);

  uint32 track_id;
  uint32 sample_description_index;
  uint32 default_sample_duration;
  uint32 default_sample_size;
  uint32 default_sample_flags;
};

struct TrackFragmentRun : FullBox {
  enum TrackFragmentFlagsMasks {
    kDataOffsetPresentMask              = 0x000001,
    kFirstSampleFlagsPresentMask        = 0x000004,
    kSampleDurationPresentMask          = 0x000100,
    kSampleSizePresentMask              = 0x000200,
    kSampleFlagsPresentMask             = 0x000400,
    kSampleCompTimeOffsetsPresentMask   = 0x000800,
  };

  DECLARE_BOX_METHODS(TrackFragmentRun);

  uint32 sample_count;
  uint32 data_offset;
  std::vector<uint32> sample_flags;
  std::vector<uint32> sample_sizes;
  std::vector<uint32> sample_durations;
  std::vector<int32> sample_composition_time_offsets;
};

struct SampleToGroupEntry {
  enum GroupDescriptionIndexBase {
    kTrackGroupDescriptionIndexBase = 0,
    kTrackFragmentGroupDescriptionIndexBase = 0x10000,
  };

  uint32 sample_count;
  uint32 group_description_index;
};

struct SampleToGroup : FullBox {
  DECLARE_BOX_METHODS(SampleToGroup);

  uint32 grouping_type;
  uint32 grouping_type_parameter;  // Version 1 only.
  std::vector<SampleToGroupEntry> entries;
};

struct CencSampleEncryptionInfoEntry {
  CencSampleEncryptionInfoEntry();
  ~CencSampleEncryptionInfoEntry();

  bool is_encrypted;
  uint8 iv_size;
  std::vector<uint8> key_id;
};

struct SampleGroupDescription : FullBox {
  DECLARE_BOX_METHODS(SampleGroupDescription);

  uint32 grouping_type;
  std::vector<CencSampleEncryptionInfoEntry> entries;
};

struct TrackFragment : Box {
  DECLARE_BOX_METHODS(TrackFragment);

  TrackFragmentHeader header;
  std::vector<TrackFragmentRun> runs;
  TrackFragmentDecodeTime decode_time;
  SampleToGroup sample_to_group;
  SampleGroupDescription sample_group_description;
  SampleAuxiliaryInformationSize auxiliary_size;
  SampleAuxiliaryInformationOffset auxiliary_offset;
};

struct MovieFragment : Box {
  DECLARE_BOX_METHODS(MovieFragment);

  MovieFragmentHeader header;
  std::vector<TrackFragment> tracks;
  std::vector<ProtectionSystemSpecificHeader> pssh;
};

struct SegmentReference {
  enum SAPType {
    TypeUnknown = 0,
    Type1 = 1,  // T(ept) = T(dec) = T(sap) = T(ptf)
    Type2 = 2,  // T(ept) = T(dec) = T(sap) < T(ptf)
    Type3 = 3,  // T(ept) < T(dec) = T(sap) <= T(ptf)
    Type4 = 4,  // T(ept) <= T(ptf) < T(dec) = T(sap)
    Type5 = 5,  // T(ept) = T(dec) < T(sap)
    Type6 = 6,  // T(ept) < T(dec) < T(sap)
  };

  bool reference_type;
  uint32 referenced_size;
  uint32 subsegment_duration;
  bool starts_with_sap;
  SAPType sap_type;
  uint32 sap_delta_time;
  // We add this field to keep track of earliest_presentation_time in this
  // subsegment. It is not part of SegmentReference.
  uint64 earliest_presentation_time;
};

struct SegmentIndex : FullBox {
  DECLARE_BOX_METHODS(SegmentIndex);

  uint32 reference_id;
  uint32 timescale;
  uint64 earliest_presentation_time;
  uint64 first_offset;
  std::vector<SegmentReference> references;
};

// The actual data is parsed and written separately, so we do not inherit it
// from Box.
struct MediaData {
  MediaData();
  ~MediaData();
  void Write(BufferWriter* buffer_writer);
  uint32 ComputeSize();
  FourCC BoxType() const;

  uint32 data_size;
};

#undef DECLARE_BOX

}  // namespace mp4
}  // namespace media

#endif  // MEDIA_MP4_BOX_DEFINITIONS_H_
