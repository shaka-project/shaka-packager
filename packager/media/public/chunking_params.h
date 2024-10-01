// Copyright 2017 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_PUBLIC_CHUNKING_PARAMS_H_
#define PACKAGER_MEDIA_PUBLIC_CHUNKING_PARAMS_H_

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

  // A shift in PTS values for text heart beats from other MPEG-2 TS
  // elementary streams. The purpose is to generate text chunks
  // at roughly the same time as the other media, even when there is no data
  // on the text pid. The scale is 90000 and the value should typically be
  // 1-2 seconds to avoid that premature chunk generation before true cue data
  // has arrived.
  int64_t ts_text_trigger_shift = 180000;
};

}  // namespace shaka

#endif  // PACKAGER_MEDIA_PUBLIC_CHUNKING_PARAMS_H_
