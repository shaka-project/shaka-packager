// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PACKAGER_MEDIA_FORMATS_MP4_BOX_DEFINITIONS_H_
#define PACKAGER_MEDIA_FORMATS_MP4_BOX_DEFINITIONS_H_

#include <vector>

#include "packager/media/base/decrypt_config.h"
#include "packager/media/base/fourccs.h"
#include "packager/media/codecs/aac_audio_specific_config.h"
#include "packager/media/codecs/es_descriptor.h"
#include "packager/media/formats/mp4/box.h"

namespace shaka {
namespace media {

class BufferReader;

namespace mp4 {

enum TrackType {
  kInvalid = 0,
  kVideo,
  kAudio,
  kHint,
  kText,
  kSubtitle,
};

class BoxBuffer;

#define DECLARE_BOX_METHODS(T)                        \
 public:                                              \
  T();                                                \
  ~T() override;                                      \
                                                      \
  FourCC BoxType() const override;                    \
                                                      \
 private:                                             \
  bool ReadWriteInternal(BoxBuffer* buffer) override; \
  size_t ComputeSizeInternal() override;              \
                                                      \
 public:

struct FileType : Box {
  DECLARE_BOX_METHODS(FileType);

  FourCC major_brand = FOURCC_NULL;
  uint32_t minor_version = 0;
  std::vector<FourCC> compatible_brands;
};

struct SegmentType : FileType {
  FourCC BoxType() const override;
};

struct ProtectionSystemSpecificHeader : FullBox {
  DECLARE_BOX_METHODS(ProtectionSystemSpecificHeader);

  std::vector<uint8_t> raw_box;
};

struct SampleAuxiliaryInformationOffset : FullBox {
  DECLARE_BOX_METHODS(SampleAuxiliaryInformationOffset);

  std::vector<uint64_t> offsets;
};

struct SampleAuxiliaryInformationSize : FullBox {
  DECLARE_BOX_METHODS(SampleAuxiliaryInformationSize);

  uint8_t default_sample_info_size = 0;
  uint32_t sample_count = 0;
  std::vector<uint8_t> sample_info_sizes;
};

struct SampleEncryptionEntry {
  /// Read/Write SampleEncryptionEntry.
  /// @param iv_size specifies the size of initialization vector.
  /// @param has_subsamples indicates whether this sample encryption entry
  ///        constains subsamples.
  /// @param buffer points to the box buffer for reading or writing.
  /// @return true on success, false otherwise.
  bool ReadWrite(uint8_t iv_size, bool has_subsamples, BoxBuffer* buffer);
  /// Parse SampleEncryptionEntry from buffer.
  /// @param iv_size specifies the size of initialization vector.
  /// @param has_subsamples indicates whether this sample encryption entry
  ///        constains subsamples.
  /// @param reader points to the buffer reader. Cannot be NULL.
  /// @return true on success, false otherwise.
  bool ParseFromBuffer(uint8_t iv_size,
                       bool has_subsamples,
                       BufferReader* reader);
  /// @return The size of the structure in bytes when it is stored.
  uint32_t ComputeSize() const;
  /// @return The accumulated size of subsamples. Returns 0 if there is no
  ///         subsamples.
  uint32_t GetTotalSizeOfSubsamples() const;

  std::vector<uint8_t> initialization_vector;
  std::vector<SubsampleEntry> subsamples;
};

struct SampleEncryption : FullBox {
  static const uint8_t kInvalidIvSize = 1;

  enum SampleEncryptionFlags {
    kUseSubsampleEncryption = 2,
  };

  DECLARE_BOX_METHODS(SampleEncryption);
  /// Parse from @a sample_encryption_data.
  /// @param iv_size specifies the size of initialization vector.
  /// @param[out] sample_encryption_entries receives parsed sample encryption
  ///             entries.
  /// @return true on success, false otherwise.
  bool ParseFromSampleEncryptionData(
      uint8_t iv_size,
      std::vector<SampleEncryptionEntry>* sample_encryption_entries) const;

  /// We may not know @a iv_size before reading this box. In this case, we will
  /// store sample encryption data for parsing later when @a iv_size is known.
  std::vector<uint8_t> sample_encryption_data;

