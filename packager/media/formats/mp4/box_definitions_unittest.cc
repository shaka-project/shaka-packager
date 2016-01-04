// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <gtest/gtest.h>

#include <limits>

#include "packager/base/memory/scoped_ptr.h"
#include "packager/media/base/buffer_writer.h"
#include "packager/media/formats/mp4/box_definitions.h"
#include "packager/media/formats/mp4/box_definitions_comparison.h"
#include "packager/media/formats/mp4/box_reader.h"

namespace edash_packager {
namespace media {
namespace mp4 {
namespace {
const uint8_t kData8Bytes[] = {3, 4, 5, 6, 7, 8, 9, 0};
const uint8_t kData16Bytes[] = {8, 7, 6, 5, 4, 3, 2, 1, 1, 2, 3, 4, 5, 6, 7, 8};
const uint8_t kData4[] = {1, 5, 4, 3, 15};
const uint8_t kData8[] = {1, 8, 42, 98, 156};
const uint16_t kData16[] = {1, 15, 45, 768, 60000};
const uint32_t kData32[] = {1, 24, 99, 1234, 9000000};
const uint64_t kData64[] = {1, 9000000, 12345678901234ULL, 56780909090900ULL};
const TrackType kSampleDescriptionTrackType = kVideo;

// 4-byte FourCC + 4-bytes size.
const uint32_t kBoxSize = 8;
}  // namespace

template <typename T>
class BoxDefinitionsTestGeneral : public testing::Test {
 public:
  BoxDefinitionsTestGeneral() : buffer_(new BufferWriter) {}

  BoxReader* CreateReader() {
    // Create a fake skip box contains the buffer and Write it.
    BufferWriter buffer;
    buffer.Swap(buffer_.get());
    uint32_t skip_box_size = buffer.Size() + kBoxSize;
    buffer_->AppendInt(skip_box_size);
    buffer_->AppendInt(static_cast<uint32_t>(FOURCC_SKIP));
    buffer_->AppendBuffer(buffer);
    bool err = false;
    return BoxReader::ReadTopLevelBox(buffer_->Buffer(), buffer_->Size(), &err);
  }

  bool ReadBack(Box* box) {
    scoped_ptr<BoxReader> reader(CreateReader());
    RCHECK(reader->ScanChildren() && reader->ReadChild(box));
    return true;
  }

  // FourCC for VideoSampleEntry is not a constant, e.g. could be avc1, or encv.
  bool ReadBack(VideoSampleEntry* video) {
    scoped_ptr<BoxReader> reader(CreateReader());
    std::vector<VideoSampleEntry> video_entries;
    RCHECK(reader->ReadAllChildren(&video_entries));
    RCHECK(video_entries.size() == 1);
    *video = video_entries[0];
    return true;
  }

  // FourCC for AudioSampleEntry is not a constant, e.g. could be mp4a, or enca.
  bool ReadBack(AudioSampleEntry* audio) {
    scoped_ptr<BoxReader> reader(CreateReader());
    std::vector<AudioSampleEntry> audio_entries;
    RCHECK(reader->ReadAllChildren(&audio_entries));
    RCHECK(audio_entries.size() == 1);
    *audio = audio_entries[0];
    return true;
  }

  // SampleDescription cannot parse on its own. Its type parameter should
  // be preset before scanning the box.
  bool ReadBack(SampleDescription* stsd) {
    stsd->type = kSampleDescriptionTrackType;
    scoped_ptr<BoxReader> reader(CreateReader());
    RCHECK(reader->ScanChildren() && reader->ReadChild(stsd));
    return true;
  }

  // SampleTable contains SampleDescription, which cannot parse on its own.
  bool ReadBack(SampleTable* stbl) {
    stbl->description.type = kSampleDescriptionTrackType;
    scoped_ptr<BoxReader> reader(CreateReader());
    RCHECK(reader->ScanChildren() && reader->ReadChild(stbl));
    return true;
  }

  // MediaInformation contains SampleDescription, which cannot parse on its own.
  bool ReadBack(MediaInformation* minf) {
    minf->sample_table.description.type = kSampleDescriptionTrackType;
    scoped_ptr<BoxReader> reader(CreateReader());
    RCHECK(reader->ScanChildren() && reader->ReadChild(minf));
    return true;
  }

  // Fill the box with sample data.
  void Fill(Box* box) {}

  // Modify the box with another set of data.
  void Modify(Box* box) {}

  // Is this box optional?
  bool IsOptional(const Box* box) { return false; }

  // Non-full box does not have version field.
  uint8_t GetAndClearVersion(Box* box) { return 0; }

  // Get full box version and then reset it to 0.
  uint8_t GetAndClearVersion(FullBox* full_box) {
    uint8_t version = full_box->version;
    full_box->version = 0;
    return version;
  }

  void Fill(FileType* ftyp) {
    ftyp->major_brand = FOURCC_DASH;
    ftyp->minor_version = 567;
    ftyp->compatible_brands.push_back(FOURCC_ISO6);
    ftyp->compatible_brands.push_back(FOURCC_MP41);
    ftyp->compatible_brands.push_back(FOURCC_AVC1);
  }

  void Modify(FileType* ftyp) {
    ftyp->major_brand = FOURCC_MP41;
    ftyp->compatible_brands.clear();
    ftyp->compatible_brands.push_back(FOURCC_DASH);
  }

