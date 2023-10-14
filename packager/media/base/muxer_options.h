// Copyright 2014 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_BASE_MUXER_OPTIONS_H_
#define PACKAGER_MEDIA_BASE_MUXER_OPTIONS_H_

#include <cstdint>
#include <string>

#include <packager/mp4_output_params.h>

namespace shaka {
namespace media {

/// This structure contains the list of configuration options for Muxer.
struct MuxerOptions {
  MuxerOptions();
  ~MuxerOptions();

  /// MP4 (ISO-BMFF) specific parameters.
  Mp4OutputParams mp4_params;

  // A positive value, in milliseconds, by which output timestamps are offset to
  // compensate for negative timestamps in the input.
  int32_t transport_stream_timestamp_offset_ms = 0;

  /// Output file name. If segment_template is not specified, the Muxer
  /// generates this single output file with all segments concatenated;
  /// Otherwise, it specifies the init segment name.
  std::string output_file_name;

  /// Specify output segment name pattern for generated segments. It can
  /// furthermore be configured by using a subset of the SegmentTemplate
  /// identifiers: $RepresentationID$, $Number$, $Bandwidth$ and $Time.
  /// Optional.
  std::string segment_template;

  /// Specify temporary directory for intermediate files.
  std::string temp_dir;

  /// User-specified bit rate for the media stream. If zero, the muxer will
  /// attempt to estimate.
  uint32_t bandwidth = 0;
};

}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_BASE_MUXER_OPTIONS_H_
