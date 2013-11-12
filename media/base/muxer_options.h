// Copyright (c) 2013 Google Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_MUXER_OPTIONS_H_
#define MEDIA_BASE_MUXER_OPTIONS_H_

#include <string>

namespace media {

struct MuxerOptions {
  // Generate a single segment for each media presentation. This option
  // should be set for on demand profile.
  bool single_segment;

  // Segment duration in seconds. If single_segment is specified, this parameter
  // sets the duration of a subsegment; otherwise, this parameter sets the
  // duration of a segment. A segment can contain one to many fragments.
  double segment_duration;

  // Fragment duration in seconds. Should not be larger than the segment
  // duration.
  double fragment_duration;

  // Force segments to begin with stream access points. Segment duration may
  // not be exactly what asked by segment_duration.
  bool segment_sap_aligned;

  // Force fragments to begin with stream access points. Fragment duration
  // may not be exactly what asked by segment_duration. Setting to true implies
  // that segment_sap_aligned is true as well.
  bool fragment_sap_aligned;

  // For ISO BMFF only.
  // Set the number of subsegments in each SIDX box. If 0, a single SIDX box
  // is used per segment. If -1, no SIDX box is used. Otherwise, the Muxer
  // will pack N subsegments in the root SIDX of the segment, with
  // segment_duration/N/fragment_duration fragments per subsegment.
  int num_subsegments_per_sidx;

  // Output file name. If segment_template is not specified, the Muxer
  // generates this single output file with all segments concatenated;
  // Otherwise, it specifies the init segment name.
  std::string output_file_name;

  // Specify output segment name pattern for generated segments. It can
  // furthermore be configured by using a subset of the SegmentTemplate
  // identifiers: $RepresentationID$, $Number$, $Bandwidth$ and $Time.
  // Optional.
  std::string segment_template;

  // Specify the temporary file for on demand media file creation.
  std::string temp_file_name;
};

}  // namespace media

#endif  // MEDIA_BASE_MUXER_OPTIONS_H_