  void Fill(ProtectionSystemSpecificHeader* pssh) {
    pssh->system_id.assign(kData16Bytes,
                           kData16Bytes + arraysize(kData16Bytes));
    pssh->data.assign(kData8, kData8 + arraysize(kData8));
  }

  void Modify(ProtectionSystemSpecificHeader* pssh) {
    pssh->system_id[2] *= 3;
    pssh->data.assign(kData4, kData4 + arraysize(kData4));
  }

  void Fill(SampleAuxiliaryInformationOffset* saio) {
    saio->offsets.assign(kData32, kData32 + arraysize(kData32));
  }

  void Modify(SampleAuxiliaryInformationOffset* saio) {
    saio->offsets.push_back(23);
  }

  void Fill(SampleAuxiliaryInformationSize* saiz) {
    saiz->default_sample_info_size = 0;
    saiz->sample_info_sizes.assign(kData8, kData8 + arraysize(kData8));
    saiz->sample_count = arraysize(kData8);
  }

  void Modify(SampleAuxiliaryInformationSize* saiz) {
    saiz->default_sample_info_size = 15;
    saiz->sample_info_sizes.clear();
  }

  void Fill(SampleEncryption* senc) {
    senc->iv_size = 8;
    senc->flags = SampleEncryption::kUseSubsampleEncryption;
    senc->sample_encryption_entries.resize(2);
    senc->sample_encryption_entries[0].initialization_vector.assign(
        kData8Bytes, kData8Bytes + arraysize(kData8Bytes));
    senc->sample_encryption_entries[0].subsamples.resize(2);
    senc->sample_encryption_entries[0].subsamples[0].clear_bytes = 17;
    senc->sample_encryption_entries[0].subsamples[0].cipher_bytes = 3456;
    senc->sample_encryption_entries[0].subsamples[1].clear_bytes = 1543;
    senc->sample_encryption_entries[0].subsamples[1].cipher_bytes = 0;
    senc->sample_encryption_entries[1] = senc->sample_encryption_entries[0];
    senc->sample_encryption_entries[1].subsamples[0].clear_bytes = 0;
    senc->sample_encryption_entries[1].subsamples[0].cipher_bytes = 15;
    senc->sample_encryption_entries[1].subsamples[1].clear_bytes = 1988;
    senc->sample_encryption_entries[1].subsamples[1].cipher_bytes = 8765;
  }

  void Modify(SampleEncryption* senc) {
    senc->flags = 0;
    senc->sample_encryption_entries.resize(1);
    senc->sample_encryption_entries[0].subsamples.clear();
  }

  void Fill(OriginalFormat* frma) { frma->format = FOURCC_AVC1; }

  void Modify(OriginalFormat* frma) { frma->format = FOURCC_MP4A; }

  void Fill(SchemeType* schm) {
    schm->type = FOURCC_CENC;
    schm->version = 12344;
  }

  void Modify(SchemeType* schm) { schm->version = 123; }

  void Fill(TrackEncryption* tenc) {
    tenc->is_encrypted = true;
    tenc->default_iv_size = 8;
    tenc->default_kid.assign(kData16Bytes,
                             kData16Bytes + arraysize(kData16Bytes));
  }

  void Modify(TrackEncryption* tenc) {
    tenc->is_encrypted = false;
    tenc->default_iv_size = 0;
  }

  void Fill(SchemeInfo* schi) { Fill(&schi->track_encryption); }

  void Modify(SchemeInfo* schi) { Modify(&schi->track_encryption); }

  void Fill(ProtectionSchemeInfo* sinf) {
    Fill(&sinf->format);
    Fill(&sinf->type);
    Fill(&sinf->info);
  }

  void Modify(ProtectionSchemeInfo* sinf) {
    Modify(&sinf->type);
    Modify(&sinf->info);
  }

  void Fill(MovieHeader* mvhd) {
    mvhd->creation_time = 1234;
    mvhd->modification_time = 2456;
    mvhd->timescale = 48000;
    mvhd->duration = 96000;
    mvhd->rate = 0x010000;
    mvhd->volume = 0x0100;
    mvhd->next_track_id = 1;
    mvhd->version = 0;
  }

  void Modify(MovieHeader* mvhd) {
    mvhd->duration = 234141324123ULL;
    mvhd->next_track_id = 3;
    mvhd->version = 1;
  }

  void Fill(TrackHeader* tkhd) {
    tkhd->creation_time = 34523443;
    tkhd->modification_time = 34533443;
    tkhd->track_id = 2;
    tkhd->duration = 96000;
    tkhd->layer = 1;
    tkhd->alternate_group = 2;
    tkhd->volume = 0;
    tkhd->width = 800;
    tkhd->height = 600;
    tkhd->version = 0;
  }

  void Modify(TrackHeader* tkhd) {
    tkhd->modification_time = 345388873443ULL;
    tkhd->volume = 0x0100;
    tkhd->width = 0;
    tkhd->height = 0;
    tkhd->version = 1;
  }

  void Fill(EditList* elst) {
    elst->edits.resize(2);
    elst->edits[0].segment_duration = 100;
    elst->edits[0].media_time = -1;
    elst->edits[0].media_rate_integer = 1;
    elst->edits[0].media_rate_fraction = 0;
    elst->edits[1].segment_duration = 300;
    elst->edits[1].media_time = 0;
    elst->edits[1].media_rate_integer = 1;
    elst->edits[1].media_rate_fraction = 0;
    elst->version = 0;
  }

  void Modify(EditList* elst) {
    elst->edits.resize(1);
    elst->edits[0].segment_duration = 0;
    elst->edits[0].media_time = 20364563456LL;
    elst->version = 1;
  }