  uint8_t iv_size = kInvalidIvSize;
  std::vector<SampleEncryptionEntry> sample_encryption_entries;
};

struct OriginalFormat : Box {
  DECLARE_BOX_METHODS(OriginalFormat);

  FourCC format = FOURCC_NULL;
};

struct SchemeType : FullBox {
  DECLARE_BOX_METHODS(SchemeType);

  FourCC type = FOURCC_NULL;
  uint32_t version = 0u;
};

struct TrackEncryption : FullBox {
  DECLARE_BOX_METHODS(TrackEncryption);

  uint8_t default_is_protected = 0;
  uint8_t default_per_sample_iv_size = 0;
  // Default to a vector of 16 zeros.
  std::vector<uint8_t> default_kid = std::vector<uint8_t>(16, 0);

  // For pattern-based encryption.
  uint8_t default_crypt_byte_block = 0;
  uint8_t default_skip_byte_block = 0;

  // Present only if
  // |default_is_protected == 1 && default_per_sample_iv_size == 0|.
  std::vector<uint8_t> default_constant_iv;
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

  uint64_t creation_time = 0;
  uint64_t modification_time = 0;
  uint32_t timescale = 0;
  uint64_t duration = 0;
  int32_t rate = 1 << 16;
  int16_t volume = 1 << 8;
  uint32_t next_track_id = 0;
};

struct TrackHeader : FullBox {
  enum TrackHeaderFlags {
    kTrackEnabled = 0x000001,
    kTrackInMovie = 0x000002,
    kTrackInPreview = 0x000004,
  };

  DECLARE_BOX_METHODS(TrackHeader);

  uint64_t creation_time = 0;
  uint64_t modification_time = 0;
  uint32_t track_id = 0;
  uint64_t duration = 0;
  int16_t layer = 0;
  int16_t alternate_group = 0;
  int16_t volume = -1;
  // width and height specify the track's visual presentation size as
  // fixed-point 16.16 values.
  uint32_t width = 0;
  uint32_t height = 0;
};

struct EditListEntry {
  uint64_t segment_duration = 0;
  int64_t media_time = 0;
  int16_t media_rate_integer = 0;
  int16_t media_rate_fraction = 0;
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

  FourCC handler_type = FOURCC_NULL;
};

struct Language {
  bool ReadWrite(BoxBuffer* buffer);
  uint32_t ComputeSize() const;

  std::string code;
};

/// Implemented per http://mp4ra.org/#/references.
struct ID3v2 : FullBox {
  DECLARE_BOX_METHODS(ID3v2);

  Language language;
  std::vector<uint8_t> id3v2_data;
};

struct Metadata : FullBox {
  DECLARE_BOX_METHODS(Metadata);

  HandlerReference handler;
  ID3v2 id3v2;
};

// This defines a common structure for various CodecConfiguration boxes:
// AVCConfiguration, HEVCConfiguration and VPCodecConfiguration.
// Note that unlike the other two CodecConfiguration boxes, VPCodecConfiguration
// box inherits from FullBox instead of Box, according to VP Codec ISO Media
// File Format Binding specification. It will be handled properly in the
// implementation.
struct CodecConfiguration : Box {
  DECLARE_BOX_METHODS(CodecConfiguration);

  FourCC box_type = FOURCC_NULL;
  // Contains full codec configuration record, including possible extension
  // boxes.
  std::vector<uint8_t> data;
};

struct ColorParameters : Box {
  DECLARE_BOX_METHODS(ColorParameters);

  FourCC color_parameter_type = FOURCC_NULL;
  uint16_t color_primaries = 1;
  uint16_t transfer_characteristics = 1;
  uint16_t matrix_coefficients = 1;
  uint8_t video_full_range_flag = 0;
  std::vector<uint8_t> raw_box;
};

struct PixelAspectRatio : Box {
  DECLARE_BOX_METHODS(PixelAspectRatio);

  uint32_t h_spacing = 0u;
  uint32_t v_spacing = 0u;
};

struct VideoSampleEntry : Box {
  DECLARE_BOX_METHODS(VideoSampleEntry);

  // Returns actual format of this sample entry.
  FourCC GetActualFormat() const {
    return format == FOURCC_encv ? sinf.format.format : format;
  }
  // Returns the box type of codec configuration box from video format.
  FourCC GetCodecConfigurationBoxType(FourCC format) const;

