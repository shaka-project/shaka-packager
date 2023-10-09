// Copyright 2015 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd
//
// Defines verbose logging flags.

#include <absl/strings/numbers.h>
#include <packager/app/vlog_flags.h>
#include <packager/kv_pairs/kv_pairs.h>

ABSL_FLAG(int32_t,
          v,
          0,
          "Show all VLOG(m) or DVLOG(m) messages for m <= this. "
          "Overridable by --vmodule.");

ABSL_FLAG(
    std::string,
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

// logging.h defines FLAGS_v and FLAGS_vmodule in terms of the gflags library,
// which we do not use.  Here we use macros to rename those symbols to avoid a
// conflict with the flags we defined above using absl.  When we switch from
// glog to absl::logging, this workaround should be removed.
#define FLAGS_v GLOG_FLAGS_v
#define FLAGS_vmodule GLOG_FLAGS_vmodule
#include <glog/logging.h>
#undef FLAGS_vmodule
#undef FLAGS_v

namespace shaka {

void register_flags_with_glog() {
  auto vlog_level = absl::GetFlag(FLAGS_v);
  if (vlog_level != 0) {
    google::SetVLOGLevel("*", vlog_level);
  }

  std::string vmodule_patterns = absl::GetFlag(FLAGS_vmodule);
  if (!vmodule_patterns.empty()) {
    std::vector<KVPair> patterns =
        SplitStringIntoKeyValuePairs(vmodule_patterns, '=', ',');
    int pattern_vlevel;

    for (const auto& pattern : patterns) {
      if (!::absl::SimpleAtoi(pattern.second, &pattern_vlevel)) {
        LOG(ERROR) << "Error parsing log level for '" << pattern.first
                   << "' from '" << pattern.second << "'";
        continue;
      }

      google::SetVLOGLevel(pattern.first.c_str(), pattern_vlevel);
    }
  }
}

}  // namespace shaka