  void Fill(Edit* edts) { Fill(&edts->list); }

  void Modify(Edit* edts) { Modify(&edts->list); }

  void Fill(HandlerReference* hdlr) {
    hdlr->type = kSampleDescriptionTrackType;
  }

  void Modify(HandlerReference* hdlr) { hdlr->type = kAudio; }

  void Fill(PixelAspectRatio* pasp) {
    pasp->h_spacing = 5;
    pasp->v_spacing = 8;
  }

  void Modify(PixelAspectRatio* pasp) { pasp->v_spacing *= 8; }

  void Fill(CodecConfigurationRecord* codec_config_record) {
    const uint8_t kAvccData[] = {
        0x01, 0x64, 0x00, 0x1f, 0xff, 0xe1, 0x00, 0x18, 0x67, 0x64, 0x00,
        0x1f, 0xac, 0xd9, 0x40, 0x50, 0x05, 0xbb, 0x01, 0x10, 0x00, 0x00,
        0x3e, 0x90, 0x00, 0x0e, 0xa6, 0x00, 0xf1, 0x83, 0x19, 0x60, 0x01,
        0x00, 0x06, 0x68, 0xeb, 0xe3, 0xcb, 0x22, 0xc0};
    codec_config_record->data.assign(kAvccData,
                                     kAvccData + arraysize(kAvccData));
  }

  void Modify(CodecConfigurationRecord* codec_config_record) {
    const uint8_t kAvccData[] = {
        0x01, 0x64, 0x00, 0x1e, 0xff, 0xe1, 0x00, 0x19, 0x67, 0x64, 0x00,
        0x1e, 0xac, 0xd9, 0x40, 0xa0, 0x2f, 0xf9, 0x70, 0x11, 0x00, 0x00,
        0x03, 0x03, 0xe9, 0x00, 0x00, 0xea, 0x60, 0x0f, 0x16, 0x2d, 0x96,
        0x01, 0x00, 0x05, 0x68, 0xeb, 0xec, 0xb2, 0x2c};
    codec_config_record->data.assign(kAvccData,
                                     kAvccData + arraysize(kAvccData));
  }

  void Fill(VideoSampleEntry* encv) {
    encv->format = FOURCC_ENCV;
    encv->data_reference_index = 1;
    encv->width = 800;
    encv->height = 600;
    Fill(&encv->pixel_aspect);
    Fill(&encv->sinf);
    Fill(&encv->codec_config_record);
  }

  void Modify(VideoSampleEntry* encv) {
    encv->height += 600;
    Modify(&encv->codec_config_record);
  }

  void Fill(ElementaryStreamDescriptor* esds) {
    const uint8_t kDecoderSpecificInfo[] = {18, 16};
    esds->es_descriptor.set_esid(1);
    esds->es_descriptor.set_object_type(kISO_14496_3);
    std::vector<uint8_t> decoder_specific_info(
        kDecoderSpecificInfo,
        kDecoderSpecificInfo + sizeof(kDecoderSpecificInfo));
    esds->es_descriptor.set_decoder_specific_info(decoder_specific_info);
  }

  void Modify(ElementaryStreamDescriptor* esds) {
    esds->es_descriptor.set_esid(2);
  }

  void Fill(DTSSpecific* ddts) {
    const uint8_t kDdtsExtraData[] = {0xe4, 0x7c, 0, 4, 0, 0x0f, 0};
    ddts->max_bitrate = 768000;
    ddts->avg_bitrate = 768000;
    ddts->sampling_frequency = 48000;
    ddts->pcm_sample_depth = 16;
    ddts->extra_data.assign(kDdtsExtraData,
                            kDdtsExtraData + arraysize(kDdtsExtraData));
  }

  void Modify(DTSSpecific* ddts) {
    ddts->pcm_sample_depth = 24;
  }

  void Fill(AudioSampleEntry* enca) {
    enca->format = FOURCC_ENCA;
    enca->data_reference_index = 2;
    enca->channelcount = 5;
    enca->samplesize = 16;
    enca->samplerate = 44100;
    Fill(&enca->sinf);
    Fill(&enca->esds);
  }

  void Modify(AudioSampleEntry* enca) { enca->channelcount = 2; }

  void Fill(WebVTTConfigurationBox* vttc) {
    vttc->config = "WEBVTT";
  }

  void Modify(WebVTTConfigurationBox* vttc) {
    vttc->config = "WEBVTT\n"
                   "Region: id=someting width=40\% lines=3";
  }

  void Fill(WebVTTSourceLabelBox* vlab) {
    vlab->source_label = "some_label";
  }

  void Modify(WebVTTSourceLabelBox* vlab) {
    vlab->source_label = "another_label";
  }

  void Fill(WVTTSampleEntry* wvtt) {
    Fill(&wvtt->config);
    Fill(&wvtt->label);
  }

  void Modify(WVTTSampleEntry* wvtt) {
    Modify(&wvtt->config);
    Modify(&wvtt->label);
  }

  void Fill(SampleDescription* stsd) {
    stsd->type = kSampleDescriptionTrackType;
    stsd->video_entries.resize(1);
    Fill(&stsd->video_entries[0]);
  }

  void Fill(DecodingTimeToSample* stts) {
    stts->decoding_time.resize(2);
    stts->decoding_time[0].sample_count = 3;
    stts->decoding_time[0].sample_delta = 5;
    stts->decoding_time[1].sample_count = 2;
    stts->decoding_time[1].sample_delta = 9;
  }

