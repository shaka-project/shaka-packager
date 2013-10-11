// Copyright (c) 2013 Google Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_MUXER_H_
#define MEDIA_BASE_MUXER_H_

#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "media/base/status.h"

namespace media {

class Encryptor;
class EncryptorSource;
class MediaSample;
class MediaStream;

class Muxer {
 public:
  struct Options {
    // Generate a single segment for each media presentation. This option
    // should be set for on demand profile.
    bool single_segment;

    // Segment duration. If single_segment is specified, this parameter sets
    // the duration of a subsegment. A subsegment can contain one to several
    // fragments.
    int segment_duration;

    // Fragment duration. Should not be larger than the segment duration.
    int fragment_duration;

    // Force segments to begin with stream access points. Segment duration may
    // not be exactly what asked by segment_duration.
    bool segment_sap_aligned;

    // Force fragments to begin with stream access points. Fragment duration
    // may not be exactly what asked by segment_duration. Imply
    // segment_sap_aligned.
    bool fragment_sap_aligned;

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

    // Specify a directory for temporary file creation.
    std::string temp_directory_name;
  };

  // Muxer does not take over the ownership of encryptor_source.
  Muxer(const Options& options, EncryptorSource* encryptor_source);
  virtual ~Muxer();

  // Adds video/audio stream.
  // Returns OK on success.
  virtual Status AddStream(MediaStream* stream);

  // Adds new media sample.
  virtual Status AddSample(const MediaStream* stream,
                           scoped_refptr<MediaSample> sample) = 0;

  // Final clean up.
  virtual Status Finalize() = 0;

  // Drives the remuxing from muxer side (pull).
  virtual Status Run();

  uint32 num_streams() const {
    return streams_.size();
  }

  MediaStream* streams(uint32 index) const {
    if (index >= streams_.size())
      return NULL;
    return streams_[index];
  }

 private:
  EncryptorSource* encryptor_source_;
  Encryptor* encryptor_;
  std::vector<MediaStream*> streams_;

  DISALLOW_COPY_AND_ASSIGN(Muxer);
};

}  // namespace media

#endif  // MEDIA_BASE_MUXER_H_
