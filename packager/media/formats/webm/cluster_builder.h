// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_WEBM_CLUSTER_BUILDER_H_
#define MEDIA_FORMATS_WEBM_CLUSTER_BUILDER_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"

namespace media {

class Cluster {
 public:
  Cluster(scoped_ptr<uint8[]> data, int size);
  ~Cluster();

  const uint8* data() const { return data_.get(); }
  int size() const { return size_; }

 private:
  scoped_ptr<uint8[]> data_;
  int size_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(Cluster);
};

class ClusterBuilder {
 public:
  ClusterBuilder();
  ~ClusterBuilder();

  void SetClusterTimecode(int64 cluster_timecode);
  void AddSimpleBlock(int track_num, int64 timecode, int flags,
                      const uint8* data, int size);
  void AddBlockGroup(int track_num, int64 timecode, int duration, int flags,
                     const uint8* data, int size);
  void AddBlockGroupWithoutBlockDuration(int track_num, int64 timecode,
                     int flags, const uint8* data, int size);

  scoped_ptr<Cluster> Finish();
  scoped_ptr<Cluster> FinishWithUnknownSize();

 private:
  void AddBlockGroupInternal(int track_num, int64 timecode,
                             bool include_block_duration, int duration,
                             int flags, const uint8* data, int size);
  void Reset();
  void ExtendBuffer(int bytes_needed);
  void UpdateUInt64(int offset, int64 value);
  void WriteBlock(uint8* buf, int track_num, int64 timecode, int flags,
                  const uint8* data, int size);

  scoped_ptr<uint8[]> buffer_;
  int buffer_size_;
  int bytes_used_;
  int64 cluster_timecode_;

  DISALLOW_COPY_AND_ASSIGN(ClusterBuilder);
};

}  // namespace media

#endif  // MEDIA_FORMATS_WEBM_CLUSTER_BUILDER_H_
