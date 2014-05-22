// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <iostream>

#include "app/fixed_key_encryption_flags.h"
#include "app/packager_common.h"
#include "app/mpd_flags.h"
#include "app/muxer_flags.h"
#include "app/single_muxer_flags.h"
#include "app/widevine_encryption_flags.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "media/base/demuxer.h"
#include "media/base/encryption_key_source.h"
#include "media/base/muxer_options.h"
#include "media/base/muxer_util.h"
#include "media/event/mpd_notify_muxer_listener.h"
#include "media/event/vod_media_info_dump_muxer_listener.h"
#include "media/file/file.h"
#include "media/file/file_closer.h"
#include "media/formats/mp4/mp4_muxer.h"
#include "mpd/base/mpd_builder.h"
#include "mpd/base/simple_mpd_notifier.h"

namespace {
const char kUsage[] =
    "Single-stream packager driver program. Sample Usage:\n%s <input> [flags]";
}  // namespace

namespace media {

bool GetSingleMuxerOptions(MuxerOptions* muxer_options) {
  DCHECK(muxer_options);

  if (!GetMuxerOptions(muxer_options))
    return false;

  muxer_options->output_file_name = FLAGS_output;
  muxer_options->segment_template = FLAGS_segment_template;
  if (!muxer_options->segment_template.empty() &&
      !ValidateSegmentTemplate(muxer_options->segment_template)) {
    LOG(ERROR) << "ERROR: segment template with '"
               << muxer_options->segment_template << "' is invalid.";
    return false;
  }

  return true;
}

bool RunPackager(const std::string& input) {
  Status status;

  if (FLAGS_output_media_info && !FLAGS_mpd_output.empty()) {
    NOTIMPLEMENTED() << "ERROR: --output_media_info and --mpd_output cannot be "
                        "enabled together.";
    return false;
  }

  if (!FLAGS_single_segment) {
    if (FLAGS_output_media_info) {
      LOG(ERROR) << "ERROR: --output_media_info can be enabled only if "
                    "--single_segment is true.";
      return false;
    }
    if (!FLAGS_mpd_output.empty() && FLAGS_segment_template.empty()) {
      LOG(ERROR) << "ERROR: --segment_template is required for live mpd "
                    "profile generation.";
      return false;
    }
  }

  // Get muxer options from commandline flags.
  MuxerOptions muxer_options;
  if (!GetSingleMuxerOptions(&muxer_options))
    return false;

  // Setup and initialize Demuxer.
  Demuxer demuxer(input, NULL);
  status = demuxer.Initialize();
  if (!status.ok()) {
    LOG(ERROR) << "Demuxer failed to initialize: " << status.ToString();
    return false;
  }

  if (FLAGS_dump_stream_info)
    DumpStreamInfo(demuxer.streams());

  if (FLAGS_output.empty()) {
    if (!FLAGS_dump_stream_info)
      LOG(WARNING) << "No output specified. Exiting.";
    return true;
  }

  // Setup muxer.
  scoped_ptr<Muxer> muxer(new mp4::MP4Muxer(muxer_options));
  scoped_ptr<event::MuxerListener> muxer_listener;
  scoped_ptr<File, FileCloser> mpd_file;
  if (FLAGS_output_media_info) {
    std::string output_mpd_file_name = FLAGS_output + ".media_info";
    mpd_file.reset(File::Open(output_mpd_file_name.c_str(), "w"));
    if (!mpd_file) {
      LOG(ERROR) << "Failed to open " << output_mpd_file_name;
      return false;
    }

    scoped_ptr<event::VodMediaInfoDumpMuxerListener> media_info_muxer_listener(
        new event::VodMediaInfoDumpMuxerListener(mpd_file.get()));
    media_info_muxer_listener->SetContentProtectionSchemeIdUri(
        FLAGS_scheme_id_uri);
    muxer_listener = media_info_muxer_listener.Pass();
    muxer->SetMuxerListener(muxer_listener.get());
  }

  scoped_ptr<dash_packager::MpdNotifier> mpd_notifier;
  if (!FLAGS_mpd_output.empty()) {
    dash_packager::DashProfile profile = FLAGS_single_segment
                                             ? dash_packager::kOnDemandProfile
                                             : dash_packager::kLiveProfile;
    std::vector<std::string> base_urls;
    base::SplitString(FLAGS_base_urls, ',', &base_urls);
    // TODO(rkuroiwa,kqyang): Get mpd options from command line.
    mpd_notifier.reset(new dash_packager::SimpleMpdNotifier(
        profile, dash_packager::MpdOptions(), base_urls, FLAGS_mpd_output));
    if (!mpd_notifier->Init()) {
      LOG(ERROR) << "MpdNotifier failed to initialize.";
      return false;
    }

    scoped_ptr<event::MpdNotifyMuxerListener> mpd_notify_muxer_listener(
        new event::MpdNotifyMuxerListener(mpd_notifier.get()));
    mpd_notify_muxer_listener->SetContentProtectionSchemeIdUri(
        FLAGS_scheme_id_uri);
    muxer_listener = mpd_notify_muxer_listener.Pass();
    muxer->SetMuxerListener(muxer_listener.get());
  }

  if (!AddStreamToMuxer(demuxer.streams(), FLAGS_stream, muxer.get()))
    return false;

  scoped_ptr<EncryptionKeySource> encryption_key_source;
  if (FLAGS_enable_widevine_encryption || FLAGS_enable_fixed_key_encryption) {
    encryption_key_source = CreateEncryptionKeySource();
    if (!encryption_key_source)
      return false;
    muxer->SetEncryptionKeySource(encryption_key_source.get(),
                                  FLAGS_max_sd_pixels,
                                  FLAGS_clear_lead,
                                  FLAGS_crypto_period_duration);
  }

  // Start remuxing process.
  status = demuxer.Run();
  if (!status.ok()) {
    LOG(ERROR) << "Remuxing failed: " << status.ToString();
    return false;
  }

  printf("Packaging completed successfully.\n");
  return true;
}

}  // namespace media

int main(int argc, char** argv) {
  google::SetUsageMessage(base::StringPrintf(kUsage, argv[0]));
  google::ParseCommandLineFlags(&argc, &argv, true);
  if (argc != 2) {
    google::ShowUsageWithFlags(argv[0]);
    return 1;
  }
  return media::RunPackager(argv[1]) ? 0 : 1;
}