  // Convert |extra_codec_configs| to vector.
  std::vector<uint8_t> ExtraCodecConfigsAsVector() const;
  // Parse |extra_codec_configs| from vector.
  bool ParseExtraCodecConfigsVector(const std::vector<uint8_t>& data);

  FourCC format = FOURCC_NULL;
  // data_reference_index is 1-based and "dref" box is mandatory so it is
  // always present.
  uint16_t data_reference_index = 1u;
  uint16_t width = 0u;
  uint16_t height = 0u;

  ColorParameters colr;
  PixelAspectRatio pixel_aspect;
  ProtectionSchemeInfo sinf;
  CodecConfiguration codec_configuration;
  // Some codecs, e.g. Dolby Vision, have extra codec configuration boxes that
  // need to be propagated to muxers.
  std::vector<CodecConfiguration> extra_codec_configs;
};

struct ElementaryStreamDescriptor : FullBox {
  DECLARE_BOX_METHODS(ElementaryStreamDescriptor);

  AACAudioSpecificConfig aac_audio_specific_config;
  ESDescriptor es_descriptor;
};

struct DTSSpecific : Box {
  DECLARE_BOX_METHODS(DTSSpecific);

  uint32_t sampling_frequency = 0u;
  uint32_t max_bitrate = 0u;
  uint32_t avg_bitrate = 0u;
  uint8_t pcm_sample_depth = 0u;
  std::vector<uint8_t> extra_data;
};

struct AC3Specific : Box {
  DECLARE_BOX_METHODS(AC3Specific);

  std::vector<uint8_t> data;
};

struct MHAConfiguration : Box {
  DECLARE_BOX_METHODS(MHAConfiguration);

  std::vector<uint8_t> data;
  uint8_t mpeg_h_3da_profile_level_indication;
};

struct EC3Specific : Box {
  DECLARE_BOX_METHODS(EC3Specific);

  std::vector<uint8_t> data;
};

struct AC4Specific : Box {
  DECLARE_BOX_METHODS(AC4Specific);

  std::vector<uint8_t> data;
};

struct OpusSpecific : Box {
  DECLARE_BOX_METHODS(OpusSpecific);

  std::vector<uint8_t> opus_identification_header;
  // The number of priming samples. Extracted from |opus_identification_header|.
  uint16_t preskip = 0u;
};

// FLAC specific decoder configuration box:
//   https://github.com/xiph/flac/blob/master/doc/isoflac.txt
// We do not care about the actual data inside, which is simply copied over.
struct FlacSpecific : FullBox {
  DECLARE_BOX_METHODS(FlacSpecific);

  std::vector<uint8_t> data;
};

struct AudioSampleEntry : Box {
  DECLARE_BOX_METHODS(AudioSampleEntry);

  // Returns actual format of this sample entry.
  FourCC GetActualFormat() const {
    return format == FOURCC_enca ? sinf.format.format : format;
  }

  FourCC format = FOURCC_NULL;
  // data_reference_index is 1-based and "dref" box is mandatory so it is
  // always present.
  uint16_t data_reference_index = 1u;
  uint16_t channelcount = 2u;
  uint16_t samplesize = 16u;
  uint32_t samplerate = 0u;

  ProtectionSchemeInfo sinf;

  ElementaryStreamDescriptor esds;
  DTSSpecific ddts;
  AC3Specific dac3;
  EC3Specific dec3;
  AC4Specific dac4;
  OpusSpecific dops;
  FlacSpecific dfla;
  MHAConfiguration mhac;
};

struct WebVTTConfigurationBox : Box {
  DECLARE_BOX_METHODS(WebVTTConfigurationBox);

  std::string config;
};

struct WebVTTSourceLabelBox : Box {
  DECLARE_BOX_METHODS(WebVTTSourceLabelBox);

  std::string source_label;
};

struct TextSampleEntry : Box {
  DECLARE_BOX_METHODS(TextSampleEntry);

  // Specifies fourcc of this sample entry. It needs to be set on write, e.g.
  // set to 'wvtt' to write WVTTSampleEntry; On read, it is recovered from box
  // header.
  FourCC format = FOURCC_NULL;