  void Modify(DecodingTimeToSample* stts) {
    stts->decoding_time.resize(3);
    stts->decoding_time[2].sample_count = 9;
    stts->decoding_time[2].sample_delta = 4;
  }

  void Fill(CompositionTimeToSample* ctts) {
    ctts->composition_offset.resize(2);
    ctts->composition_offset[0].sample_count = 3;
    ctts->composition_offset[0].sample_offset = 5;
    ctts->composition_offset[1].sample_count = 2;
    ctts->composition_offset[1].sample_offset = 9;
    ctts->version = 0;
  }

  void Modify(CompositionTimeToSample* ctts) {
    ctts->composition_offset.resize(1);
    ctts->composition_offset[0].sample_count = 6;
    ctts->composition_offset[0].sample_offset = -9;
    ctts->version = 1;
  }

  void Fill(SampleToChunk* stsc) {
    stsc->chunk_info.resize(2);
    stsc->chunk_info[0].first_chunk = 1;
    stsc->chunk_info[0].samples_per_chunk = 5;
    stsc->chunk_info[0].sample_description_index = 0;
    stsc->chunk_info[1].first_chunk = 5;
    stsc->chunk_info[1].samples_per_chunk = 2;
    stsc->chunk_info[1].sample_description_index = 1;
  }

  void Modify(SampleToChunk* stsc) {
    stsc->chunk_info.resize(4);
    stsc->chunk_info[2].first_chunk = 7;
    stsc->chunk_info[2].samples_per_chunk = 8;
    stsc->chunk_info[2].sample_description_index = 1;
    stsc->chunk_info[3].first_chunk = 9;
    stsc->chunk_info[3].samples_per_chunk = 12;
    stsc->chunk_info[3].sample_description_index = 0;
  }

  void Fill(SampleSize* stsz) {
    stsz->sample_size = 0;
    stsz->sizes.assign(kData8, kData8 + arraysize(kData8));
    stsz->sample_count = arraysize(kData8);
  }

  void Modify(SampleSize* stsz) {
    stsz->sample_size = 35;
    stsz->sizes.clear();
  }

  void Fill(CompactSampleSize* stz2) {
    stz2->field_size = 4;
    stz2->sizes.assign(kData4, kData4 + arraysize(kData4));
  }

  void Modify(CompactSampleSize* stz2) {
    stz2->field_size = 8;
    stz2->sizes.assign(kData8, kData8 + arraysize(kData8));
  }

  void Fill(ChunkLargeOffset* co64) {
    co64->offsets.assign(kData64, kData64 + arraysize(kData64));
  }

  void Modify(ChunkLargeOffset* co64) { co64->offsets.pop_back(); }

  void Fill(ChunkOffset* stco) {
    stco->offsets.assign(kData32, kData32 + arraysize(kData32));
  }

  void Modify(ChunkOffset* stco) { stco->offsets.push_back(10); }

  void Fill(SyncSample* stss) {
    stss->sample_number.assign(kData32, kData32 + arraysize(kData32));
  }

  void Modify(SyncSample* stss) { stss->sample_number.pop_back(); }

  void Fill(SampleTable* stbl) {
    Fill(&stbl->description);
    Fill(&stbl->decoding_time_to_sample);
    Fill(&stbl->composition_time_to_sample);
    Fill(&stbl->sample_to_chunk);
    Fill(&stbl->sample_size);
    Fill(&stbl->chunk_large_offset);
    Fill(&stbl->sync_sample);
  }

  void Modify(SampleTable* stbl) {
    Modify(&stbl->chunk_large_offset);
    Modify(&stbl->sync_sample);
  }

  void Fill(MediaHeader* mdhd) {
    mdhd->creation_time = 124231432;
    mdhd->modification_time =
        static_cast<uint64_t>(std::numeric_limits<uint32_t>::max()) + 1;
    mdhd->timescale = 50000;
    mdhd->duration = 250000;
    strcpy(mdhd->language, "abc");
    mdhd->version = 1;
  }

  void Modify(MediaHeader* mdhd) {
    mdhd->creation_time = 2;
    mdhd->modification_time = std::numeric_limits<uint32_t>::max();
    strcpy(mdhd->language, "und");
    mdhd->version = 0;
  }

  void Fill(VideoMediaHeader* vmhd) {
    vmhd->graphicsmode = 4123;
    vmhd->opcolor_red = 323;
    vmhd->opcolor_green = 2135;
    vmhd->opcolor_blue = 2387;
  }

  void Modify(VideoMediaHeader* vmhd) { vmhd->graphicsmode *= 2; }

  void Fill(SoundMediaHeader* smhd) { smhd->balance = 8762; }

  void Modify(SoundMediaHeader* smhd) { smhd->balance /= 2; }

  void Fill(SubtitleMediaHeader* sthd) {}
  void Modify(SubtitleMediaHeader* sthd) {}

  void Fill(DataEntryUrl* url) {
    url->flags = 2;
    url->location.assign(kData8, kData8 + arraysize(kData8));
  }

  void Modify(DataEntryUrl* url) {
    url->flags += 1;
    url->location.assign(kData4, kData4 + arraysize(kData4));
  }

  void Fill(DataReference* dref) {
    dref->data_entry.resize(2);
    Fill(&dref->data_entry[0]);
    Fill(&dref->data_entry[1]);
    dref->data_entry[1].location.assign(kData4, kData4 + arraysize(kData4));
  }

