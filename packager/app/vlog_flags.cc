// Copyright 2015 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd
//
// Defines verbose logging flags.

#include "packager/app/vlog_flags.h"

DEFINE_int32(v,
             0,
             "Show all VLOG(m) or DVLOG(m) messages for m <= this. "
             "Overridable by --vmodule.");
DEFINE_string(
    vmodule,
    "",
    "Per-module verbose level."
    "Argument is a comma-separated list of <module name>=<log level>. "
    "<module name> is a glob pattern, matched against the filename base "
    "(that is, name ignoring .cc/.h./-inl.h). "
    "A pattern without slashes matches just the file name portion, otherwise "
    "the whole file path (still without .cc/.h./-inl.h) is matched. "
    "? and * in the glob pattern match any single or sequence of characters "
    "respectively including slashes. "
    "<log level> overrides any value given by --v.");
