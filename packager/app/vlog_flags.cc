// Copyright 2015 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd
//
// Defines verbose logging flags.

#include <packager/app/vlog_flags.h>

#include <absl/log/globals.h>
#include <absl/log/log.h>
#include <absl/strings/numbers.h>

#include <packager/kv_pairs/kv_pairs.h>
#include <packager/macros/logging.h>

ABSL_FLAG(int,
          v,
          0,
          "Show all VLOG(m) or DVLOG(m) messages for m <= this. "
          "Overridable by --vmodule.");

ABSL_FLAG(
    std::string,
    vmodule,
    "",
    "Per-module verbose level. THIS FLAG IS DEPRECATED. "
    "Argument is a comma-separated list of <module name>=<log level>. "
    "The logging system no longer supports different levels for different "
    "modules, so the verbosity level will be set to the maximum specified for "
    "any module or given by --v.");

ABSL_DECLARE_FLAG(int, minloglevel);

namespace shaka {

void handle_vlog_flags() {
  // Reference the log level flag to keep the absl::log flags from getting
  // stripped from the executable.
  int log_level = absl::GetFlag(FLAGS_minloglevel);
  (void)log_level;

  int vlog_level = absl::GetFlag(FLAGS_v);
  std::string vmodule_patterns = absl::GetFlag(FLAGS_vmodule);

  if (!vmodule_patterns.empty()) {
    std::vector<KVPair> patterns =
        SplitStringIntoKeyValuePairs(vmodule_patterns, '=', ',');
    int pattern_vlevel;
    bool warning_shown = false;

    for (const auto& pattern : patterns) {
      if (!warning_shown) {
        LOG(WARNING) << "--vmodule ignored, combined with --v!";
        warning_shown = true;
      }

      if (!::absl::SimpleAtoi(pattern.second, &pattern_vlevel)) {
        LOG(ERROR) << "Error parsing log level for '" << pattern.first
                   << "' from '" << pattern.second << "'";
        continue;
      }
    }
  }

  if (vlog_level != 0) {
    absl::SetMinLogLevel(static_cast<absl::LogSeverityAtLeast>(-vlog_level));
  }
}

}  // namespace shaka
