// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/webm/tracks_builder.h"

#include "media/webm/webm_constants.h"

namespace media {

// Returns size of an integer, formatted using Matroska serialization.
static int GetUIntMkvSize(uint64 value) {
  if (value < 0x07FULL)
    return 1;
  if (value < 0x03FFFULL)
    return 2;
  if (value < 0x01FFFFFULL)
    return 3;
  if (value < 0x0FFFFFFFULL)
    return 4;
  if (value < 0x07FFFFFFFFULL)
    return 5;
  if (value < 0x03FFFFFFFFFFULL)
    return 6;
  if (value < 0x01FFFFFFFFFFFFULL)
    return 7;
  return 8;
}

// Returns the minimium size required to serialize an integer value.
static int GetUIntSize(uint64 value) {
  if (value < 0x0100ULL)
    return 1;
  if (value < 0x010000ULL)
    return 2;
  if (value < 0x01000000ULL)
    return 3;
  if (value < 0x0100000000ULL)
    return 4;
  if (value < 0x010000000000ULL)
    return 5;
  if (value < 0x01000000000000ULL)
    return 6;
  if (value < 0x0100000000000000ULL)
    return 7;
  return 8;
}

static int MasterElementSize(int element_id, int payload_size) {
  return GetUIntSize(element_id) + GetUIntMkvSize(payload_size) + payload_size;
}

static int IntElementSize(int element_id, int value) {
  return GetUIntSize(element_id) + 1 + GetUIntSize(value);
}

static int StringElementSize(int element_id, const std::string& value) {
 return GetUIntSize(element_id) +
        GetUIntMkvSize(value.length()) +
        value.length();
}

static void SerializeInt(uint8** buf_ptr, int* buf_size_ptr,
                         int64 value, int size) {
  uint8*& buf = *buf_ptr;
  int& buf_size = *buf_size_ptr;

  for (int idx = 1; idx <= size; ++idx) {
    *buf++ = static_cast<uint8>(value >> ((size - idx) * 8));
    --buf_size;
  }
}

static void WriteElementId(uint8** buf, int* buf_size, int element_id) {
  SerializeInt(buf, buf_size, element_id, GetUIntSize(element_id));
}

static void WriteUInt(uint8** buf, int* buf_size, uint64 value) {
  const int size = GetUIntMkvSize(value);
  value |= (1ULL << (size * 7));  // Matroska formatting
  SerializeInt(buf, buf_size, value, size);
}

static void WriteMasterElement(uint8** buf, int* buf_size,
                               int element_id, int payload_size) {
  WriteElementId(buf, buf_size, element_id);
  WriteUInt(buf, buf_size, payload_size);
}

static void WriteIntElement(uint8** buf, int* buf_size,
                            int element_id, int value) {
  WriteElementId(buf, buf_size, element_id);

  const int size = GetUIntSize(value);
  WriteUInt(buf, buf_size, size);

  SerializeInt(buf, buf_size, value, size);
}

static void WriteStringElement(uint8** buf_ptr, int* buf_size_ptr,
                               int element_id, const std::string& value) {
  uint8*& buf = *buf_ptr;
  int& buf_size = *buf_size_ptr;

  WriteElementId(&buf, &buf_size, element_id);

  const uint64 size = value.length();
  WriteUInt(&buf, &buf_size, size);

  memcpy(buf, value.data(), size);
  buf += size;
  buf_size -= size;
}

TracksBuilder::TracksBuilder() {}
TracksBuilder::~TracksBuilder() {}

void TracksBuilder::AddTrack(
    int track_num,
    int track_type,
    const std::string& codec_id,
    const std::string& name,
    const std::string& language) {
  tracks_.push_back(Track(track_num, track_type, codec_id, name, language));
}

std::vector<uint8> TracksBuilder::Finish() {
  // Allocate the storage
  std::vector<uint8> buffer;
  buffer.resize(GetTracksSize());

  // Populate the storage with a tracks header
  WriteTracks(&buffer[0], buffer.size());

  return buffer;
}

int TracksBuilder::GetTracksSize() const {
  return MasterElementSize(kWebMIdTracks, GetTracksPayloadSize());
}

int TracksBuilder::GetTracksPayloadSize() const {
  int payload_size = 0;

  for (TrackList::const_iterator itr = tracks_.begin();
       itr != tracks_.end(); ++itr) {
    payload_size += itr->GetSize();
  }

  return payload_size;
}

void TracksBuilder::WriteTracks(uint8* buf, int buf_size) const {
  WriteMasterElement(&buf, &buf_size, kWebMIdTracks, GetTracksPayloadSize());

  for (TrackList::const_iterator itr = tracks_.begin();
       itr != tracks_.end(); ++itr) {
    itr->Write(&buf, &buf_size);
  }
}

TracksBuilder::Track::Track(int track_num, int track_type,
                            const std::string& codec_id,
                            const std::string& name,
                            const std::string& language)
    : track_num_(track_num),
      track_type_(track_type),
      codec_id_(codec_id),
      name_(name),
      language_(language) {
}

int TracksBuilder::Track::GetSize() const {
  return MasterElementSize(kWebMIdTrackEntry, GetPayloadSize());
}

int TracksBuilder::Track::GetPayloadSize() const {
  int size = 0;

  size += IntElementSize(kWebMIdTrackNumber, track_num_);
  size += IntElementSize(kWebMIdTrackType, track_type_);

  if (!codec_id_.empty())
    size += StringElementSize(kWebMIdCodecID, codec_id_);

  if (!name_.empty())
    size += StringElementSize(kWebMIdName, name_);

  if (!language_.empty())
    size += StringElementSize(kWebMIdLanguage, language_);

  return size;
}

void TracksBuilder::Track::Write(uint8** buf, int* buf_size) const {
  WriteMasterElement(buf, buf_size, kWebMIdTrackEntry, GetPayloadSize());

  WriteIntElement(buf, buf_size, kWebMIdTrackNumber, track_num_);
  WriteIntElement(buf, buf_size, kWebMIdTrackType, track_type_);

  if (!codec_id_.empty())
    WriteStringElement(buf, buf_size, kWebMIdCodecID, codec_id_);

  if (!name_.empty())
    WriteStringElement(buf, buf_size, kWebMIdName, name_);

  if (!language_.empty())
    WriteStringElement(buf, buf_size, kWebMIdLanguage, language_);
}

}  // namespace media