  void Modify(DataReference* dref) {
    dref->data_entry.resize(3);
    Fill(&dref->data_entry[2]);
    dref->data_entry[2].location.push_back(100);
  }

  void Fill(DataInformation* dinf) { Fill(&dinf->dref); }

  void Modify(DataInformation* dinf) { Modify(&dinf->dref); }

  void Fill(MediaInformation* minf) {
    Fill(&minf->dinf);
    Fill(&minf->sample_table);
    Fill(&minf->vmhd);
  }

  void Modify(MediaInformation* minf) {
    Modify(&minf->dinf);
    Modify(&minf->sample_table);
  }

  void Fill(Media* mdia) {
    Fill(&mdia->header);
    Fill(&mdia->handler);
    Fill(&mdia->information);
  }

  void Modify(Media* mdia) { Modify(&mdia->information); }

  void Fill(Track* trak) {
    Fill(&trak->header);
    Fill(&trak->media);
    Fill(&trak->edit);
  }

  void Modify(Track* trak) { Modify(&trak->media); }

  void Fill(MovieExtendsHeader* mehd) {
    mehd->fragment_duration = 23489038090ULL;
    mehd->version = 1;
  }

  void Modify(MovieExtendsHeader* mehd) {
    mehd->fragment_duration = 123456;
    mehd->version = 0;
  }

  void Fill(TrackExtends* trex) {
    trex->track_id = 2;
    trex->default_sample_description_index = 3;
    trex->default_sample_duration = 832;
    trex->default_sample_size = 89723;
    trex->default_sample_flags = 12;
  }

  void Modify(TrackExtends* trex) { trex->default_sample_size = 543; }

  void Fill(MovieExtends* mvex) {
    Fill(&mvex->header);
    mvex->tracks.resize(2);
    Fill(&mvex->tracks[0]);
    mvex->tracks[1].track_id = 1;
    mvex->tracks[1].default_sample_description_index = 13;
    mvex->tracks[1].default_sample_duration = 97687;
    mvex->tracks[1].default_sample_size = 1232;
    mvex->tracks[1].default_sample_flags = 6;
  }

  void Modify(MovieExtends* mvex) { mvex->tracks.resize(1); }

  void Fill(Movie* moov) {
    Fill(&moov->header);
    Fill(&moov->extends);
    moov->tracks.resize(2);
    Fill(&moov->tracks[0]);
    Fill(&moov->tracks[1]);
  }

  void Modify(Movie* moov) { moov->tracks.resize(1); }

  void Fill(TrackFragmentDecodeTime* tfdt) {
    tfdt->decode_time = 234029673820ULL;
    tfdt->version = 1;
  }

  void Modify(TrackFragmentDecodeTime* tfdt) {
    tfdt->decode_time = 4567;
    tfdt->version = 0;
  }

  void Fill(MovieFragmentHeader* mfhd) { mfhd->sequence_number = 23235; }

  void Modify(MovieFragmentHeader* mfhd) { mfhd->sequence_number = 67890; }

  void Fill(TrackFragmentHeader* tfhd) {
    tfhd->flags = TrackFragmentHeader::kSampleDescriptionIndexPresentMask |
                  TrackFragmentHeader::kDefaultSampleDurationPresentMask |
                  TrackFragmentHeader::kDefaultSampleSizePresentMask |
                  TrackFragmentHeader::kDefaultSampleFlagsPresentMask;
    tfhd->track_id = 1;
    tfhd->sample_description_index = 233;
    tfhd->default_sample_duration = 42545;
    tfhd->default_sample_size = 8765;
    tfhd->default_sample_flags = 65;
  }

  void Modify(TrackFragmentHeader* tfhd) { tfhd->default_sample_size = 888; }

  void Fill(TrackFragmentRun* trun) {
    trun->flags = TrackFragmentRun::kDataOffsetPresentMask |
                  TrackFragmentRun::kSampleDurationPresentMask |
                  TrackFragmentRun::kSampleSizePresentMask |
                  TrackFragmentRun::kSampleFlagsPresentMask |
                  TrackFragmentRun::kSampleCompTimeOffsetsPresentMask;
    trun->data_offset = 783246;
    trun->sample_count = arraysize(kData32);
    trun->sample_flags.assign(kData32, kData32 + arraysize(kData32));
    trun->sample_sizes = trun->sample_flags;
    trun->sample_sizes[0] += 1000;
    trun->sample_durations = trun->sample_flags;
    trun->sample_durations[1] += 2343;
    trun->sample_composition_time_offsets.assign(kData32,
                                                 kData32 + arraysize(kData32));
    trun->sample_composition_time_offsets[2] = -89782;
    trun->version = 1;
  }

  void Modify(TrackFragmentRun* trun) {
    trun->flags |= TrackFragmentRun::kFirstSampleFlagsPresentMask;
    trun->flags &= ~TrackFragmentRun::kSampleFlagsPresentMask;
    trun->sample_flags.resize(1);
    trun->sample_composition_time_offsets[2] = 9;
    trun->version = 0;
  }

  void Fill(SampleToGroup* sbgp) {
    sbgp->grouping_type = FOURCC_SEIG;
    sbgp->entries.resize(2);
    sbgp->entries[0].sample_count = 3;
    sbgp->entries[0].group_description_index = 0x10002;
    sbgp->entries[1].sample_count = 1212;
    sbgp->entries[1].group_description_index = 0x10001;
  }

