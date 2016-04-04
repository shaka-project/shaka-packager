// Copyright 2015 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/formats/webm/seek_head.h"

#include <limits>

#include "packager/third_party/libwebm/src/mkvmuxerutil.hpp"
#include "packager/third_party/libwebm/src/webmids.hpp"

namespace edash_packager {
namespace media {
namespace {
const mkvmuxer::uint64 kElementIds[] = {
    mkvmuxer::kMkvInfo, mkvmuxer::kMkvTracks, mkvmuxer::kMkvCluster,
    mkvmuxer::kMkvCues,
};
const int kElementIdCount = arraysize(kElementIds);

uint64_t MaxSeekEntrySize() {
  const uint64_t max_entry_payload_size =
      EbmlElementSize(
          mkvmuxer::kMkvSeekID,
          static_cast<mkvmuxer::uint64>(std::numeric_limits<uint32_t>::max())) +
      EbmlElementSize(mkvmuxer::kMkvSeekPosition,
                      std::numeric_limits<mkvmuxer::uint64>::max());
  const uint64_t max_entry_size =
      EbmlMasterElementSize(mkvmuxer::kMkvSeek, max_entry_payload_size) +
      max_entry_payload_size;

  return max_entry_size;
}
}  // namespace

SeekHead::SeekHead()
    : cluster_pos_(-1),
      cues_pos_(-1),
      info_pos_(-1),
      tracks_pos_(-1),
      wrote_void_(false) {}

SeekHead::~SeekHead() {}

bool SeekHead::Write(mkvmuxer::IMkvWriter* writer) {
  std::vector<uint64_t> element_sizes;
  const uint64_t payload_size = GetPayloadSize(&element_sizes);

  if (payload_size == 0) {
    return true;
  }

  const int64_t start_pos = writer->Position();
  if (!WriteEbmlMasterElement(writer, mkvmuxer::kMkvSeekHead, payload_size))
    return false;

  const int64_t positions[] = {info_pos_, tracks_pos_, cluster_pos_, cues_pos_};
  for (int i = 0; i < kElementIdCount; ++i) {
    if (element_sizes[i] == 0)
      continue;

    const mkvmuxer::uint64 position =
        static_cast<mkvmuxer::uint64>(positions[i]);
    if (!WriteEbmlMasterElement(writer, mkvmuxer::kMkvSeek, element_sizes[i]) ||
        !WriteEbmlElement(writer, mkvmuxer::kMkvSeekID, kElementIds[i]) ||
        !WriteEbmlElement(writer, mkvmuxer::kMkvSeekPosition, position))
      return false;
  }

  // If we wrote void before, then fill in the extra with void.
  if (wrote_void_) {
    const uint64_t max_payload_size = kElementIdCount * MaxSeekEntrySize();
    const uint64_t total_void_size =
        EbmlMasterElementSize(mkvmuxer::kMkvSeekHead, max_payload_size) +
        max_payload_size;

    const uint64_t extra_void =
        total_void_size - (writer->Position() - start_pos);
    if (!WriteVoidElement(writer, extra_void))
      return false;
  }

  return true;
}

bool SeekHead::WriteVoid(mkvmuxer::IMkvWriter* writer) {
  const uint64_t payload_size = kElementIdCount * MaxSeekEntrySize();
  const uint64_t total_size =
      EbmlMasterElementSize(mkvmuxer::kMkvSeekHead, payload_size) +
      payload_size;

  wrote_void_ = true;
  const uint64_t written = WriteVoidElement(writer, total_size);
  if (!written)
    return false;

  return true;
}

uint64_t SeekHead::GetPayloadSize(std::vector<uint64_t>* data) {
  const int64_t positions[] = {info_pos_, tracks_pos_, cluster_pos_, cues_pos_};
  uint64_t total_payload_size = 0;
  data->resize(kElementIdCount);
  for (int i = 0; i < kElementIdCount; ++i) {
    if (positions[i] < 0) {
      (*data)[i] = 0;
      continue;
    }

    const mkvmuxer::uint64 position =
        static_cast<mkvmuxer::uint64>(positions[i]);
    (*data)[i] = EbmlElementSize(mkvmuxer::kMkvSeekID, kElementIds[i]) +
                 EbmlElementSize(mkvmuxer::kMkvSeekPosition, position);
    total_payload_size +=
        data->at(i) + EbmlMasterElementSize(mkvmuxer::kMkvSeek, data->at(i));
  }

  return total_payload_size;
}

}  // namespace media
}  // namespace edash_packager
