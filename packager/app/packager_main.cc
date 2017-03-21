// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <gflags/gflags.h>
#include <iostream>

#include "packager/app/fixed_key_encryption_flags.h"
#include "packager/app/hls_flags.h"
#include "packager/app/libcrypto_threading.h"
#include "packager/app/mpd_flags.h"
#include "packager/app/muxer_flags.h"
#include "packager/app/packager_util.h"
#include "packager/app/playready_key_encryption_flags.h"
#include "packager/app/stream_descriptor.h"
#include "packager/app/vlog_flags.h"
#include "packager/app/widevine_encryption_flags.h"
#include "packager/base/at_exit.h"
#include "packager/base/command_line.h"
#include "packager/base/files/file_path.h"
#include "packager/base/logging.h"
#include "packager/base/path_service.h"
#include "packager/base/strings/string_split.h"
#include "packager/base/strings/stringprintf.h"
#include "packager/base/threading/simple_thread.h"
#include "packager/base/time/clock.h"
#include "packager/hls/base/hls_notifier.h"
#include "packager/hls/base/simple_hls_notifier.h"
#include "packager/media/base/container_names.h"
#include "packager/media/base/fourccs.h"
#include "packager/media/base/key_source.h"
#include "packager/media/base/muxer_options.h"
#include "packager/media/base/muxer_util.h"
#include "packager/media/chunking/chunking_handler.h"
#include "packager/media/crypto/encryption_handler.h"
#include "packager/media/demuxer/demuxer.h"
#include "packager/media/event/hls_notify_muxer_listener.h"
#include "packager/media/event/mpd_notify_muxer_listener.h"
#include "packager/media/event/vod_media_info_dump_muxer_listener.h"
#include "packager/media/file/file.h"
#include "packager/media/formats/mp2t/ts_muxer.h"
#include "packager/media/formats/mp4/mp4_muxer.h"
#include "packager/media/formats/webm/webm_muxer.h"
#include "packager/media/trick_play/trick_play_handler.h"
#include "packager/mpd/base/dash_iop_mpd_notifier.h"
#include "packager/mpd/base/media_info.pb.h"
#include "packager/mpd/base/mpd_builder.h"
#include "packager/mpd/base/simple_mpd_notifier.h"
#include "packager/version/version.h"

#if defined(OS_WIN)
#include <codecvt>
#include <functional>
#include <locale>
#endif  // defined(OS_WIN)

DEFINE_bool(use_fake_clock_for_muxer,
            false,
            "Set to true to use a fake clock for muxer. With this flag set, "
            "creation time and modification time in outputs are set to 0. "
            "Should only be used for testing.");
DEFINE_bool(override_version,
            false,
            "Override packager version in the generated outputs with "
            "--test_version if it is set to true. Should be used for "
            "testing only.");
DEFINE_string(test_version,
              "",
              "Packager version for testing. Ignored if --override_version is "
              "false. Should be used for testing only.");