  // data_reference_index is 1-based and "dref" box is mandatory so it is
  // always present.
  uint16_t data_reference_index = 1u;

  // Sub fields for ttml text sample entry.
  std::string namespace_;
  std::string schema_location;
  // Optional MPEG4BitRateBox.

  // Sub boxes for wvtt text sample entry.
  WebVTTConfigurationBox config;
  WebVTTSourceLabelBox label;
  // Optional MPEG4BitRateBox.
};

struct SampleDescription : FullBox {
  DECLARE_BOX_METHODS(SampleDescription);

  TrackType type = kInvalid;
  // TODO(kqyang): Clean up the code to have one single member, e.g. by creating
  // SampleEntry struct, std::vector<SampleEntry> sample_entries.
  std::vector<VideoSampleEntry> video_entries;
  std::vector<AudioSampleEntry> audio_entries;
  std::vector<TextSampleEntry> text_entries;
};

struct DecodingTime {
  uint32_t sample_count;
  uint32_t sample_delta;
};

// stts.
struct DecodingTimeToSample : FullBox {
  DECLARE_BOX_METHODS(DecodingTimeToSample);

  std::vector<DecodingTime> decoding_time;
};

struct CompositionOffset {
  uint32_t sample_count;
  // If version == 0, sample_offset is uint32_t;
  // If version == 1, sample_offset is int32_t.
  // Use int64_t so both can be supported properly.
  int64_t sample_offset;
};

// ctts. Optional.
struct CompositionTimeToSample : FullBox {
  DECLARE_BOX_METHODS(CompositionTimeToSample);

  std::vector<CompositionOffset> composition_offset;
};

struct ChunkInfo {
  uint32_t first_chunk;
  uint32_t samples_per_chunk;
  uint32_t sample_description_index;
};

// stsc.
struct SampleToChunk : FullBox {
  DECLARE_BOX_METHODS(SampleToChunk);

  std::vector<ChunkInfo> chunk_info;
};

// stsz.
struct SampleSize : FullBox {
  DECLARE_BOX_METHODS(SampleSize);

  uint32_t sample_size = 0u;
  uint32_t sample_count = 0u;
  std::vector<uint32_t> sizes;
};

// stz2.
struct CompactSampleSize : FullBox {
  DECLARE_BOX_METHODS(CompactSampleSize);

  uint8_t field_size = 0u;
  std::vector<uint32_t> sizes;
};

// co64.
struct ChunkLargeOffset : FullBox {
  DECLARE_BOX_METHODS(ChunkLargeOffset);

  std::vector<uint64_t> offsets;
};

// stco.
struct ChunkOffset : ChunkLargeOffset {
  DECLARE_BOX_METHODS(ChunkOffset);
};

// stss. Optional.
struct SyncSample : FullBox {
  DECLARE_BOX_METHODS(SyncSample);

  std::vector<uint32_t> sample_number;
};

struct CencSampleEncryptionInfoEntry {
  bool ReadWrite(BoxBuffer* buffer);
  uint32_t ComputeSize() const;

  uint8_t is_protected = 0u;
  uint8_t per_sample_iv_size = 0u;
  std::vector<uint8_t> key_id;

  // For pattern-based encryption.
  uint8_t crypt_byte_block = 0u;
  uint8_t skip_byte_block = 0u;

  // Present only if |is_protected == 1 && per_sample_iv_size == 0|.
  std::vector<uint8_t> constant_iv;
};

struct AudioRollRecoveryEntry {
  bool ReadWrite(BoxBuffer* buffer);
  uint32_t ComputeSize() const;

  int16_t roll_distance = 0;
};

struct SampleGroupDescription : FullBox {
  DECLARE_BOX_METHODS(SampleGroupDescription);

  template <typename T>
  bool ReadWriteEntries(BoxBuffer* buffer, std::vector<T>* entries);

  uint32_t grouping_type = 0;
  // Only present if grouping_type == 'seig'.
  std::vector<CencSampleEncryptionInfoEntry>
      cenc_sample_encryption_info_entries;
  // Only present if grouping_type == 'roll'.
  std::vector<AudioRollRecoveryEntry> audio_roll_recovery_entries;
};

struct SampleToGroupEntry {
  enum GroupDescriptionIndexBase {
    kTrackGroupDescriptionIndexBase = 0,
    kTrackFragmentGroupDescriptionIndexBase = 0x10000,
  };

