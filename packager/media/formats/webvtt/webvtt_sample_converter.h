// Copyright 2015 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_FORMATS_WEBVTT_WEBVTT_SAMPLE_CONVERTER_H_
#define PACKAGER_MEDIA_FORMATS_WEBVTT_WEBVTT_SAMPLE_CONVERTER_H_

#include <stdint.h>
#include <list>

#include "packager/media/formats/mp4/box.h"
#include "packager/media/formats/mp4/box_definitions.h"
#include "packager/media/formats/webvtt/cue.h"
#include "packager/status.h"

namespace shaka {
namespace media {

/// Appends box to vector.
/// @param box is the box to be serialized.
/// @param output_vector is where the data is appended.
void AppendBoxToVector(mp4::Box* box, std::vector<uint8_t>* output_vector);

/// According to the spec, when cues overlap, samples must be created.\n
/// The example below has 2 WebVTT cues:\n
/// 00:01:00.000 --> 00:02:00.000\n
/// hello\n
///\n
/// 00:01:15.000 --> 00:02:15.000\n
/// how are you?\n
///\n
/// These are added (AddSample()) as 2 samples but must be split into 3 samples
/// and 4 cues ('vttc' boxes).\n
/// First sample:\n
///  start_time: 00:01:00.000\n
///  duration: 15 seconds\n
///  cue payload: hello\n
///\n
/// Second sample:\n
///  start_time: 00:01:15.000\n
///  duration: 45 seconds\n
///  cue payload: hello\n
///  cue payload: how are you?\n
///\n
/// Third sample:\n
///  start_time: 00:02:00.000\n
///  duration: 15 seconds\n
///  cue payload: how are you?\n
///\n
/// This class buffers the samples that are passed to AddSample() and creates
/// more samples as necessary.
/// Methods are virtual only for mocking, not intended for inheritance.
class WebVttSampleConverter {
 public:
  WebVttSampleConverter();
  virtual ~WebVttSampleConverter();

  /// Add a webvtt cue.
  /// @param cue is a webvtt cue.
  virtual void PushCue(const Cue& cue);

  /// Process all the buffered samples.
  /// This finalizes the object and further calls to PushSample() may result in
  /// an undefined behavior.
  virtual void Flush();

  /// @return The number of samples that are processed and ready to be popped.
  virtual size_t ReadySamplesSize();

  /// Returns a MediaSample that is non-overlapping with the previous samples
  /// that it has output. The data in the sample is one or more ISO-BMFF boxes
  /// for the duration of the sample.
  /// @return The first sample that is ready to be processed.
  virtual std::shared_ptr<MediaSample> PopSample();

 private:
  // Handle |cues_| except the last item, and create samples from them.
  // All cues that overlap with the latest cue are not processed.
  // Usually the last cue (and cues that overlap with it) should not be
  // processed right away because the following cues may overlap with the latest
  // cue or the existing cues.
  // If a cue has been proceessed, then this returns true.
  bool HandleAllCuesButLatest();

  // Same as HandleAllCuesButLatest() but it also includes the latest cue.
  // If a cue has been processed, then this returns true.
  bool HandleAllCues();

  // Sweep line algorithm that handles the cues in |cues_|.
  // This does not erase |cues_|.
  // If a cue has been processed, this returns true.
  // |sweep_line| is the start time and |sweep_stop_time| is when the sweep
  // should stop.
  bool SweepCues(uint64_t sweep_line, uint64_t sweep_stop_time);

  // This is going to be in 'mdat' box. Keep this around until a sample is
  // ready.
  std::list<Cue> cues_;

  // For comment samples.
  std::list<mp4::VTTAdditionalTextBox> additional_texts_;

  // Samples that are ready to be processed.
  std::list<std::shared_ptr<MediaSample>> ready_samples_;

  // This keeps track of the max end time of the processed cues which is the
  // start time of the next cue. Used to check if cue_current_time has to be set
  // or an empty cue (gap) has to be added.
  uint64_t next_cue_start_time_;

  DISALLOW_COPY_AND_ASSIGN(WebVttSampleConverter);
};

}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_FORMATS_WEBVTT_WEBVTT_SAMPLE_CONVERTER_H_
