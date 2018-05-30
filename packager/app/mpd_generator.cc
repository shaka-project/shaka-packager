// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <iostream>

#include "packager/app/mpd_generator_flags.h"
#include "packager/app/vlog_flags.h"
#include "packager/base/at_exit.h"
#include "packager/base/command_line.h"
#include "packager/base/logging.h"
#include "packager/base/strings/string_split.h"
#include "packager/base/strings/stringprintf.h"
#include "packager/mpd/util/mpd_writer.h"
#include "packager/tools/license_notice.h"
#include "packager/version/version.h"

#if defined(OS_WIN)
#include <codecvt>
#include <functional>
#include <locale>
#endif  // defined(OS_WIN)

DEFINE_bool(licenses, false, "Dump licenses.");
DEFINE_string(test_packager_version,
              "",
              "Packager version for testing. Should be used for testing only.");

namespace shaka {
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
  typedef std::vector<std::string>::const_iterator Iterator;

  std::vector<std::string> input_files = base::SplitString(
      FLAGS_input, ",", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);

  if (!FLAGS_base_urls.empty()) {
    base_urls = base::SplitString(FLAGS_base_urls, ",", base::KEEP_WHITESPACE,
                                  base::SPLIT_WANT_ALL);
  }

  MpdWriter mpd_writer;
  for (Iterator it = base_urls.begin(); it != base_urls.end(); ++it)
    mpd_writer.AddBaseUrl(*it);

  for (const std::string& file : input_files) {
    if (!mpd_writer.AddFile(file)) {
      LOG(WARNING) << "MpdWriter failed to read " << file << ", skipping.";
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

  // Set up logging.
  logging::LoggingSettings log_settings;
  log_settings.logging_dest = logging::LOG_TO_SYSTEM_DEBUG_LOG;
  CHECK(logging::InitLogging(log_settings));

  google::SetVersionString(GetPackagerVersion());
  google::SetUsageMessage(base::StringPrintf(kUsage, argv[0]));
  google::ParseCommandLineFlags(&argc, &argv, true);
  if (FLAGS_licenses) {
    for (const char* line : kLicenseNotice)
      std::cout << line << std::endl;
    return kSuccess;
  }

  ExitStatus status = CheckRequiredFlags();
  if (status != kSuccess) {
    google::ShowUsageWithFlags("Usage");
    return status;
  }

  if (!FLAGS_test_packager_version.empty())
    SetPackagerVersionForTesting(FLAGS_test_packager_version);

  return RunMpdGenerator();
}

}  // namespace
}  // namespace shaka

#if defined(OS_WIN)
// Windows wmain, which converts wide character arguments to UTF-8.
int wmain(int argc, wchar_t* argv[], wchar_t* envp[]) {
  std::unique_ptr<char* [], std::function<void(char**)>> utf8_argv(
      new char*[argc], [argc](char** utf8_args) {
        // TODO(tinskip): This leaks, but if this code is enabled, it crashes.
        // Figure out why. I suspect gflags does something funny with the
        // argument array.
        // for (int idx = 0; idx < argc; ++idx)
        //   delete[] utf8_args[idx];
        delete[] utf8_args;
      });
  std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
  for (int idx = 0; idx < argc; ++idx) {
    std::string utf8_arg(converter.to_bytes(argv[idx]));
    utf8_arg += '\0';
    utf8_argv[idx] = new char[utf8_arg.size()];
    memcpy(utf8_argv[idx], &utf8_arg[0], utf8_arg.size());
  }
  return shaka::MpdMain(argc, utf8_argv.get());
}
#else
int main(int argc, char** argv) {
  return shaka::MpdMain(argc, argv);
}
#endif  // !defined(OS_WIN)
