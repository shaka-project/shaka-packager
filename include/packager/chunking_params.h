// Copyright 2017 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_PUBLIC_CHUNKING_PARAMS_H_
#define PACKAGER_PUBLIC_CHUNKING_PARAMS_H_

#include <cstdint>

namespace shaka {

/// Default heartbeat shift for DVB-Teletext: 1 second at 90kHz timescale.
constexpr int64_t kDefaultTtxHeartbeatShift = 90000;

/// Chunking (segmentation) related parameters.
struct ChunkingParams {
  /// Segment duration in seconds.
  double segment_duration_in_seconds = 0;
  /// Subsegment duration in seconds. Should not be larger than the segment
  /// duration.
  double subsegment_duration_in_seconds = 0;

  /// Force segments to begin with stream access points. Actual segment duration
  /// may not be exactly what is specified by segment_duration.
  bool segment_sap_aligned = true;
  /// Force subsegments to begin with stream access points. Actual subsegment
  /// duration may not be exactly what is specified by subsegment_duration.
  /// Setting to subsegment_sap_aligned to true but segment_sap_aligned to false
  /// is not allowed.
  bool subsegment_sap_aligned = true;
  /// Enable LL-DASH streaming.
  /// Each segment constists of many fragments, and each fragment contains one
  /// chunk. A chunk is the smallest unit and is constructed of a single moof
  /// and mdat atom. Each chunk is uploaded immediately upon creation,
  /// decoupling latency from segment duration.
  bool low_latency_dash_mode = false;

  /// Indicates the startNumber in DASH SegmentTemplate and HLS segment name.
  int64_t start_segment_number = 1;

  // For DVB-Teletext in MPEG-2 TS: timing offset (in 90kHz ticks) between
  // video PTS timestamps and text segment generation. This compensates for
  // the pipeline delay where video is processed ahead of teletext.
  // Default is 90000 (1 second). If the value is too large, heartbeat-
  // triggered text segments are generated later than video segments.
  // If too small, some text cues may be absent in the output.
  int64_t ts_ttx_heartbeat_shift = kDefaultTtxHeartbeatShift;
};

}  // namespace shaka

#endif  // PACKAGER_PUBLIC_CHUNKING_PARAMS_H_
