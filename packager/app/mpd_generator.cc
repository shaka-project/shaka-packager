// Copyright 2014 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <iostream>

#if defined(OS_WIN)
#include <codecvt>
#include <functional>
#endif  // defined(OS_WIN)

#include <absl/flags/parse.h>
#include <absl/flags/usage.h>
#include <absl/flags/usage_config.h>
#include <absl/log/check.h>
#include <absl/log/initialize.h>
#include <absl/log/log.h>
#include <absl/strings/str_format.h>
#include <absl/strings/str_split.h>

#include <packager/app/mpd_generator_flags.h>
#include <packager/app/vlog_flags.h>
#include <packager/mpd/util/mpd_writer.h>
#include <packager/tools/license_notice.h>
#include <packager/version/version.h>

ABSL_FLAG(bool, licenses, false, "Dump licenses.");
ABSL_FLAG(std::string,
          test_packager_version,
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
  if (absl::GetFlag(FLAGS_input).empty()) {
    LOG(ERROR) << "--input is required.";
    return kEmptyInputError;
  }

  if (absl::GetFlag(FLAGS_output).empty()) {
    LOG(ERROR) << "--output is required.";
    return kEmptyOutputError;
  }

  return kSuccess;
}

ExitStatus RunMpdGenerator() {
  DCHECK_EQ(CheckRequiredFlags(), kSuccess);
  std::vector<std::string> base_urls;
  typedef std::vector<std::string>::const_iterator Iterator;

  std::vector<std::string> input_files =
      absl::StrSplit(absl::GetFlag(FLAGS_input), ",", absl::AllowEmpty());

  if (!absl::GetFlag(FLAGS_base_urls).empty()) {
    base_urls =
        absl::StrSplit(absl::GetFlag(FLAGS_base_urls), ",", absl::AllowEmpty());
  }

  MpdWriter mpd_writer;
  for (Iterator it = base_urls.begin(); it != base_urls.end(); ++it)
    mpd_writer.AddBaseUrl(*it);

  for (const std::string& file : input_files) {
    if (!mpd_writer.AddFile(file)) {
      LOG(WARNING) << "MpdWriter failed to read " << file << ", skipping.";
    }
  }

  if (!mpd_writer.WriteMpdToFile(absl::GetFlag(FLAGS_output).c_str())) {
    LOG(ERROR) << "Failed to write MPD to " << absl::GetFlag(FLAGS_output);
    return kFailedToWriteMpdToFileError;
  }

  return kSuccess;
}

int MpdMain(int argc, char** argv) {
  absl::FlagsUsageConfig flag_config;
  flag_config.version_string = []() -> std::string {
    return "mpd_generator version " + GetPackagerVersion() + "\n";
  };
  flag_config.contains_help_flags =
      [](absl::string_view flag_file_name) -> bool { return true; };
  absl::SetFlagsUsageConfig(flag_config);

  auto usage = absl::StrFormat(kUsage, argv[0]);
  absl::SetProgramUsageMessage(usage);
  absl::ParseCommandLine(argc, argv);

  if (absl::GetFlag(FLAGS_licenses)) {
    for (const char* line : kLicenseNotice)
      std::cout << line << std::endl;
    return kSuccess;
  }

  ExitStatus status = CheckRequiredFlags();
  if (status != kSuccess) {
    std::cerr << "Usage " << absl::ProgramUsageMessage();
    return status;
  }

  handle_vlog_flags();

  absl::InitializeLog();

  if (!absl::GetFlag(FLAGS_test_packager_version).empty())
    SetPackagerVersionForTesting(absl::GetFlag(FLAGS_test_packager_version));

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

  // Because we just converted wide character args into UTF8, and because
  // std::filesystem::u8path is used to interpret all std::string paths as
  // UTF8, we should set the locale to UTF8 as well, for the transition point
  // to C library functions like fopen to work correctly with non-ASCII paths.
  std::setlocale(LC_ALL, ".UTF8");

  return shaka::MpdMain(argc, utf8_argv.get());
}
#else
int main(int argc, char** argv) {
  return shaka::MpdMain(argc, argv);
}
#endif  // !defined(OS_WIN)