namespace shaka {
namespace media {
namespace {

const char kUsage[] =
    "%s [flags] <stream_descriptor> ...\n\n"
    "  stream_descriptor consists of comma separated field_name/value pairs:\n"
    "  field_name=value,[field_name=value,]...\n"
    "  Supported field names are as follows (names in parenthesis are alias):\n"
    "  - input (in): Required input/source media file path or network stream\n"
    "    URL.\n"
    "  - stream_selector (stream): Required field with value 'audio',\n"
    "    'video', 'text', or stream number (zero based).\n"
    "  - output (out,init_segment): Required output file (single file) or\n"
    "    initialization file path (multiple file).\n"
    "  - segment_template (segment): Optional value which specifies the\n"
    "    naming  pattern for the segment files, and that the stream should be\n"
    "    split into multiple files. Its presence should be consistent across\n"
    "    streams.\n"
    "  - bandwidth (bw): Optional value which contains a user-specified\n"
    "    content bit rate for the stream, in bits/sec. If specified, this\n"
    "    value is propagated to the $Bandwidth$ template parameter for\n"
    "    segment names. If not specified, its value may be estimated.\n"
    "  - language (lang): Optional value which contains a user-specified\n"
    "    language tag. If specified, this value overrides any language\n"
    "    metadata in the input track.\n"
    "  - output_format (format): Optional value which specifies the format\n"
    "    of the output files (MP4 or WebM).  If not specified, it will be\n"
    "    derived from the file extension of the output file.\n"
    "  - hls_name: Required for audio when outputting HLS.\n"
    "    name of the output stream. This is not (necessarily) the same as\n"
    "    output. This is used as the NAME attribute for EXT-X-MEDIA\n"
    "  - hls_group_id: Required for audio when outputting HLS.\n"
    "    The group ID for the output stream. For HLS this is used as the\n"
    "    GROUP-ID attribute for EXT-X-MEDIA.\n"
    "  - playlist_name: Required for HLS output.\n"
    "    Name of the playlist for the stream. Usually ends with '.m3u8'.\n";

const char kMediaInfoSuffix[] = ".media_info";

enum ExitStatus {
  kSuccess = 0,
  kArgumentValidationFailed,
  kPackagingFailed,
  kInternalError,
};

// TODO(rkuroiwa): Write TTML and WebVTT parser (demuxing) for a better check
// and for supporting live/segmenting (muxing).  With a demuxer and a muxer,
// CreateRemuxJobs() shouldn't treat text as a special case.
std::string DetermineTextFileFormat(const std::string& file) {
  std::string content;
  if (!File::ReadFileToString(file.c_str(), &content)) {
    LOG(ERROR) << "Failed to open file " << file
               << " to determine file format.";
    return "";
  }
  MediaContainerName container_name = DetermineContainer(
      reinterpret_cast<const uint8_t*>(content.data()), content.size());
  if (container_name == CONTAINER_WEBVTT) {
    return "vtt";
  } else if (container_name == CONTAINER_TTML) {
    return "ttml";
  }

  return "";
}

}  // namespace

// A fake clock that always return time 0 (epoch). Should only be used for
// testing.
class FakeClock : public base::Clock {
 public:
  base::Time Now() override { return base::Time(); }
};

// Demux, Mux(es) and worker thread used to remux a source file/stream.
class RemuxJob : public base::SimpleThread {
 public:
  RemuxJob(std::unique_ptr<Demuxer> demuxer)
      : SimpleThread("RemuxJob"), demuxer_(std::move(demuxer)) {}

  ~RemuxJob() override {}

  Demuxer* demuxer() { return demuxer_.get(); }
  Status status() { return status_; }

 private:
  void Run() override {
    DCHECK(demuxer_);
    status_ = demuxer_->Run();
  }

  std::unique_ptr<Demuxer> demuxer_;
  Status status_;