  void Modify(SampleToGroup* sbgp) {
    sbgp->entries.resize(1);
    sbgp->entries[0].sample_count = 5;
    sbgp->entries[0].group_description_index = 0x10001;
  }

  void Fill(SampleGroupDescription* sgpd) {
    sgpd->grouping_type = FOURCC_SEIG;
    sgpd->entries.resize(2);
    sgpd->entries[0].is_encrypted = true;
    sgpd->entries[0].iv_size = 8;
    sgpd->entries[0].key_id.assign(kData16Bytes,
                                   kData16Bytes + arraysize(kData16Bytes));
    sgpd->entries[1].is_encrypted = false;
    sgpd->entries[1].iv_size = 0;
    sgpd->entries[1].key_id.resize(16);
    sgpd->version = 1;
  }

  void Modify(SampleGroupDescription* sgpd) {
    sgpd->entries.resize(1);
    sgpd->entries[0].key_id[4] = 88;
    sgpd->version = 1;
  }

  void Fill(TrackFragment* traf) {
    Fill(&traf->header);
    traf->runs.resize(1);
    Fill(&traf->runs[0]);
    Fill(&traf->decode_time);
    Fill(&traf->auxiliary_offset);
    Fill(&traf->auxiliary_size);
  }

  void Modify(TrackFragment* traf) {
    Modify(&traf->header);
    Modify(&traf->decode_time);
    Fill(&traf->sample_to_group);
    Fill(&traf->sample_group_description);
  }

  void Fill(MovieFragment* moof) {
    Fill(&moof->header);
    moof->tracks.resize(1);
    Fill(&moof->tracks[0]);
  }

  void Modify(MovieFragment* moof) {
    moof->tracks.resize(2);
    Fill(&moof->tracks[1]);
    Modify(&moof->tracks[1]);
  }

  void Fill(SegmentIndex* sidx) {
    sidx->reference_id = 3;
    sidx->timescale = 56700;
    sidx->earliest_presentation_time = 234;
    sidx->first_offset = 876223;
    sidx->references.resize(2);
    sidx->references[0].reference_type = true;
    sidx->references[0].referenced_size = 23424;
    sidx->references[0].subsegment_duration = 9083423;
    sidx->references[0].starts_with_sap = true;
    sidx->references[0].sap_type = SegmentReference::Type1;
    sidx->references[0].sap_delta_time = 2382;
    sidx->references[1].reference_type = false;
    sidx->references[1].referenced_size = 34572;
    sidx->references[1].subsegment_duration = 7234323;
    sidx->references[1].starts_with_sap = false;
    sidx->references[1].sap_type = SegmentReference::Type5;
    sidx->references[1].sap_delta_time = 53;
    sidx->version = 0;
  }

  void Modify(SegmentIndex* sidx) {
    sidx->earliest_presentation_time = 2348677865434ULL;
    sidx->references.resize(3);
    sidx->references[2] = sidx->references[1];
    sidx->references[2].subsegment_duration = 87662;
    sidx->version = 1;
  }

  void Fill(CueSourceIDBox* vsid) {
    vsid->source_id = 5;
  }

  void Modify(CueSourceIDBox* vsid) {
    vsid->source_id = 100;
  }

  void Fill(CueTimeBox* ctim) {
    ctim->cue_current_time = "00:19:00.000";
  }

  void Modify(CueTimeBox* ctim) {
    ctim->cue_current_time = "00:20:01.291";
  }

  void Fill(CueIDBox* iden) {
    iden->cue_id = "some_id";
  }

  void Modify(CueIDBox* iden) {
    iden->cue_id = "another_id";
  }

  void Fill(CueSettingsBox* sttg) {
    sttg->settings = "align:left";
  }

  void Modify(CueSettingsBox* sttg) {
    sttg->settings = "align:right";
  }

  void Fill(CuePayloadBox* payl) {
    payl->cue_text = "hello";
  }

  void Modify(CuePayloadBox* payl) {
    payl->cue_text = "hi";
  }

  void Fill(VTTEmptyCueBox* vtte) {}
  void Modify(VTTEmptyCueBox* vtte) {}

  void Fill(VTTAdditionalTextBox* vtta) {
    vtta->cue_additional_text = "NOTE some comment";
  }

  void Modify(VTTAdditionalTextBox* vtta) {
    vtta->cue_additional_text = "NOTE another comment";
  }

  void Fill(VTTCueBox* vttc) {
    Fill(&vttc->cue_source_id);
    Fill(&vttc->cue_id);
    Fill(&vttc->cue_time);
    Fill(&vttc->cue_settings);
    Fill(&vttc->cue_payload);
  }

  void Modify(VTTCueBox* vttc) {
    Modify(&vttc->cue_source_id);
    Modify(&vttc->cue_id);
    Modify(&vttc->cue_time);
    Modify(&vttc->cue_settings);
    Modify(&vttc->cue_payload);
  }

