// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <iostream>

#include "app/fixed_key_encryption_flags.h"
#include "app/libcrypto_threading.h"
#include "app/mpd_flags.h"
#include "app/muxer_flags.h"
#include "app/packager_util.h"
#include "app/stream_descriptor.h"
#include "app/widevine_encryption_flags.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/threading/simple_thread.h"
#include "media/base/demuxer.h"
#include "media/base/key_source.h"
#include "media/base/muxer_options.h"
#include "media/base/muxer_util.h"
#include "media/event/mpd_notify_muxer_listener.h"
#include "media/event/vod_media_info_dump_muxer_listener.h"
#include "media/formats/mp4/mp4_muxer.h"
#include "mpd/base/mpd_builder.h"
#include "mpd/base/simple_mpd_notifier.h"

namespace {
const char kUsage[] =
    "Packager driver program. Sample Usage:\n"
    "%s [flags] <stream_descriptor> ...\n"
    "stream_descriptor consists of comma separated field_name/value pairs:\n"
    "field_name=value,[field_name=value,]...\n"
    "Supported field names are as follows:\n"
    "  - input (in): Required input/source media file path or network stream "
    "URL.\n"
    "  - stream_selector (stream): Required field with value 'audio', 'video', "
    "or stream number (zero based).\n"
    "  - output (out): Required output file (single file) or initialization "
    "file path (multiple file).\n"
    "  - segment_template (segment): Optional value which specifies the "
    "naming  pattern for the segment files, and that the stream should be "
    "split into multiple files. Its presence should be consistent across "
    "streams.\n"
    "  - bandwidth (bw): Optional value which contains a user-specified "
    "content bit rate for the stream, in bits/sec. If specified, this value is "
    "propagated to the $Bandwidth$ template parameter for segment names. "
    "If not specified, its value may be estimated.\n";
}  // namespace

namespace media {

using dash_packager::DashProfile;
using dash_packager::kOnDemandProfile;
using dash_packager::kLiveProfile;
using dash_packager::MpdNotifier;
using dash_packager::MpdOptions;
using dash_packager::SimpleMpdNotifier;
using event::MpdNotifyMuxerListener;
using event::MuxerListener;
using event::VodMediaInfoDumpMuxerListener;

// Demux, Mux(es) and worker thread used to remux a source file/stream.
class RemuxJob : public base::SimpleThread {
 public:
  RemuxJob(scoped_ptr<Demuxer> demuxer)
      : SimpleThread("RemuxJob"),
        demuxer_(demuxer.Pass()) {}

  virtual ~RemuxJob() {
    STLDeleteElements(&muxers_);
  }

  void AddMuxer(scoped_ptr<Muxer> mux) {
    muxers_.push_back(mux.release());
  }

  Demuxer* demuxer() { return demuxer_.get(); }
  Status status() { return status_; }

 private:
  virtual void Run() OVERRIDE {
    DCHECK(demuxer_);
    status_ = demuxer_->Run();
  }

  scoped_ptr<Demuxer> demuxer_;
  std::vector<Muxer*> muxers_;
  Status status_;

