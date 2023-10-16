// Copyright 2017 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd
//
// Defines retired / deprecated flags. These flags will be removed in later
// versions.

#include <packager/app/retired_flags.h>

#include <cstdio>

ABSL_FLAG(std::string, profile, "", "This flag is deprecated. Do not use.");
ABSL_FLAG(bool, single_segment, true, "This flag is deprecated. Do not use.");
ABSL_FLAG(bool,
          webm_subsample_encryption,
          true,
          "This flag is deprecated. Use vp9_subsample_encryption instead.");
ABSL_FLAG(double,
          availability_time_offset,
          0,
          "This flag is deprecated. Use suggested_presentation_delay "
          "instead which can achieve similar effect.");
ABSL_FLAG(std::string,
          playready_key_id,
          "",
          "This flag is deprecated. Use --enable_raw_key_encryption with "
          "--generate_playready_pssh to generate PlayReady PSSH.");
ABSL_FLAG(std::string,
          playready_key,
          "",
          "This flag is deprecated. Use --enable_raw_key_encryption with "
          "--generate_playready_pssh to generate PlayReady PSSH.");
ABSL_FLAG(bool,
          mp4_use_decoding_timestamp_in_timeline,
          false,
          "This flag is deprecated. Do not use.");
ABSL_FLAG(
    int32_t,
    num_subsegments_per_sidx,
    0,
    "This flag is deprecated. Use --generate_sidx_in_media_segments instead.");
ABSL_FLAG(bool,
          generate_widevine_pssh,
          false,
          "This flag is deprecated. Use --protection_systems instead.");
ABSL_FLAG(bool,
          generate_playready_pssh,
          false,
          "This flag is deprecated. Use --protection_systems instead.");
ABSL_FLAG(bool,
          generate_common_pssh,
          false,
          "This flag is deprecated. Use --protection_systems instead.");
ABSL_FLAG(bool,
          generate_static_mpd,
          false,
          "This flag is deprecated. Use --generate_static_live_mpd instead.");

// The current gflags library does not provide a way to check whether a flag is
// set in command line. If a flag has a different value to its default value,
// the flag must have been set. It is possible that the flag is set to the same
// value as its default value though.
bool InformRetiredStringFlag(const char* flagname, const std::string& value) {
  if (!value.empty())
    fprintf(stderr, "WARNING: %s is deprecated and ignored.\n", flagname);
  return true;
}

bool InformRetiredDefaultTrueFlag(const char* flagname, bool value) {
  if (!value)
    fprintf(stderr, "WARNING: %s is deprecated and ignored.\n", flagname);
  return true;
}

bool InformRetiredDefaultFalseFlag(const char* flagname, bool value) {
  if (value)
    fprintf(stderr, "WARNING: %s is deprecated and ignored.\n", flagname);
  return true;
}

bool InformRetiredDefaultDoubleFlag(const char* flagname, double value) {
  if (value != 0)
    fprintf(stderr, "WARNING: %s is deprecated and ignored.\n", flagname);
  return true;
}

bool InformRetiredDefaultInt32Flag(const char* flagname, int32_t value) {
  if (value != 0)
    fprintf(stderr, "WARNING: %s is deprecated and ignored.\n", flagname);
  return true;
}

bool InformRetiredPsshGenerationFlag(const char* flagname, bool value) {
  if (value) {
    fprintf(stderr,
            "WARNING: %s is deprecated and ignored. Please switch to "
            "--protection_systems.\n",
            flagname);
  }
  return true;
}

bool InformRetiredGenerateStaticMpdFlag(const char* flagname, bool value) {
  if (value) {
    fprintf(stderr,
            "WARNING: %s is deprecated and ignored. Please switch to "
            "--generate_static_live_mpd.\n",
            flagname);
  }
  return true;
}

namespace shaka {
bool ValidateRetiredFlags() {
  bool success = true;

  auto profile = absl::GetFlag(FLAGS_profile);
  if (!InformRetiredStringFlag("profile", profile)) {
    success = false;
  }

  auto single_segment = absl::GetFlag(FLAGS_single_segment);
  if (!InformRetiredDefaultTrueFlag("single_segment", single_segment)) {
    success = false;
  }
  auto webm_subsample_encryption =
      absl::GetFlag(FLAGS_webm_subsample_encryption);
  if (!InformRetiredDefaultTrueFlag("webm_subsample_encryption",
                                    webm_subsample_encryption)) {
    success = false;
  }
  auto availability_time_offset = absl::GetFlag(FLAGS_availability_time_offset);
  if (!InformRetiredDefaultDoubleFlag("availability_time_offset",
                                      availability_time_offset)) {
    success = false;
  }
  auto playready_key_id = absl::GetFlag(FLAGS_playready_key_id);
  if (!InformRetiredStringFlag("playready_key_id", playready_key_id)) {
    success = false;
  }
  auto playready_key = absl::GetFlag(FLAGS_playready_key);
  if (!InformRetiredStringFlag("playready_key", playready_key)) {
    success = false;
  }
  auto mp4_use_decoding_timestamp_in_timeline =
      absl::GetFlag(FLAGS_mp4_use_decoding_timestamp_in_timeline);
  if (!InformRetiredDefaultFalseFlag("mp4_use_decoding_timestamp_in_timeline",
                                     mp4_use_decoding_timestamp_in_timeline)) {
    success = false;
  }
  auto num_subsegments_per_sidx = absl::GetFlag(FLAGS_num_subsegments_per_sidx);
  if (!InformRetiredDefaultInt32Flag("num_subsegments_per_sidx",
                                     num_subsegments_per_sidx)) {
    success = false;
  }
  auto generate_widevine_pssh = absl::GetFlag(FLAGS_generate_widevine_pssh);
  if (!InformRetiredPsshGenerationFlag("generate_widevine_pssh",
                                       generate_widevine_pssh)) {
    success = false;
  }
  auto generate_playready_pssh = absl::GetFlag(FLAGS_generate_playready_pssh);
  if (!InformRetiredPsshGenerationFlag("generate_playready_pssh",
                                       generate_playready_pssh)) {
    success = false;
  }
  auto generate_common_pssh = absl::GetFlag(FLAGS_generate_common_pssh);
  if (!InformRetiredPsshGenerationFlag("generate_common_pssh",
                                       generate_common_pssh)) {
    success = false;
  }
  auto generate_static_mpd = absl::GetFlag(FLAGS_generate_static_mpd);
  if (!InformRetiredGenerateStaticMpdFlag("generate_static_mpd",
                                          generate_static_mpd)) {
    success = false;
  }

  return success;
}
}  // namespace shaka