  uint32_t sample_count = 0u;
  uint32_t group_description_index = 0u;
};

struct SampleToGroup : FullBox {
  DECLARE_BOX_METHODS(SampleToGroup);

  uint32_t grouping_type = 0u;
  uint32_t grouping_type_parameter = 0u;  // Version 1 only.
  std::vector<SampleToGroupEntry> entries;
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
  std::vector<SampleGroupDescription> sample_group_descriptions;
  std::vector<SampleToGroup> sample_to_groups;
};

struct MediaHeader : FullBox {
  DECLARE_BOX_METHODS(MediaHeader);

  uint64_t creation_time = 0u;
  uint64_t modification_time = 0u;
  uint32_t timescale = 0u;
  uint64_t duration = 0u;
  Language language;
};

struct VideoMediaHeader : FullBox {
  DECLARE_BOX_METHODS(VideoMediaHeader);

  uint16_t graphicsmode = 0u;
  uint16_t opcolor_red = 0u;
  uint16_t opcolor_green = 0u;
  uint16_t opcolor_blue = 0u;
};

struct SoundMediaHeader : FullBox {
  DECLARE_BOX_METHODS(SoundMediaHeader);

  uint16_t balance = 0u;
};

struct NullMediaHeader : FullBox {
  DECLARE_BOX_METHODS(NullMediaHeader);
};

struct SubtitleMediaHeader : FullBox {
  DECLARE_BOX_METHODS(SubtitleMediaHeader);
};

struct DataEntryUrl : FullBox {
  DECLARE_BOX_METHODS(DataEntryUrl);

  std::vector<uint8_t> location;
};

struct DataReference : FullBox {
  DECLARE_BOX_METHODS(DataReference);

  // Can be either url or urn box. Fix to url box for now.
  std::vector<DataEntryUrl> data_entry = std::vector<DataEntryUrl>(1);
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
  NullMediaHeader nmhd;
  SubtitleMediaHeader sthd;
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
  SampleEncryption sample_encryption;
};

struct MovieExtendsHeader : FullBox {
  DECLARE_BOX_METHODS(MovieExtendsHeader);

  uint64_t fragment_duration = 0u;
};

struct TrackExtends : FullBox {
  DECLARE_BOX_METHODS(TrackExtends);

  uint32_t track_id = 0u;
  uint32_t default_sample_description_index = 0u;
  uint32_t default_sample_duration = 0u;
  uint32_t default_sample_size = 0u;
  uint32_t default_sample_flags = 0u;
};

struct MovieExtends : Box {
  DECLARE_BOX_METHODS(MovieExtends);

  MovieExtendsHeader header;
  std::vector<TrackExtends> tracks;
};

struct Movie : Box {
  DECLARE_BOX_METHODS(Movie);

  MovieHeader header;
  Metadata metadata;  // Used to hold version information.
  MovieExtends extends;
  std::vector<Track> tracks;
  std::vector<ProtectionSystemSpecificHeader> pssh;
};

struct TrackFragmentDecodeTime : FullBox {
  DECLARE_BOX_METHODS(TrackFragmentDecodeTime);

  uint64_t decode_time = 0u;
};

struct MovieFragmentHeader : FullBox {
  DECLARE_BOX_METHODS(MovieFragmentHeader);

  uint32_t sequence_number = 0u;
};

struct TrackFragmentHeader : FullBox {
  enum TrackFragmentFlagsMasks {
    kBaseDataOffsetPresentMask = 0x000001,
    kSampleDescriptionIndexPresentMask = 0x000002,
    kDefaultSampleDurationPresentMask = 0x000008,
    kDefaultSampleSizePresentMask = 0x000010,
    kDefaultSampleFlagsPresentMask = 0x000020,
    kDurationIsEmptyMask = 0x010000,
    kDefaultBaseIsMoofMask = 0x020000,
  };

  enum SampleFlagsMasks {
    kReservedMask = 0xFC000000,
    kSampleDependsOnMask = 0x03000000,
    kSampleIsDependedOnMask = 0x00C00000,
    kSampleHasRedundancyMask = 0x00300000,
    kSamplePaddingValueMask = 0x000E0000,
    kNonKeySampleMask = 0x00010000,
    kSampleDegradationPriorityMask = 0x0000FFFF,
  };

