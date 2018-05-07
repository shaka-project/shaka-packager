// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd
//
// Overloads operator== for mp4 boxes, mainly used for testing.

#ifndef PACKAGER_MEDIA_FORMATS_MP4_BOX_DEFINITIONS_COMPARISON_H_
#define PACKAGER_MEDIA_FORMATS_MP4_BOX_DEFINITIONS_COMPARISON_H_

#include "packager/media/formats/mp4/box_definitions.h"

namespace shaka {
namespace media {

inline bool operator==(const SubsampleEntry& lhs, const SubsampleEntry& rhs) {
  return lhs.clear_bytes == rhs.clear_bytes &&
         lhs.cipher_bytes == rhs.cipher_bytes;
}

namespace mp4 {

inline bool operator==(const FileType& lhs, const FileType& rhs) {
  return lhs.major_brand == rhs.major_brand &&
         lhs.minor_version == rhs.minor_version &&
         lhs.compatible_brands == rhs.compatible_brands;
}

inline bool operator==(const ProtectionSystemSpecificHeader& lhs,
                       const ProtectionSystemSpecificHeader& rhs) {
  return lhs.raw_box == rhs.raw_box;
}

inline bool operator==(const SampleAuxiliaryInformationOffset& lhs,
                       const SampleAuxiliaryInformationOffset& rhs) {
  return lhs.offsets == rhs.offsets;
}

inline bool operator==(const SampleAuxiliaryInformationSize& lhs,
                       const SampleAuxiliaryInformationSize& rhs) {
  return lhs.default_sample_info_size == rhs.default_sample_info_size &&
         lhs.sample_count == rhs.sample_count &&
         lhs.sample_info_sizes == rhs.sample_info_sizes;
}

inline bool operator==(const SampleEncryptionEntry& lhs,
                       const SampleEncryptionEntry& rhs) {
  return lhs.initialization_vector == rhs.initialization_vector &&
         lhs.subsamples == rhs.subsamples;
}

inline bool operator==(const SampleEncryption& lhs,
                       const SampleEncryption& rhs) {
  return lhs.iv_size == rhs.iv_size &&
         lhs.sample_encryption_entries == rhs.sample_encryption_entries;
}

inline bool operator==(const OriginalFormat& lhs, const OriginalFormat& rhs) {
  return lhs.format == rhs.format;
}

inline bool operator==(const SchemeType& lhs, const SchemeType& rhs) {
  return lhs.type == rhs.type && lhs.version == rhs.version;
}

inline bool operator==(const TrackEncryption& lhs, const TrackEncryption& rhs) {
  return lhs.default_is_protected == rhs.default_is_protected &&
         lhs.default_per_sample_iv_size == rhs.default_per_sample_iv_size &&
         lhs.default_kid == rhs.default_kid &&
         lhs.default_crypt_byte_block == rhs.default_crypt_byte_block &&
         lhs.default_skip_byte_block == rhs.default_skip_byte_block &&
         lhs.default_constant_iv == rhs.default_constant_iv;
}

inline bool operator==(const SchemeInfo& lhs, const SchemeInfo& rhs) {
  return lhs.track_encryption == rhs.track_encryption;
}

inline bool operator==(const ProtectionSchemeInfo& lhs,
                       const ProtectionSchemeInfo& rhs) {
  return lhs.format == rhs.format && lhs.type == rhs.type &&
         lhs.info == rhs.info;
}

inline bool operator==(const MovieHeader& lhs, const MovieHeader& rhs) {
  return lhs.creation_time == rhs.creation_time &&
         lhs.modification_time == rhs.modification_time &&
         lhs.timescale == rhs.timescale && lhs.duration == rhs.duration &&
         lhs.rate == rhs.rate && lhs.volume == rhs.volume &&
         lhs.next_track_id == rhs.next_track_id;
}

inline bool operator==(const TrackHeader& lhs, const TrackHeader& rhs) {
  return lhs.creation_time == rhs.creation_time &&
         lhs.modification_time == rhs.modification_time &&
         lhs.track_id == rhs.track_id && lhs.duration == rhs.duration &&
         lhs.layer == rhs.layer && lhs.alternate_group == rhs.alternate_group &&
         lhs.volume == rhs.volume && lhs.width == rhs.width &&
         lhs.height == rhs.height;
}

inline bool operator==(const SampleDescription& lhs,
                       const SampleDescription& rhs) {
  return lhs.type == rhs.type && lhs.video_entries == rhs.video_entries &&
         lhs.audio_entries == rhs.audio_entries;
}

inline bool operator==(const DecodingTime& lhs, const DecodingTime& rhs) {
  return lhs.sample_count == rhs.sample_count &&
         lhs.sample_delta == rhs.sample_delta;
}

inline bool operator==(const DecodingTimeToSample& lhs,
                       const DecodingTimeToSample& rhs) {
  return lhs.decoding_time == rhs.decoding_time;
}

inline bool operator==(const CompositionOffset& lhs,
                       const CompositionOffset& rhs) {
  return lhs.sample_count == rhs.sample_count &&
         lhs.sample_offset == rhs.sample_offset;
}

inline bool operator==(const CompositionTimeToSample& lhs,
                       const CompositionTimeToSample& rhs) {
  return lhs.composition_offset == rhs.composition_offset;
}

inline bool operator==(const ChunkInfo& lhs, const ChunkInfo& rhs) {
  return lhs.first_chunk == rhs.first_chunk &&
         lhs.samples_per_chunk == rhs.samples_per_chunk &&
         lhs.sample_description_index == rhs.sample_description_index;
}

inline bool operator==(const SampleToChunk& lhs, const SampleToChunk& rhs) {
  return lhs.chunk_info == rhs.chunk_info;
}

inline bool operator==(const SampleSize& lhs, const SampleSize& rhs) {
  return lhs.sample_size == rhs.sample_size &&
         lhs.sample_count == rhs.sample_count && lhs.sizes == rhs.sizes;
}

inline bool operator==(const CompactSampleSize& lhs,
                       const CompactSampleSize& rhs) {
  return lhs.field_size == rhs.field_size && lhs.sizes == rhs.sizes;
}

inline bool operator==(const ChunkLargeOffset& lhs,
                       const ChunkLargeOffset& rhs) {
  return lhs.offsets == rhs.offsets;
}

inline bool operator==(const SyncSample& lhs, const SyncSample& rhs) {
  return lhs.sample_number == rhs.sample_number;
}

inline bool operator==(const CencSampleEncryptionInfoEntry& lhs,
                       const CencSampleEncryptionInfoEntry& rhs) {
  return lhs.is_protected == rhs.is_protected &&
         lhs.per_sample_iv_size == rhs.per_sample_iv_size &&
         lhs.key_id == rhs.key_id &&
         lhs.crypt_byte_block == rhs.crypt_byte_block &&
         lhs.skip_byte_block == rhs.skip_byte_block &&
         lhs.constant_iv == rhs.constant_iv;
}

inline bool operator==(const AudioRollRecoveryEntry& lhs,
                       const AudioRollRecoveryEntry& rhs) {
  return lhs.roll_distance == rhs.roll_distance;
}

inline bool operator==(const SampleGroupDescription& lhs,
                       const SampleGroupDescription& rhs) {
  return lhs.grouping_type == rhs.grouping_type &&
         lhs.cenc_sample_encryption_info_entries ==
             rhs.cenc_sample_encryption_info_entries &&
         lhs.audio_roll_recovery_entries == rhs.audio_roll_recovery_entries;
}

inline bool operator==(const SampleToGroupEntry& lhs,
                       const SampleToGroupEntry& rhs) {
  return lhs.sample_count == rhs.sample_count &&
         lhs.group_description_index == rhs.group_description_index;
}

inline bool operator==(const SampleToGroup& lhs,
                       const SampleToGroup& rhs) {
  return lhs.grouping_type == rhs.grouping_type &&
         lhs.grouping_type_parameter == rhs.grouping_type_parameter &&
         lhs.entries == rhs.entries;
}

inline bool operator==(const SampleTable& lhs, const SampleTable& rhs) {
  return lhs.description == rhs.description &&
         lhs.decoding_time_to_sample == rhs.decoding_time_to_sample &&
         lhs.composition_time_to_sample == rhs.composition_time_to_sample &&
         lhs.sample_to_chunk == rhs.sample_to_chunk &&
         lhs.sample_size == rhs.sample_size &&
         lhs.chunk_large_offset == rhs.chunk_large_offset &&
         lhs.sync_sample == rhs.sync_sample &&
         lhs.sample_group_descriptions == rhs.sample_group_descriptions &&
         lhs.sample_to_groups == rhs.sample_to_groups;
}

inline bool operator==(const EditListEntry& lhs, const EditListEntry& rhs) {
  return lhs.segment_duration == rhs.segment_duration &&
         lhs.media_time == rhs.media_time &&
         lhs.media_rate_integer == rhs.media_rate_integer &&
         lhs.media_rate_fraction == rhs.media_rate_fraction;
}

inline bool operator==(const EditList& lhs, const EditList& rhs) {
  return lhs.edits == rhs.edits;
}

inline bool operator==(const Edit& lhs, const Edit& rhs) {
  return lhs.list == rhs.list;
}

inline bool operator==(const HandlerReference& lhs,
                       const HandlerReference& rhs) {
  return lhs.handler_type == rhs.handler_type;
}

inline bool operator==(const Language& lhs,
                       const Language& rhs) {
  return lhs.code == rhs.code;
}

inline bool operator==(const ID3v2& lhs, const ID3v2& rhs) {
  return lhs.language == rhs.language && lhs.id3v2_data == rhs.id3v2_data;
}

inline bool operator==(const Metadata& lhs, const Metadata& rhs) {
  return lhs.handler == rhs.handler && lhs.id3v2 == rhs.id3v2;
}

inline bool operator==(const CodecConfiguration& lhs,
                       const CodecConfiguration& rhs) {
  return lhs.box_type == rhs.box_type && lhs.data == rhs.data;
}

inline bool operator==(const PixelAspectRatio& lhs,
                       const PixelAspectRatio& rhs) {
  return lhs.h_spacing == rhs.h_spacing && lhs.v_spacing == rhs.v_spacing;
}

inline bool operator==(const VideoSampleEntry& lhs,
                       const VideoSampleEntry& rhs) {
  return lhs.format == rhs.format &&
         lhs.data_reference_index == rhs.data_reference_index &&
         lhs.width == rhs.width && lhs.height == rhs.height &&
         lhs.pixel_aspect == rhs.pixel_aspect && lhs.sinf == rhs.sinf &&
         lhs.codec_configuration == rhs.codec_configuration;
}

inline bool operator==(const ESDescriptor& lhs, const ESDescriptor& rhs) {
  return lhs.esid() == rhs.esid() && lhs.object_type() == rhs.object_type() &&
         lhs.max_bitrate() == rhs.max_bitrate() &&
         lhs.avg_bitrate() == rhs.avg_bitrate() &&
         lhs.decoder_specific_info() == rhs.decoder_specific_info();
}

inline bool operator==(const ElementaryStreamDescriptor& lhs,
                       const ElementaryStreamDescriptor& rhs) {
  return lhs.es_descriptor == rhs.es_descriptor;
}

inline bool operator==(const DTSSpecific& lhs, const DTSSpecific& rhs) {
  return lhs.sampling_frequency == rhs.sampling_frequency &&
         lhs.max_bitrate == rhs.max_bitrate &&
         lhs.avg_bitrate == rhs.avg_bitrate &&
         lhs.pcm_sample_depth == rhs.pcm_sample_depth &&
         lhs.extra_data == rhs.extra_data;
}

inline bool operator==(const AC3Specific& lhs, const AC3Specific& rhs) {
  return lhs.data == rhs.data;
}

inline bool operator==(const EC3Specific& lhs, const EC3Specific& rhs) {
  return lhs.data == rhs.data;
}

inline bool operator==(const OpusSpecific& lhs, const OpusSpecific& rhs) {
  return lhs.opus_identification_header == rhs.opus_identification_header &&
         lhs.preskip == rhs.preskip;
}

inline bool operator==(const AudioSampleEntry& lhs,
                       const AudioSampleEntry& rhs) {
  return lhs.format == rhs.format &&
         lhs.data_reference_index == rhs.data_reference_index &&
         lhs.channelcount == rhs.channelcount &&
         lhs.samplesize == rhs.samplesize && lhs.samplerate == rhs.samplerate &&
         lhs.sinf == rhs.sinf && lhs.esds == rhs.esds && lhs.ddts == rhs.ddts &&
         lhs.dac3 == rhs.dac3 && lhs.dec3 == rhs.dec3 && lhs.dops == rhs.dops;
}

inline bool operator==(const WebVTTConfigurationBox& lhs,
                       const WebVTTConfigurationBox& rhs) {
  return lhs.config == rhs.config;
}

inline bool operator==(const WebVTTSourceLabelBox& lhs,
                       const WebVTTSourceLabelBox& rhs) {
  return lhs.source_label == rhs.source_label;
}

inline bool operator==(const TextSampleEntry& lhs, const TextSampleEntry& rhs) {
  return lhs.config == rhs.config && lhs.label == rhs.label;
}

inline bool operator==(const MediaHeader& lhs, const MediaHeader& rhs) {
  return lhs.creation_time == rhs.creation_time &&
         lhs.modification_time == rhs.modification_time &&
         lhs.timescale == rhs.timescale && lhs.duration == rhs.duration &&
         lhs.language == rhs.language;
}

inline bool operator==(const VideoMediaHeader& lhs,
                       const VideoMediaHeader& rhs) {
  return lhs.graphicsmode == rhs.graphicsmode &&
         lhs.opcolor_red == rhs.opcolor_red &&
         lhs.opcolor_green == rhs.opcolor_green &&
         lhs.opcolor_blue == rhs.opcolor_blue;
}

inline bool operator==(const SoundMediaHeader& lhs,
                       const SoundMediaHeader& rhs) {
  return lhs.balance == rhs.balance;
}

inline bool operator==(const SubtitleMediaHeader& lhs,
                       const SubtitleMediaHeader& rhs) {
  return true;
}

inline bool operator==(const DataEntryUrl& lhs, const DataEntryUrl& rhs) {
  return lhs.flags == rhs.flags && lhs.location == rhs.location;
}

inline bool operator==(const DataReference& lhs, const DataReference& rhs) {
  return lhs.data_entry == rhs.data_entry;
}

inline bool operator==(const DataInformation& lhs, const DataInformation& rhs) {
  return lhs.dref == rhs.dref;
}

inline bool operator==(const MediaInformation& lhs,
                       const MediaInformation& rhs) {
  return lhs.dinf == rhs.dinf && lhs.sample_table == rhs.sample_table &&
         lhs.vmhd == rhs.vmhd && lhs.smhd == rhs.smhd;
}

inline bool operator==(const Media& lhs, const Media& rhs) {
  return lhs.header == rhs.header && lhs.handler == rhs.handler &&
         lhs.information == rhs.information;
}

inline bool operator==(const Track& lhs, const Track& rhs) {
  return lhs.header == rhs.header && lhs.media == rhs.media &&
         lhs.edit == rhs.edit && lhs.sample_encryption == rhs.sample_encryption;
}

inline bool operator==(const MovieExtendsHeader& lhs,
                       const MovieExtendsHeader& rhs) {
  return lhs.fragment_duration == rhs.fragment_duration;
}

inline bool operator==(const TrackExtends& lhs, const TrackExtends& rhs) {
  return lhs.track_id == rhs.track_id &&
         lhs.default_sample_description_index ==
             rhs.default_sample_description_index &&
         lhs.default_sample_duration == rhs.default_sample_duration &&
         lhs.default_sample_size == rhs.default_sample_size &&
         lhs.default_sample_flags == rhs.default_sample_flags;
}

inline bool operator==(const MovieExtends& lhs, const MovieExtends& rhs) {
  return lhs.header == rhs.header && lhs.tracks == rhs.tracks;
}

inline bool operator==(const Movie& lhs, const Movie& rhs) {
  return lhs.header == rhs.header && lhs.extends == rhs.extends &&
         lhs.tracks == rhs.tracks && lhs.pssh == rhs.pssh;
}

inline bool operator==(const TrackFragmentDecodeTime& lhs,
                       const TrackFragmentDecodeTime& rhs) {
  return lhs.decode_time == rhs.decode_time;
}

inline bool operator==(const MovieFragmentHeader& lhs,
                       const MovieFragmentHeader& rhs) {
  return lhs.sequence_number == rhs.sequence_number;
}

inline bool operator==(const TrackFragmentHeader& lhs,
                       const TrackFragmentHeader& rhs) {
  return lhs.flags == rhs.flags && lhs.track_id == rhs.track_id &&
         lhs.sample_description_index == rhs.sample_description_index &&
         lhs.default_sample_duration == rhs.default_sample_duration &&
         lhs.default_sample_size == rhs.default_sample_size &&
         lhs.default_sample_flags == rhs.default_sample_flags;
}

inline bool operator==(const TrackFragmentRun& lhs,
                       const TrackFragmentRun& rhs) {
  return lhs.flags == rhs.flags && lhs.sample_count == rhs.sample_count &&
         lhs.data_offset == rhs.data_offset &&
         lhs.sample_flags == rhs.sample_flags &&
         lhs.sample_sizes == rhs.sample_sizes &&
         lhs.sample_durations == rhs.sample_durations &&
         lhs.sample_composition_time_offsets ==
             rhs.sample_composition_time_offsets;
}

inline bool operator==(const TrackFragment& lhs, const TrackFragment& rhs) {
  return lhs.header == rhs.header && lhs.runs == rhs.runs &&
         lhs.decode_time == rhs.decode_time &&
         lhs.auxiliary_offset == rhs.auxiliary_offset &&
         lhs.auxiliary_size == rhs.auxiliary_size &&
         lhs.sample_encryption == rhs.sample_encryption;
}

inline bool operator==(const MovieFragment& lhs, const MovieFragment& rhs) {
  return lhs.header == rhs.header && lhs.tracks == rhs.tracks &&
         lhs.pssh == rhs.pssh;
}

inline bool operator==(const SegmentReference& lhs,
                       const SegmentReference& rhs) {
  return lhs.reference_type == rhs.reference_type &&
         lhs.referenced_size == rhs.referenced_size &&
         lhs.subsegment_duration == rhs.subsegment_duration &&
         lhs.starts_with_sap == rhs.starts_with_sap &&
         lhs.sap_type == rhs.sap_type &&
         lhs.sap_delta_time == rhs.sap_delta_time;
}

inline bool operator==(const SegmentIndex& lhs, const SegmentIndex& rhs) {
  return lhs.reference_id == rhs.reference_id &&
         lhs.timescale == rhs.timescale &&
         lhs.earliest_presentation_time == rhs.earliest_presentation_time &&
         lhs.first_offset == rhs.first_offset &&
         lhs.references == rhs.references;
}

inline bool operator==(const CueSourceIDBox& lhs,
                       const CueSourceIDBox& rhs) {
  return lhs.source_id == rhs.source_id;
}

inline bool operator==(const CueTimeBox& lhs,
                       const CueTimeBox& rhs) {
  return lhs.cue_current_time == rhs.cue_current_time;
}

inline bool operator==(const CueIDBox& lhs,
                       const CueIDBox& rhs) {
  return lhs.cue_id == rhs.cue_id;
}

inline bool operator==(const CueSettingsBox& lhs,
                       const CueSettingsBox& rhs) {
  return lhs.settings == rhs.settings;
}

inline bool operator==(const CuePayloadBox& lhs,
                       const CuePayloadBox& rhs) {
  return lhs.cue_text == rhs.cue_text;
}

inline bool operator==(const VTTEmptyCueBox& lhs, const VTTEmptyCueBox& rhs) {
  return true;
}

inline bool operator==(const VTTAdditionalTextBox& lhs,
                       const VTTAdditionalTextBox& rhs) {
  return lhs.cue_additional_text == rhs.cue_additional_text;
}

inline bool operator==(const VTTCueBox& lhs,
                       const VTTCueBox& rhs) {
  return lhs.cue_source_id == rhs.cue_source_id && lhs.cue_id == rhs.cue_id &&
         lhs.cue_time == rhs.cue_time && lhs.cue_settings == rhs.cue_settings &&
         lhs.cue_payload == rhs.cue_payload;
}

}  // namespace mp4
}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_FORMATS_MP4_BOX_DEFINITIONS_COMPARISON_H_