  DISALLOW_COPY_AND_ASSIGN(RemuxJob);
};

bool CreateRemuxJobs(const StreamDescriptorList& stream_descriptors,
                     const MuxerOptions& muxer_options,
                     KeySource* key_source,
                     MpdNotifier* mpd_notifier,
                     std::vector<MuxerListener*>* muxer_listeners,
                     std::vector<RemuxJob*>* remux_jobs) {
  DCHECK(muxer_listeners);
  DCHECK(remux_jobs);

  std::string previous_input;
  for (StreamDescriptorList::const_iterator stream_iter =
           stream_descriptors.begin();
       stream_iter != stream_descriptors.end();
       ++stream_iter) {
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

    if (stream_iter->input != previous_input) {
      // New remux job needed. Create demux and job thread.
      scoped_ptr<Demuxer> demuxer(new Demuxer(stream_iter->input));
      demuxer->SetKeySource(CreateDecryptionKeySource());
      Status status = demuxer->Initialize();
      if (!status.ok()) {
        LOG(ERROR) << "Demuxer failed to initialize: " << status.ToString();
        return false;
      }
      if (FLAGS_dump_stream_info) {
        printf("\nFile \"%s\":\n", stream_iter->input.c_str());
        DumpStreamInfo(demuxer->streams());
        if (stream_iter->output.empty())
          continue;  // just need stream info.
      }
      remux_jobs->push_back(new RemuxJob(demuxer.Pass()));
      previous_input = stream_iter->input;
    }
    DCHECK(!remux_jobs->empty());

    scoped_ptr<Muxer> muxer(new mp4::MP4Muxer(stream_muxer_options));
    if (key_source) {
      muxer->SetKeySource(key_source,
                          FLAGS_max_sd_pixels,
                          FLAGS_clear_lead,
                          FLAGS_crypto_period_duration);
    }

    scoped_ptr<MuxerListener> muxer_listener;
    DCHECK(!(FLAGS_output_media_info && mpd_notifier));
    if (FLAGS_output_media_info) {
      const std::string output_mpd_file_name =
          stream_muxer_options.output_file_name + ".media_info";
      scoped_ptr<VodMediaInfoDumpMuxerListener>
          vod_media_info_dump_muxer_listener(
              new VodMediaInfoDumpMuxerListener(output_mpd_file_name));
      vod_media_info_dump_muxer_listener->SetContentProtectionSchemeIdUri(
          FLAGS_scheme_id_uri);
      muxer_listener = vod_media_info_dump_muxer_listener.Pass();
    }
    if (mpd_notifier) {
      scoped_ptr<MpdNotifyMuxerListener> mpd_notify_muxer_listener(
          new MpdNotifyMuxerListener(mpd_notifier));
      mpd_notify_muxer_listener->SetContentProtectionSchemeIdUri(
          FLAGS_scheme_id_uri);
      muxer_listener = mpd_notify_muxer_listener.Pass();
    }

    if (muxer_listener) {
      muxer_listeners->push_back(muxer_listener.release());
      muxer->SetMuxerListener(muxer_listeners->back());
    }

    if (!AddStreamToMuxer(remux_jobs->back()->demuxer()->streams(),
                          stream_iter->stream_selector,
                          muxer.get()))
      return false;
    remux_jobs->back()->AddMuxer(muxer.Pass());
  }

  return true;
}

Status RunRemuxJobs(const std::vector<RemuxJob*>& remux_jobs) {
  // Start the job threads.
  for (std::vector<RemuxJob*>::const_iterator job_iter = remux_jobs.begin();
       job_iter != remux_jobs.end();
       ++job_iter) {
    (*job_iter)->Start();
  }

  // Wait for all jobs to complete or an error occurs.
  Status status;
  bool all_joined;
  do {
    all_joined = true;
    for (std::vector<RemuxJob*>::const_iterator job_iter = remux_jobs.begin();
         job_iter != remux_jobs.end();
         ++job_iter) {
      if ((*job_iter)->HasBeenJoined()) {
        status = (*job_iter)->status();
        if (!status.ok())
          break;
      } else {
        all_joined = false;
        (*job_iter)->Join();
      }
    }
  } while (!all_joined && status.ok());

  return status;
}

bool RunPackager(const StreamDescriptorList& stream_descriptors) {
  if (!AssignFlagsFromProfile())
    return false;

  if (FLAGS_output_media_info && !FLAGS_mpd_output.empty()) {
    NOTIMPLEMENTED() << "ERROR: --output_media_info and --mpd_output do not "
                        "work together.";
    return false;
  }
  if (FLAGS_output_media_info && !FLAGS_single_segment) {
    // TODO(rkuroiwa, kqyang): Support partial media info dump for live.
    NOTIMPLEMENTED() << "ERROR: --output_media_info is only supported if "
                        "--single_segment is true.";
    return false;
  }

  // Get basic muxer options.
  MuxerOptions muxer_options;
  if (!GetMuxerOptions(&muxer_options))
    return false;

  MpdOptions mpd_options;
  if (!GetMpdOptions(&mpd_options))
    return false;

  // Create encryption key source if needed.
  scoped_ptr<KeySource> encryption_key_source;
  if (FLAGS_enable_widevine_encryption || FLAGS_enable_fixed_key_encryption) {
    encryption_key_source = CreateEncryptionKeySource();
    if (!encryption_key_source)
      return false;
  }

  scoped_ptr<MpdNotifier> mpd_notifier;
  if (!FLAGS_mpd_output.empty()) {
    DashProfile profile =
        FLAGS_single_segment ? kOnDemandProfile : kLiveProfile;
    std::vector<std::string> base_urls;
    base::SplitString(FLAGS_base_urls, ',', &base_urls);
    mpd_notifier.reset(new SimpleMpdNotifier(profile, mpd_options, base_urls,
                                             FLAGS_mpd_output));
    if (!mpd_notifier->Init()) {
      LOG(ERROR) << "MpdNotifier failed to initialize.";
      return false;
    }
  }

  // TODO(kqyang): Should Muxer::SetMuxerListener take owership of the
  // muxer_listeners object? Then we can get rid of |muxer_listeners|.
  std::vector<MuxerListener*> muxer_listeners;
  STLElementDeleter<std::vector<MuxerListener*> > deleter(&muxer_listeners);
  std::vector<RemuxJob*> remux_jobs;
  STLElementDeleter<std::vector<RemuxJob*> > scoped_jobs_deleter(&remux_jobs);
  if (!CreateRemuxJobs(stream_descriptors,
                       muxer_options,
                       encryption_key_source.get(),
                       mpd_notifier.get(),
                       &muxer_listeners,
                       &remux_jobs)) {
    return false;
  }

  Status status = RunRemuxJobs(remux_jobs);
  if (!status.ok()) {
    LOG(ERROR) << "Packaging Error: " << status.ToString();
    return false;
  }

  printf("Packaging completed successfully.\n");
  return true;
}

}  // namespace media

int main(int argc, char** argv) {
  google::SetUsageMessage(base::StringPrintf(kUsage, argv[0]));
  google::ParseCommandLineFlags(&argc, &argv, true);
  if (argc < 2) {
    google::ShowUsageWithFlags(argv[0]);
    return 1;
  }
  media::LibcryptoThreading libcrypto_threading;
  if (!libcrypto_threading.Initialize()) {
    LOG(ERROR) << "Could not initialize libcrypto threading.";
    return 1;
  }
  // TODO(tinskip): Make InsertStreamDescriptor a member of
  // StreamDescriptorList.
  media::StreamDescriptorList stream_descriptors;
  for (int i = 1; i < argc; ++i) {
    if (!InsertStreamDescriptor(argv[i], &stream_descriptors))
      return 1;
  }
  return media::RunPackager(stream_descriptors) ? 0 : 1;
}