  DECLARE_BOX_METHODS(TrackFragmentHeader);

  uint32_t track_id = 0u;
  uint32_t sample_description_index = 0u;
  uint32_t default_sample_duration = 0u;
  uint32_t default_sample_size = 0u;
  uint32_t default_sample_flags = 0u;
};

struct TrackFragmentRun : FullBox {
  enum TrackFragmentFlagsMasks {
    kDataOffsetPresentMask = 0x000001,
    kFirstSampleFlagsPresentMask = 0x000004,
    kSampleDurationPresentMask = 0x000100,
    kSampleSizePresentMask = 0x000200,
    kSampleFlagsPresentMask = 0x000400,
    kSampleCompTimeOffsetsPresentMask = 0x000800,
  };

  DECLARE_BOX_METHODS(TrackFragmentRun);

  uint32_t sample_count = 0u;
  uint32_t data_offset = 0u;
  std::vector<uint32_t> sample_flags;
  std::vector<uint32_t> sample_sizes;
  std::vector<uint32_t> sample_durations;
  std::vector<int64_t> sample_composition_time_offsets;
};

struct TrackFragment : Box {
  DECLARE_BOX_METHODS(TrackFragment);

  TrackFragmentHeader header;
  std::vector<TrackFragmentRun> runs;
  bool decode_time_absent = false;
  TrackFragmentDecodeTime decode_time;
  std::vector<SampleGroupDescription> sample_group_descriptions;
  std::vector<SampleToGroup> sample_to_groups;
  SampleAuxiliaryInformationSize auxiliary_size;
  SampleAuxiliaryInformationOffset auxiliary_offset;
  SampleEncryption sample_encryption;
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

  bool reference_type = false;
  uint32_t referenced_size = 0u;
  uint32_t subsegment_duration = 0u;
  bool starts_with_sap = false;
  SAPType sap_type = TypeUnknown;
  uint32_t sap_delta_time = 0u;
  // We add this field to keep track of earliest_presentation_time in this
  // subsegment. It is not part of SegmentReference.
  uint64_t earliest_presentation_time = 0u;
};

struct SegmentIndex : FullBox {
  DECLARE_BOX_METHODS(SegmentIndex);

  uint32_t reference_id = 0u;
  uint32_t timescale = 0u;
  uint64_t earliest_presentation_time = 0u;
  uint64_t first_offset = 0u;
  std::vector<SegmentReference> references;
};

// The actual data is parsed and written separately.
struct MediaData : Box {
  DECLARE_BOX_METHODS(MediaData);

  uint32_t data_size = 0u;
};

// Using negative value as "not set". It is very unlikely that 2^31 cues happen
// at once.
const int kCueSourceIdNotSet = -1;

struct CueSourceIDBox : Box {
  DECLARE_BOX_METHODS(CueSourceIDBox);

  int32_t source_id = kCueSourceIdNotSet;
};

struct CueTimeBox : Box {
  DECLARE_BOX_METHODS(CueTimeBox);

  std::string cue_current_time;
};

struct CueIDBox : Box {
  DECLARE_BOX_METHODS(CueIDBox);

  std::string cue_id;
};

struct CueSettingsBox : Box {
  DECLARE_BOX_METHODS(CueSettingsBox);

  std::string settings;
};

struct CuePayloadBox : Box {
  DECLARE_BOX_METHODS(CuePayloadBox);

  std::string cue_text;
};

struct VTTEmptyCueBox : Box {
  DECLARE_BOX_METHODS(VTTEmptyCueBox);
};

struct VTTAdditionalTextBox : Box {
  DECLARE_BOX_METHODS(VTTAdditionalTextBox);

  std::string cue_additional_text;
};

struct VTTCueBox : Box {
  DECLARE_BOX_METHODS(VTTCueBox);

  CueSourceIDBox cue_source_id;
  CueIDBox cue_id;
  CueTimeBox cue_time;
  CueSettingsBox cue_settings;
  CuePayloadBox cue_payload;
};

#undef DECLARE_BOX

}  // namespace mp4
}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_FORMATS_MP4_BOX_DEFINITIONS_H_
