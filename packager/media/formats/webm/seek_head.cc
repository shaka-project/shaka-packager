// Copyright 2015 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/formats/webm/seek_head.h"

#include <algorithm>
#include <limits>

#include "packager/base/logging.h"
#include "packager/third_party/libwebm/src/mkvmuxerutil.hpp"
#include "packager/third_party/libwebm/src/webmids.hpp"

namespace shaka {
namespace media {
namespace {

// Cluster, Cues, Info, Tracks.
const size_t kElementIdCount = 4u;

uint64_t EbmlMasterElementWithPayloadSize(mkvmuxer::MkvId id, uint64_t payload_size) {
  return EbmlMasterElementSize(id, payload_size) + payload_size;
}

uint64_t MaxSeekEntrySize() {
  const uint64_t max_entry_payload_size =
      EbmlElementSize(
          mkvmuxer::kMkvSeekID,
          static_cast<mkvmuxer::uint64>(std::numeric_limits<uint32_t>::max())) +
      EbmlElementSize(mkvmuxer::kMkvSeekPosition,
                      std::numeric_limits<mkvmuxer::uint64>::max());
  return EbmlMasterElementWithPayloadSize(mkvmuxer::kMkvSeek,
                                          max_entry_payload_size);
}

}  // namespace

SeekHead::SeekHead()
    : total_void_size_(EbmlMasterElementWithPayloadSize(
          mkvmuxer::kMkvSeekHead,
          kElementIdCount * MaxSeekEntrySize())) {}

SeekHead::~SeekHead() {}

bool SeekHead::Write(mkvmuxer::IMkvWriter* writer) {
  std::vector<SeekElement> seek_elements = CreateSeekElements();
  if (seek_elements.empty())
    return true;

  uint64_t payload_size = 0;
  for (const SeekHead::SeekElement& seek_element : seek_elements) {
    payload_size +=
        EbmlMasterElementWithPayloadSize(mkvmuxer::kMkvSeek, seek_element.size);
  }

  const int64_t start_pos = writer->Position();
  if (!WriteEbmlMasterElement(writer, mkvmuxer::kMkvSeekHead, payload_size))
    return false;

  for (const SeekHead::SeekElement& element : seek_elements) {
    if (!WriteEbmlMasterElement(writer, mkvmuxer::kMkvSeek, element.size) ||
        !WriteEbmlElement(writer, mkvmuxer::kMkvSeekID, element.id) ||
        !WriteEbmlElement(writer, mkvmuxer::kMkvSeekPosition, element.position))
      return false;
  }

  // If we wrote void before, then fill in the extra with void.
  if (wrote_void_) {
    const uint64_t extra_void =
        total_void_size_ - (writer->Position() - start_pos);
    if (!WriteVoidElement(writer, extra_void))
      return false;
  }

  return true;
}

bool SeekHead::WriteVoid(mkvmuxer::IMkvWriter* writer) {
  const uint64_t written = WriteVoidElement(writer, total_void_size_);
  if (!written)
    return false;
  wrote_void_ = true;
  return true;
}

std::vector<SeekHead::SeekElement> SeekHead::CreateSeekElements() {
  std::vector<SeekHead::SeekElement> seek_elements;
  if (info_pos_ != 0)
    seek_elements.emplace_back(mkvmuxer::kMkvInfo, info_pos_);
  if (tracks_pos_ != 0)
    seek_elements.emplace_back(mkvmuxer::kMkvTracks, tracks_pos_);
  if (cues_pos_ != 0)
    seek_elements.emplace_back(mkvmuxer::kMkvCues, cues_pos_);
  if (cluster_pos_ != 0)
    seek_elements.emplace_back(mkvmuxer::kMkvCluster, cluster_pos_);
  DCHECK_LE(seek_elements.size(), kElementIdCount);

  std::sort(seek_elements.begin(), seek_elements.end(),
            [](const SeekHead::SeekElement& left,
               const SeekHead::SeekElement& right) {
              return left.position < right.position;
            });
  for (SeekHead::SeekElement& element : seek_elements) {
    element.size =
        EbmlElementSize(mkvmuxer::kMkvSeekID, element.id) +
        EbmlElementSize(mkvmuxer::kMkvSeekPosition, element.position);
  }
  return seek_elements;
}

}  // namespace media
}  // namespace shaka