  DISALLOW_COPY_AND_ASSIGN(RemuxJob);
};

bool StreamInfoToTextMediaInfo(const StreamDescriptor& stream_descriptor,
                               const MuxerOptions& stream_muxer_options,
                               MediaInfo* text_media_info) {
  const std::string& language = stream_descriptor.language;
  std::string format = DetermineTextFileFormat(stream_descriptor.input);
  if (format.empty()) {
    LOG(ERROR) << "Failed to determine the text file format for "
               << stream_descriptor.input;
    return false;
  }

  if (!File::Copy(stream_descriptor.input.c_str(),
                  stream_muxer_options.output_file_name.c_str())) {
    LOG(ERROR) << "Failed to copy the input file (" << stream_descriptor.input
               << ") to output file (" << stream_muxer_options.output_file_name
               << ").";
    return false;
  }

  text_media_info->set_media_file_name(stream_muxer_options.output_file_name);
  text_media_info->set_container_type(MediaInfo::CONTAINER_TEXT);

  if (stream_muxer_options.bandwidth != 0) {
    text_media_info->set_bandwidth(stream_muxer_options.bandwidth);
  } else {
    // Text files are usually small and since the input is one file; there's no
    // way for the player to do ranged requests. So set this value to something
    // reasonable.
    text_media_info->set_bandwidth(256);
  }

  MediaInfo::TextInfo* text_info = text_media_info->mutable_text_info();
  text_info->set_format(format);
  if (!language.empty())
    text_info->set_language(language);

  return true;
}

std::shared_ptr<Muxer> CreateOutputMuxer(const MuxerOptions& options,
                                         MediaContainerName container) {
  if (container == CONTAINER_WEBM) {
    return std::shared_ptr<Muxer>(new webm::WebMMuxer(options));
  } else if (container == CONTAINER_MPEG2TS) {
    return std::shared_ptr<Muxer>(new mp2t::TsMuxer(options));
  } else {
    DCHECK_EQ(container, CONTAINER_MOV);
    return std::shared_ptr<Muxer>(new mp4::MP4Muxer(options));
  }
}

bool CreateRemuxJobs(const StreamDescriptorList& stream_descriptors,
                     const ChunkingOptions& chunking_options,
                     const EncryptionOptions& encryption_options,
                     const MuxerOptions& muxer_options,
                     FakeClock* fake_clock,
                     KeySource* encryption_key_source,
                     MpdNotifier* mpd_notifier,
                     hls::HlsNotifier* hls_notifier,
                     std::vector<std::unique_ptr<RemuxJob>>* remux_jobs) {
  // No notifiers OR (mpd_notifier XOR hls_notifier); which is NAND.
  DCHECK(!(mpd_notifier && hls_notifier));
  DCHECK(remux_jobs);

  std::shared_ptr<TrickPlayHandler> trick_play_handler;

  std::string previous_input;
  std::string previous_stream_selector;
  int stream_number = 0;
  for (StreamDescriptorList::const_iterator
           stream_iter = stream_descriptors.begin();
       stream_iter != stream_descriptors.end();
       ++stream_iter, ++stream_number) {
    // Process stream descriptor.
    MuxerOptions stream_muxer_options(muxer_options);
    stream_muxer_options.output_file_name = stream_iter->output;
    if (!stream_iter->segment_template.empty()) {
      if (!ValidateSegmentTemplate(stream_iter->segment_template)) {
        LOG(ERROR) << "ERROR: segment template with '"
                   << stream_iter->segment_template << "' is invalid.";
        return false;
      }
      stream_muxer_options.segment_template = stream_iter->segment_template;
    }
    stream_muxer_options.bandwidth = stream_iter->bandwidth;

    if (stream_iter->stream_selector == "text" &&
        stream_iter->output_format != CONTAINER_MOV) {
      MediaInfo text_media_info;
      if (!StreamInfoToTextMediaInfo(*stream_iter, stream_muxer_options,
                                     &text_media_info)) {
        return false;
      }

      if (mpd_notifier) {
        uint32_t unused;
        if (!mpd_notifier->NotifyNewContainer(text_media_info, &unused)) {
          LOG(ERROR) << "Failed to process text file " << stream_iter->input;
        } else {
          mpd_notifier->Flush();
        }
      } else if (FLAGS_output_media_info) {
        VodMediaInfoDumpMuxerListener::WriteMediaInfoToFile(
            text_media_info,
            stream_muxer_options.output_file_name + kMediaInfoSuffix);
      } else {
        NOTIMPLEMENTED()
            << "--mpd_output or --output_media_info flags are "
               "required for text output. Skipping manifest related output for "
            << stream_iter->input;
      }
      continue;
    }

    if (stream_iter->input != previous_input) {
      // New remux job needed. Create demux and job thread.
      std::unique_ptr<Demuxer> demuxer(new Demuxer(stream_iter->input));
      demuxer->set_dump_stream_info(FLAGS_dump_stream_info);
      if (FLAGS_enable_widevine_decryption ||
          FLAGS_enable_fixed_key_decryption) {
        std::unique_ptr<KeySource> decryption_key_source(
            CreateDecryptionKeySource());
        if (!decryption_key_source)
          return false;
        demuxer->SetKeySource(std::move(decryption_key_source));
      }
      remux_jobs->emplace_back(new RemuxJob(std::move(demuxer)));
      trick_play_handler.reset();
      previous_input = stream_iter->input;
      // Skip setting up muxers if output is not needed.
      if (stream_iter->output.empty() && stream_iter->segment_template.empty())
        continue;
    }
    DCHECK(!remux_jobs->empty());

    // Each stream selector requires an individual trick play handler.
    // E.g., an input with two video streams needs two trick play handlers.
    // TODO(hmchen): add a test case in packager_test.py for two video streams
    // input.
    if (stream_iter->stream_selector != previous_stream_selector) {
      previous_stream_selector = stream_iter->stream_selector;
      trick_play_handler.reset();
    }

    std::shared_ptr<Muxer> muxer(
        CreateOutputMuxer(stream_muxer_options, stream_iter->output_format));
    if (FLAGS_use_fake_clock_for_muxer) muxer->set_clock(fake_clock);

    std::unique_ptr<MuxerListener> muxer_listener;
    DCHECK(!(FLAGS_output_media_info && mpd_notifier));
    if (FLAGS_output_media_info) {
      const std::string output_media_info_file_name =
          stream_muxer_options.output_file_name + kMediaInfoSuffix;
      std::unique_ptr<VodMediaInfoDumpMuxerListener>
          vod_media_info_dump_muxer_listener(
              new VodMediaInfoDumpMuxerListener(output_media_info_file_name));
      muxer_listener = std::move(vod_media_info_dump_muxer_listener);
    }
    if (mpd_notifier) {
      std::unique_ptr<MpdNotifyMuxerListener> mpd_notify_muxer_listener(
          new MpdNotifyMuxerListener(mpd_notifier));
      muxer_listener = std::move(mpd_notify_muxer_listener);
    }

    if (hls_notifier) {
      // TODO(rkuroiwa): Do some smart stuff to group the audios, e.g. detect
      // languages.
      std::string group_id = stream_iter->hls_group_id;
      std::string name = stream_iter->hls_name;
      std::string hls_playlist_name = stream_iter->hls_playlist_name;
      if (group_id.empty())
        group_id = "audio";
      if (name.empty())
        name = base::StringPrintf("stream_%d", stream_number);
      if (hls_playlist_name.empty())
        hls_playlist_name = base::StringPrintf("stream_%d.m3u8", stream_number);

      muxer_listener.reset(new HlsNotifyMuxerListener(hls_playlist_name, name,
                                                      group_id, hls_notifier));
    }

    if (muxer_listener)
      muxer->SetMuxerListener(std::move(muxer_listener));

    // Create a new trick_play_handler. Note that the stream_decriptors
    // are sorted so that for the same input and stream_selector, the main
    // stream is always the last one following the trick play streams.
    if (stream_iter->trick_play_rate > 0) {
      if (!trick_play_handler) {
        trick_play_handler.reset(new TrickPlayHandler());
      }
      trick_play_handler->SetHandlerForTrickPlay(stream_iter->trick_play_rate,
                                                 std::move(muxer));
      if (trick_play_handler->IsConnected())
        continue;
    } else if (trick_play_handler) {
      trick_play_handler->SetHandlerForMainStream(std::move(muxer));
      DCHECK(trick_play_handler->IsConnected());
      continue;
    }

    std::vector<std::shared_ptr<MediaHandler>> handlers;

    auto chunking_handler = std::make_shared<ChunkingHandler>(chunking_options);
    handlers.push_back(chunking_handler);

    Status status;
    if (encryption_key_source) {
      auto new_encryption_options = encryption_options;
      // Use Sample AES in MPEG2TS.
      // TODO(kqyang): Consider adding a new flag to enable Sample AES as we
      // will support CENC in TS in the future.
      if (stream_iter->output_format == CONTAINER_MPEG2TS) {
        LOG(INFO) << "Use Apple Sample AES encryption for MPEG2TS.";
        new_encryption_options.protection_scheme =
            kAppleSampleAesProtectionScheme;
      }
      handlers.emplace_back(
          new EncryptionHandler(new_encryption_options, encryption_key_source));
    }

    // If trick_play_handler is available, muxer should already be connected to
    // trick_play_handler.
    if (trick_play_handler) {
      handlers.push_back(trick_play_handler);
    } else {
      handlers.push_back(std::move(muxer));
    }

    auto* demuxer = remux_jobs->back()->demuxer();
    const std::string& stream_selector = stream_iter->stream_selector;
    status.Update(demuxer->SetHandler(stream_selector, chunking_handler));
    status.Update(ConnectHandlers(handlers));

    if (!status.ok()) {
      LOG(ERROR) << "Failed to setup graph: " << status;
      return false;
    }
    if (!stream_iter->language.empty())
      demuxer->SetLanguageOverride(stream_selector, stream_iter->language);
  }

  // Initialize processing graph.
  for (const std::unique_ptr<RemuxJob>& job : *remux_jobs) {
    Status status = job->demuxer()->Initialize();
    if (!status.ok()) {
      LOG(ERROR) << "Failed to initialize processing graph " << status;
      return false;
    }
  }
  return true;
}

Status RunRemuxJobs(const std::vector<std::unique_ptr<RemuxJob>>& remux_jobs) {
  // Start the job threads.
  for (const std::unique_ptr<RemuxJob>& job : remux_jobs)
    job->Start();

  // Wait for all jobs to complete or an error occurs.
  Status status;
  bool all_joined;
  do {
    all_joined = true;
    for (const std::unique_ptr<RemuxJob>& job : remux_jobs) {
      if (job->HasBeenJoined()) {
        status = job->status();
        if (!status.ok())
          break;
      } else {
        all_joined = false;
        job->Join();
      }
    }
  } while (!all_joined && status.ok());

  return status;
}

bool RunPackager(const StreamDescriptorList& stream_descriptors) {
  if (FLAGS_output_media_info && !FLAGS_mpd_output.empty()) {
    NOTIMPLEMENTED() << "ERROR: --output_media_info and --mpd_output do not "
                        "work together.";
    return false;
  }

  // Since there isn't a muxer listener that can output both MPD and HLS,
  // disallow specifying both MPD and HLS flags.
  if (!FLAGS_mpd_output.empty() && !FLAGS_hls_master_playlist_output.empty()) {
    LOG(ERROR) << "Cannot output both MPD and HLS.";
    return false;
  }

  ChunkingOptions chunking_options = GetChunkingOptions();
  EncryptionOptions encryption_options = GetEncryptionOptions();

  MuxerOptions muxer_options = GetMuxerOptions();

  DCHECK(!stream_descriptors.empty());
  // On demand profile generates single file segment while live profile
  // generates multiple segments specified using segment template.
  const bool on_demand_dash_profile =
      stream_descriptors.begin()->segment_template.empty();
  for (const auto& stream_descriptor : stream_descriptors) {
    if (on_demand_dash_profile != stream_descriptor.segment_template.empty()) {
      LOG(ERROR) << "Inconsistent stream descriptor specification: "
                    "segment_template should be specified for none or all "
                    "stream descriptors.";
      return false;
    }
  }
  if (FLAGS_output_media_info && !on_demand_dash_profile) {
    // TODO(rkuroiwa, kqyang): Support partial media info dump for live.
    NOTIMPLEMENTED() << "ERROR: --output_media_info is only supported for "
                        "on-demand profile (not using segment_template).";
    return false;
  }

  MpdOptions mpd_options = GetMpdOptions(on_demand_dash_profile);

  // Create encryption key source if needed.
  std::unique_ptr<KeySource> encryption_key_source;
  if (FLAGS_enable_widevine_encryption || FLAGS_enable_fixed_key_encryption ||
      FLAGS_enable_playready_encryption) {
    if (encryption_options.protection_scheme == FOURCC_NULL)
      return false;
    encryption_key_source = CreateEncryptionKeySource();
    if (!encryption_key_source)
      return false;
  }

  std::unique_ptr<MpdNotifier> mpd_notifier;
  if (!FLAGS_mpd_output.empty()) {
    std::vector<std::string> base_urls = base::SplitString(
        FLAGS_base_urls, ",", base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    if (FLAGS_generate_dash_if_iop_compliant_mpd) {
      mpd_notifier.reset(
          new DashIopMpdNotifier(mpd_options, base_urls, FLAGS_mpd_output));
    } else {
      mpd_notifier.reset(
          new SimpleMpdNotifier(mpd_options, base_urls, FLAGS_mpd_output));
    }
    if (!mpd_notifier->Init()) {
      LOG(ERROR) << "MpdNotifier failed to initialize.";
      return false;
    }
  }

  std::unique_ptr<hls::HlsNotifier> hls_notifier;
  if (!FLAGS_hls_master_playlist_output.empty()) {
    base::FilePath master_playlist_path(
        base::FilePath::FromUTF8Unsafe(FLAGS_hls_master_playlist_output));
    base::FilePath master_playlist_name = master_playlist_path.BaseName();

    hls_notifier.reset(new hls::SimpleHlsNotifier(
        hls::HlsNotifier::HlsProfile::kOnDemandProfile, FLAGS_hls_base_url,
        master_playlist_path.DirName().AsEndingWithSeparator().AsUTF8Unsafe(),
        master_playlist_name.AsUTF8Unsafe()));
  }

  std::vector<std::unique_ptr<RemuxJob>> remux_jobs;
  FakeClock fake_clock;
  if (!CreateRemuxJobs(stream_descriptors, chunking_options, encryption_options,
                       muxer_options, &fake_clock, encryption_key_source.get(),
                       mpd_notifier.get(), hls_notifier.get(), &remux_jobs)) {
    return false;
  }

  Status status = RunRemuxJobs(remux_jobs);
  if (!status.ok()) {
    LOG(ERROR) << "Packaging Error: " << status.ToString();
    return false;
  }

  if (hls_notifier) {
    if (!hls_notifier->Flush())
      return false;
  }
  if (mpd_notifier) {
    if (!mpd_notifier->Flush())
      return false;
  }

  printf("Packaging completed successfully.\n");
  return true;
}

int PackagerMain(int argc, char** argv) {
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
  if (argc < 2) {
    google::ShowUsageWithFlags("Usage");
    return kSuccess;
  }

  if (!ValidateWidevineCryptoFlags() || !ValidateFixedCryptoFlags() ||
      !ValidatePRCryptoFlags()) {
    return kArgumentValidationFailed;
  }

  if (FLAGS_override_version)
    SetPackagerVersionForTesting(FLAGS_test_version);

  LibcryptoThreading libcrypto_threading;
  // TODO(tinskip): Make InsertStreamDescriptor a member of
  // StreamDescriptorList.
  StreamDescriptorList stream_descriptors;
  for (int i = 1; i < argc; ++i) {
    if (!InsertStreamDescriptor(argv[i], &stream_descriptors))
      return kArgumentValidationFailed;
  }
  return RunPackager(stream_descriptors) ? kSuccess : kPackagingFailed;
}

}  // namespace media
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
  return shaka::media::PackagerMain(argc, utf8_argv.get());
}
#else
int main(int argc, char** argv) {
  return shaka::media::PackagerMain(argc, argv);
}
#endif  // defined(OS_WIN)
