// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/webm/cluster_builder.h"

#include "base/logging.h"
#include "media/base/data_buffer.h"
#include "media/formats/webm/webm_constants.h"

namespace media {

static const uint8 kClusterHeader[] = {
  0x1F, 0x43, 0xB6, 0x75,  // CLUSTER ID
  0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // cluster(size = 0)
  0xE7,  // Timecode ID
  0x88,  // timecode(size=8)
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // timecode value
};

static const uint8 kSimpleBlockHeader[] = {
  0xA3,  // SimpleBlock ID
  0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // SimpleBlock(size = 0)
};

static const uint8 kBlockGroupHeader[] = {
  0xA0,  // BlockGroup ID
  0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // BlockGroup(size = 0)
  0x9B,  // BlockDuration ID
  0x88,  // BlockDuration(size = 8)
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // duration
  0xA1,  // Block ID
  0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // Block(size = 0)
};

static const uint8 kBlockGroupHeaderWithoutBlockDuration[] = {
  0xA0,  // BlockGroup ID
  0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // BlockGroup(size = 0)
  0xA1,  // Block ID
  0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // Block(size = 0)
};

enum {
  kClusterSizeOffset = 4,
  kClusterTimecodeOffset = 14,

  kSimpleBlockSizeOffset = 1,

  kBlockGroupSizeOffset = 1,
  kBlockGroupWithoutBlockDurationBlockSizeOffset = 10,
  kBlockGroupDurationOffset = 11,
  kBlockGroupBlockSizeOffset = 20,

