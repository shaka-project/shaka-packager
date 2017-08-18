// Copyright 2017 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_PUBLIC_MP4_OUTPUT_PARAMS_H_
#define PACKAGER_MEDIA_PUBLIC_MP4_OUTPUT_PARAMS_H_

namespace shaka {

/// MP4 (ISO-BMFF) output related parameters.
struct Mp4OutputParams {
  // Include pssh in the encrypted stream. CMAF and DASH-IF recommends carrying
  // license acquisition information in the manifest and not duplicate the
  // information in the stream. (This is not a hard requirement so we are still
  // CMAF compatible even if pssh is included in the stream.)
  bool include_pssh_in_stream = true;
  /// Set the number of subsegments in each SIDX box. If 0, a single SIDX box
  /// is used per segment. If -1, no SIDX box is used. Otherwise, the Muxer
  /// will pack N subsegments in the root SIDX of the segment, with
  /// segment_duration/N/subsegment_duration fragments per subsegment.
  /// This flag is ingored for DASH MPD with on-demand profile.
  static constexpr int kNoSidxBoxInSegment = -1;
  static constexpr int kSingleSidxPerSegment = 0;
  int num_subsegments_per_sidx = kSingleSidxPerSegment;
  /// Set the flag use_decoding_timestamp_in_timeline, which if set to true, use
  /// decoding timestamp instead of presentation timestamp in media timeline,
  /// which is needed to workaround a Chromium bug that decoding timestamp is
  /// used in buffered range, https://crbug.com/398130.
  bool use_decoding_timestamp_in_timeline = false;
};

}  // namespace shaka

#endif  // PACKAGER_MEDIA_PUBLIC_MP4_OUTPUT_PARAMS_H_
