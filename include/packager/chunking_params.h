// Copyright 2017 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_PUBLIC_CHUNKING_PARAMS_H_
#define PACKAGER_PUBLIC_CHUNKING_PARAMS_H_

namespace shaka {

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

  /// Used to set the decode time for only for timed text packing, specifically
  /// when packaging from VTT to VTT in MP4 or TTML in MP4.
  int64_t timed_text_decode_time = -1;

  /// Enable VTT text chunking adjustment when the sample end time falls outside
  /// the segment end time.
  bool adjust_sample_boundaries = false;

  /// Indicates the startNumber in DASH SegmentTemplate and HLS segment name.
  int64_t start_segment_number = 1;
};

}  // namespace shaka

#endif  // PACKAGER_PUBLIC_CHUNKING_PARAMS_H_
