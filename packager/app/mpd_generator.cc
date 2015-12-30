// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/app/mpd_generator_flags.h"
#include "packager/app/vlog_flags.h"
#include "packager/base/at_exit.h"
#include "packager/base/command_line.h"
#include "packager/base/logging.h"
#include "packager/base/strings/string_split.h"
#include "packager/base/strings/stringprintf.h"
#include "packager/mpd/util/mpd_writer.h"
#include "packager/version/version.h"

namespace edash_packager {
namespace {
const char kUsage[] =
    "MPD generation driver program.\n"
    "This program accepts MediaInfo files in human readable text "
    "format and outputs an MPD.\n"
    "The main use case for this is to output MPD for VOD.\n"
    "Limitations:\n"
    " Each MediaInfo can only have one of VideoInfo, AudioInfo, or TextInfo.\n"
    " There will be at most 3 AdaptationSets in the MPD, i.e. 1 video, 1 "
    "audio, and 1 text.\n"
    "Sample Usage:\n"
    "%s --input=\"video1.media_info,video2.media_info,audio1.media_info\" "
    "--output=\"video_audio.mpd\"";

enum ExitStatus {
  kSuccess = 0,
  kEmptyInputError,
  kEmptyOutputError,
  kFailedToWriteMpdToFileError
};

ExitStatus CheckRequiredFlags() {
  if (FLAGS_input.empty()) {
    LOG(ERROR) << "--input is required.";
    return kEmptyInputError;
  }

  if (FLAGS_output.empty()) {
    LOG(ERROR) << "--output is required.";
    return kEmptyOutputError;
  }

  return kSuccess;
}

ExitStatus RunMpdGenerator() {
  DCHECK_EQ(CheckRequiredFlags(), kSuccess);
  std::vector<std::string> base_urls;
  std::vector<std::string> input_files;
  typedef std::vector<std::string>::const_iterator Iterator;

  base::SplitString(FLAGS_input, ',', &input_files);

  if (!FLAGS_base_urls.empty()) {
    base::SplitString(FLAGS_base_urls, ',', &base_urls);
  }

  edash_packager::MpdWriter mpd_writer;
  for (Iterator it = base_urls.begin(); it != base_urls.end(); ++it)
    mpd_writer.AddBaseUrl(*it);

  for (Iterator it = input_files.begin(); it != input_files.end(); ++it) {
    if (!mpd_writer.AddFile(it->c_str(), FLAGS_output)) {
      LOG(WARNING) << "MpdWriter failed to read " << *it << ", skipping.";
    }
  }

  if (!mpd_writer.WriteMpdToFile(FLAGS_output.c_str())) {
    LOG(ERROR) << "Failed to write MPD to " << FLAGS_output;
    return kFailedToWriteMpdToFileError;
  }

  return kSuccess;
}

int MpdMain(int argc, char** argv) {
  base::AtExitManager exit;
  // Needed to enable VLOG/DVLOG through --vmodule or --v.
  base::CommandLine::Init(argc, argv);
  CHECK(logging::InitLogging(logging::LoggingSettings()));

  google::SetUsageMessage(base::StringPrintf(kUsage, argv[0]));
  google::ParseCommandLineFlags(&argc, &argv, true);

  ExitStatus status = CheckRequiredFlags();
  if (status != kSuccess) {
    std::string version_string =
        base::StringPrintf("mpd_generator version %s", kPackagerVersion);
    google::ShowUsageWithFlags(version_string.c_str());
    return status;
  }

  return RunMpdGenerator();
}

}  // namespace
}  // namespace edash_packager

int main(int argc, char** argv) {
  return edash_packager::MpdMain(argc, argv);
}
