// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd
//
// Defines Muxer flags.

#include "app/muxer_flags.h"

DEFINE_string(stream,
              "video",
              "Add the specified stream to muxer. Allowed values, 'video' - "
              "the first video stream; or 'audio' - the first audio stream; or "
              "zero based stream id.");
DEFINE_string(output,
              "",
              "Output file path. If segment_template is not specified, "
              "the muxer generates this single output file with all "
              "segments concatenated; Otherwise, it specifies the "
              "initialization segment name.");
DEFINE_string(segment_template,
              "",
              "Segment template pattern for generated segments. It should "
              "comply with ISO/IEC 23009-1:2012 5.3.9.4.4.");