  bool IsOptional(const SampleAuxiliaryInformationOffset* box) { return true; }
  bool IsOptional(const SampleAuxiliaryInformationSize* box) { return true; }
  bool IsOptional(const SampleEncryption* box) { return true; }
  bool IsOptional(const ProtectionSchemeInfo* box) { return true; }
  bool IsOptional(const EditList* box) { return true; }
  bool IsOptional(const Edit* box) { return true; }
  bool IsOptional(const CodecConfigurationRecord* box) { return true; }
  bool IsOptional(const PixelAspectRatio* box) { return true; }
  bool IsOptional(const ElementaryStreamDescriptor* box) { return true; }
  // Recommended, but optional.
  bool IsOptional(const WebVTTSourceLabelBox* box) { return true; }
  bool IsOptional(const CompositionTimeToSample* box) { return true; }
  bool IsOptional(const SyncSample* box) { return true; }
  bool IsOptional(const MovieExtendsHeader* box) { return true; }
  bool IsOptional(const MovieExtends* box) { return true; }
  bool IsOptional(const SampleToGroup* box) { return true; }
  bool IsOptional(const SampleGroupDescription* box) { return true; }
  bool IsOptional(const CueSourceIDBox* box) { return true; }
  bool IsOptional(const CueIDBox* box) { return true; }
  bool IsOptional(const CueTimeBox* box) { return true; }
  bool IsOptional(const CueSettingsBox* box) { return true; }
  bool IsOptional(const DTSSpecific* box) {return true; }