  kInitialBufferSize = 32768,
};

Cluster::Cluster(scoped_ptr<uint8[]> data, int size)
    : data_(data.Pass()), size_(size) {}
Cluster::~Cluster() {}

ClusterBuilder::ClusterBuilder() { Reset(); }
ClusterBuilder::~ClusterBuilder() {}

void ClusterBuilder::SetClusterTimecode(int64 cluster_timecode) {
  DCHECK_EQ(cluster_timecode_, -1);

  cluster_timecode_ = cluster_timecode;

  // Write the timecode into the header.
  uint8* buf = buffer_.get() + kClusterTimecodeOffset;
  for (int i = 7; i >= 0; --i) {
    buf[i] = cluster_timecode & 0xff;
    cluster_timecode >>= 8;
  }
}

void ClusterBuilder::AddSimpleBlock(int track_num, int64 timecode, int flags,
                                    const uint8* data, int size) {
  int block_size = size + 4;
  int bytes_needed = sizeof(kSimpleBlockHeader) + block_size;
  if (bytes_needed > (buffer_size_ - bytes_used_))
    ExtendBuffer(bytes_needed);

  uint8* buf = buffer_.get() + bytes_used_;
  int block_offset = bytes_used_;
  memcpy(buf, kSimpleBlockHeader, sizeof(kSimpleBlockHeader));
  UpdateUInt64(block_offset + kSimpleBlockSizeOffset, block_size);
  buf += sizeof(kSimpleBlockHeader);

  WriteBlock(buf, track_num, timecode, flags, data, size);

  bytes_used_ += bytes_needed;
}

void ClusterBuilder::AddBlockGroup(int track_num, int64 timecode, int duration,
                                   int flags, const uint8* data, int size) {
  AddBlockGroupInternal(track_num, timecode, true, duration, flags, data, size);
}

void ClusterBuilder::AddBlockGroupWithoutBlockDuration(int track_num,
                                                       int64 timecode,
                                                       int flags,
                                                       const uint8* data,
                                                       int size) {
  AddBlockGroupInternal(track_num, timecode, false, 0, flags, data, size);
}


void ClusterBuilder::AddBlockGroupInternal(int track_num, int64 timecode,
                                           bool include_block_duration,
                                           int duration, int flags,
                                           const uint8* data, int size) {
  int block_size = size + 4;
  int bytes_needed = block_size;
  if (include_block_duration) {
    bytes_needed += sizeof(kBlockGroupHeader);
  } else {
    bytes_needed += sizeof(kBlockGroupHeaderWithoutBlockDuration);
  }

  int block_group_size = bytes_needed - 9;

  if (bytes_needed > (buffer_size_ - bytes_used_))
    ExtendBuffer(bytes_needed);

  uint8* buf = buffer_.get() + bytes_used_;
  int block_group_offset = bytes_used_;
  if (include_block_duration) {
    memcpy(buf, kBlockGroupHeader, sizeof(kBlockGroupHeader));
    UpdateUInt64(block_group_offset + kBlockGroupDurationOffset, duration);
    UpdateUInt64(block_group_offset + kBlockGroupBlockSizeOffset, block_size);
    buf += sizeof(kBlockGroupHeader);
  } else {
    memcpy(buf, kBlockGroupHeaderWithoutBlockDuration,
           sizeof(kBlockGroupHeaderWithoutBlockDuration));
    UpdateUInt64(
        block_group_offset + kBlockGroupWithoutBlockDurationBlockSizeOffset,
        block_size);
    buf += sizeof(kBlockGroupHeaderWithoutBlockDuration);
  }

  UpdateUInt64(block_group_offset + kBlockGroupSizeOffset, block_group_size);

  // Make sure the 4 most-significant bits are 0.
  // http://www.matroska.org/technical/specs/index.html#block_structure
  flags &= 0x0f;

  WriteBlock(buf, track_num, timecode, flags, data, size);

  bytes_used_ += bytes_needed;
}

void ClusterBuilder::WriteBlock(uint8* buf, int track_num, int64 timecode,
                                int flags, const uint8* data, int size) {
  DCHECK_GE(track_num, 0);
  DCHECK_LE(track_num, 126);
  DCHECK_GE(flags, 0);
  DCHECK_LE(flags, 0xff);
  DCHECK(data);
  DCHECK_GT(size, 0);
  DCHECK_NE(cluster_timecode_, -1);

  int64 timecode_delta = timecode - cluster_timecode_;
  DCHECK_GE(timecode_delta, -32768);
  DCHECK_LE(timecode_delta, 32767);

  buf[0] = 0x80 | (track_num & 0x7F);
  buf[1] = (timecode_delta >> 8) & 0xff;
  buf[2] = timecode_delta & 0xff;
  buf[3] = flags & 0xff;
  memcpy(buf + 4, data, size);
}

scoped_ptr<Cluster> ClusterBuilder::Finish() {
  DCHECK_NE(cluster_timecode_, -1);

  UpdateUInt64(kClusterSizeOffset, bytes_used_ - (kClusterSizeOffset + 8));

  scoped_ptr<Cluster> ret(new Cluster(buffer_.Pass(), bytes_used_));
  Reset();
  return ret.Pass();
}

scoped_ptr<Cluster> ClusterBuilder::FinishWithUnknownSize() {
  DCHECK_NE(cluster_timecode_, -1);

  UpdateUInt64(kClusterSizeOffset, kWebMUnknownSize);

  scoped_ptr<Cluster> ret(new Cluster(buffer_.Pass(), bytes_used_));
  Reset();
  return ret.Pass();
}

void ClusterBuilder::Reset() {
  buffer_size_ = kInitialBufferSize;
  buffer_.reset(new uint8[buffer_size_]);
  memcpy(buffer_.get(), kClusterHeader, sizeof(kClusterHeader));
  bytes_used_ = sizeof(kClusterHeader);
  cluster_timecode_ = -1;
}

void ClusterBuilder::ExtendBuffer(int bytes_needed) {
  int new_buffer_size = 2 * buffer_size_;

  while ((new_buffer_size - bytes_used_) < bytes_needed)
    new_buffer_size *= 2;

  scoped_ptr<uint8[]> new_buffer(new uint8[new_buffer_size]);

  memcpy(new_buffer.get(), buffer_.get(), bytes_used_);
  buffer_.reset(new_buffer.release());
  buffer_size_ = new_buffer_size;
}

void ClusterBuilder::UpdateUInt64(int offset, int64 value) {
  DCHECK_LE(offset + 7, buffer_size_);
  uint8* buf = buffer_.get() + offset;

  // Fill the last 7 bytes of size field in big-endian order.
  for (int i = 7; i > 0; i--) {
    buf[i] = value & 0xff;
    value >>= 8;
  }
}

}  // namespace media