 protected:
  scoped_ptr<BufferWriter> buffer_;
};

typedef testing::Types<
    FileType,
    SegmentType,
    ProtectionSystemSpecificHeader,
    SampleAuxiliaryInformationOffset,
    SampleAuxiliaryInformationSize,
    OriginalFormat,
    SchemeType,
    TrackEncryption,
    SchemeInfo,
    ProtectionSchemeInfo,
    MovieHeader,
    TrackHeader,
    EditList,
    Edit,
    HandlerReference,
    CodecConfigurationRecord,
    PixelAspectRatio,
    VideoSampleEntry,
    ElementaryStreamDescriptor,
    AudioSampleEntry,
    WebVTTConfigurationBox,
    WebVTTSourceLabelBox,
    WVTTSampleEntry,
    SampleDescription,
    DecodingTimeToSample,
    CompositionTimeToSample,
    SampleToChunk,
    SampleSize,
    CompactSampleSize,
    ChunkLargeOffset,
    ChunkOffset,
    SyncSample,
    SampleTable,
    MediaHeader,
    VideoMediaHeader,
    SoundMediaHeader,
    SubtitleMediaHeader,
    DataEntryUrl,
    DataReference,
    DataInformation,
    MediaInformation,
    Media,
    Track,
    MovieExtendsHeader,
    TrackExtends,
    MovieExtends,
    Movie,
    TrackFragmentDecodeTime,
    MovieFragmentHeader,
    TrackFragmentHeader> Boxes;

// GTEST support a maximum of 50 types in the template list, so we have to
// break it into two groups.
typedef testing::Types<
    TrackFragmentRun,
    TrackFragment,
    MovieFragment,
    SegmentIndex,
    SampleToGroup,
    SampleGroupDescription,
    CueSourceIDBox,
    CueTimeBox,
    CueIDBox,
    CueSettingsBox,
    CuePayloadBox,
    VTTEmptyCueBox,
    VTTAdditionalTextBox,
    VTTCueBox,
    DTSSpecific> Boxes2;

TYPED_TEST_CASE_P(BoxDefinitionsTestGeneral);

TYPED_TEST_P(BoxDefinitionsTestGeneral, WriteHeader) {
  TypeParam box;
  LOG(INFO) << "Processing " << FourCCToString(box.BoxType());
  this->Fill(&box);
  box.WriteHeader(this->buffer_.get());
  // Box header size should be either 8 bytes or 12 bytes.
  EXPECT_TRUE(this->buffer_->Size() == 8 || this->buffer_->Size() == 12);
}

TYPED_TEST_P(BoxDefinitionsTestGeneral, WriteReadbackCompare) {
  TypeParam box;
  LOG(INFO) << "Processing " << FourCCToString(box.BoxType());
  this->Fill(&box);
  box.Write(this->buffer_.get());

  TypeParam box_readback;
  ASSERT_TRUE(this->ReadBack(&box_readback));
  ASSERT_EQ(box, box_readback);
}

TYPED_TEST_P(BoxDefinitionsTestGeneral, WriteModifyWrite) {
  TypeParam box;
  LOG(INFO) << "Processing " << FourCCToString(box.BoxType());
  this->Fill(&box);
  // Save the expected version set earlier in function |Fill|, then clear
  // the version, expecting box.Write set version as expected.
  uint8_t version = this->GetAndClearVersion(&box);
  box.Write(this->buffer_.get());
  EXPECT_EQ(version, this->GetAndClearVersion(&box));

  this->buffer_->Clear();
  this->Modify(&box);
  version = this->GetAndClearVersion(&box);
  box.Write(this->buffer_.get());
  EXPECT_EQ(version, this->GetAndClearVersion(&box));

  TypeParam box_readback;
  ASSERT_TRUE(this->ReadBack(&box_readback));
  ASSERT_EQ(box, box_readback);
}

TYPED_TEST_P(BoxDefinitionsTestGeneral, Empty) {
  TypeParam box;
  LOG(INFO) << "Processing " << FourCCToString(box.BoxType());
  if (this->IsOptional(&box)) {
    ASSERT_EQ(0u, box.ComputeSize());
  } else {
    ASSERT_NE(0u, box.ComputeSize());
  }
}

REGISTER_TYPED_TEST_CASE_P(BoxDefinitionsTestGeneral,
                           WriteHeader,
                           WriteReadbackCompare,
                           WriteModifyWrite,
                           Empty);

INSTANTIATE_TYPED_TEST_CASE_P(BoxDefinitionTypedTests,
                              BoxDefinitionsTestGeneral,
                              Boxes);
INSTANTIATE_TYPED_TEST_CASE_P(BoxDefinitionTypedTests2,
                              BoxDefinitionsTestGeneral,
                              Boxes2);

// Test other cases of box input.
class BoxDefinitionsTest : public BoxDefinitionsTestGeneral<Box> {};

TEST_F(BoxDefinitionsTest, DTSSampleEntry) {
  AudioSampleEntry entry;
  entry.format = FOURCC_DTSE;
  entry.data_reference_index = 2;
  entry.channelcount = 5;
  entry.samplesize = 16;
  entry.samplerate = 44100;
  Fill(&entry.ddts);
  entry.Write(this->buffer_.get());

  AudioSampleEntry entry_readback;
  ASSERT_TRUE(ReadBack(&entry_readback));
  ASSERT_EQ(entry, entry_readback);
}

TEST_F(BoxDefinitionsTest, ProtectionSystemSpecificHeader) {
  ProtectionSystemSpecificHeader pssh;
  Fill(&pssh);
  pssh.Write(this->buffer_.get());

  ProtectionSystemSpecificHeader pssh_readback;
  ASSERT_TRUE(ReadBack(&pssh_readback));
  ASSERT_EQ(pssh, pssh_readback);

  pssh_readback.raw_box[15] += 1;
  pssh_readback.Write(this->buffer_.get());

  ProtectionSystemSpecificHeader pssh_readback2;
  ASSERT_TRUE(ReadBack(&pssh_readback2));

  // If raw_box is set, raw_box will be written instead.
  ASSERT_FALSE(pssh_readback == pssh_readback2);
  ASSERT_EQ(pssh_readback.raw_box, pssh_readback2.raw_box);
}

TEST_F(BoxDefinitionsTest, CompactSampleSize_FieldSize16) {
  CompactSampleSize stz2;
  stz2.field_size = 16;
  stz2.sizes.assign(kData16, kData16 + arraysize(kData16));
  stz2.Write(this->buffer_.get());

  CompactSampleSize stz2_readback;
  ASSERT_TRUE(ReadBack(&stz2_readback));
  ASSERT_EQ(stz2, stz2_readback);
}

TEST_F(BoxDefinitionsTest, ChunkLargeOffsetSmallOffset) {
  ChunkLargeOffset co64;
  co64.offsets.assign(kData32, kData32 + arraysize(kData32));
  co64.Write(this->buffer_.get());

  // The data is stored in ChunkOffset box instead.
  ChunkOffset stco;
  ASSERT_TRUE(ReadBack(&stco));
  ASSERT_EQ(co64, stco);
}

TEST_F(BoxDefinitionsTest, TrackFragmentHeader_NoSampleSize) {
  TrackFragmentHeader tfhd;
  Fill(&tfhd);
  tfhd.flags &= ~TrackFragmentHeader::kDefaultSampleSizePresentMask;
  tfhd.Write(this->buffer_.get());

  TrackFragmentHeader tfhd_readback;
  ASSERT_TRUE(ReadBack(&tfhd_readback));
  EXPECT_EQ(0u, tfhd_readback.default_sample_size);
  tfhd.default_sample_size = 0;
  ASSERT_EQ(tfhd, tfhd_readback);
}

TEST_F(BoxDefinitionsTest, TrackFragmentRun_NoSampleSize) {
  TrackFragmentRun trun;
  Fill(&trun);
  trun.flags &= ~TrackFragmentRun::kSampleSizePresentMask;
  trun.Write(this->buffer_.get());

  TrackFragmentRun trun_readback;
  ASSERT_TRUE(ReadBack(&trun_readback));
  EXPECT_TRUE(trun_readback.sample_sizes.empty());
  trun.sample_sizes.clear();
  ASSERT_EQ(trun, trun_readback);
}

TEST_F(BoxDefinitionsTest, SampleEncryptionIsOptional) {
  SampleEncryption senc;
  EXPECT_EQ(0u, senc.ComputeSize());
}

TEST_F(BoxDefinitionsTest, SampleEncryptionWithIvKnownWhenReading) {
  SampleEncryption senc;
  Fill(&senc);
  senc.Write(buffer_.get());

  SampleEncryption senc_readback;
  senc_readback.iv_size = senc.iv_size;

  ASSERT_TRUE(ReadBack(&senc_readback));
  EXPECT_EQ(0u, senc_readback.sample_encryption_data.size());
  EXPECT_NE(0u, senc_readback.sample_encryption_entries.size());
  ASSERT_EQ(senc, senc_readback);

  Modify(&senc);
  senc.Write(buffer_.get());
  ASSERT_TRUE(ReadBack(&senc_readback));
  ASSERT_EQ(senc, senc_readback);
}

TEST_F(BoxDefinitionsTest, SampleEncryptionWithIvUnknownWhenReading) {
  SampleEncryption senc;
  Fill(&senc);
  senc.Write(buffer_.get());

  SampleEncryption senc_readback;
  senc_readback.iv_size = 0;

  ASSERT_TRUE(ReadBack(&senc_readback));
  EXPECT_NE(0u, senc_readback.sample_encryption_data.size());
  EXPECT_EQ(0u, senc_readback.sample_encryption_entries.size());

  std::vector<SampleEncryptionEntry> sample_encryption_entries;
  ASSERT_TRUE(senc_readback.ParseFromSampleEncryptionData(
      senc.iv_size, &sample_encryption_entries));
  ASSERT_EQ(senc.sample_encryption_entries, sample_encryption_entries);
}

}  // namespace mp4
}  // namespace media
}  // namespace edash_packager
