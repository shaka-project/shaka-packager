// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <iostream>

#include "app/fixed_key_encryption_flags.h"
#include "app/libcrypto_threading.h"
#include "app/packager_common.h"
#include "app/mpd_flags.h"
#include "app/muxer_flags.h"
#include "app/widevine_encryption_flags.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/threading/simple_thread.h"
#include "media/base/demuxer.h"
#include "media/base/encryption_key_source.h"
#include "media/base/muxer_options.h"
#include "media/base/muxer_util.h"
#include "media/event/mpd_notify_muxer_listener.h"
#include "media/formats/mp4/mp4_muxer.h"
#include "mpd/base/mpd_builder.h"
#include "mpd/base/simple_mpd_notifier.h"

namespace {
const char kUsage[] =
    "Packager driver program. Sample Usage:\n"
    "%s [flags] <stream_descriptor> ...\n"
    "stream_descriptor may be repeated and consists of a tuplet as follows:\n"
    "<input_file>#<stream_selector>,<output_file>[,<segment_template>]\n"
    "  - input_file is a file path or network stream URL.\n"
    "  - stream_selector is one of 'audio', 'video', or stream number.\n"
    "  - output_file is the output file (single file) or initialization file"
    " path (multiple file)."
    "  - segment_template is an optional value which specifies the naming"
    " pattern for the segment files, and that the stream should be split into"
    " multiple files. Its presence should be consistent across streams.\n";

typedef std::vector<std::string> StringVector;

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

bool CreateRemuxJobs(const StringVector& stream_descriptors,
                     const MuxerOptions& muxer_options,
                     EncryptionKeySource* key_source,
                     MpdNotifier* mpd_notifier,
                     std::vector<MuxerListener*>* muxer_listeners,
                     std::vector<RemuxJob*>* remux_jobs) {
  DCHECK(muxer_listeners);
  DCHECK(remux_jobs);

  // Sort the stream descriptors so that we can group muxers by demux.
  StringVector sorted_descriptors(stream_descriptors);
  std::sort(sorted_descriptors.begin(), sorted_descriptors.end());

  std::string previous_file_path;
  for (StringVector::const_iterator stream_iter = sorted_descriptors.begin();
       stream_iter != sorted_descriptors.end();
       ++stream_iter) {
    // Process stream descriptor.
    StringVector descriptor;
    base::SplitString(*stream_iter, ',', &descriptor);
    if ((descriptor.size() < 2) || (descriptor.size() > 3)) {
      LOG(ERROR)
          << "Malformed stream descriptor (invalid number of components).";
      return false;
    }
    size_t hash_pos = descriptor[0].find('#');
    if (hash_pos == std::string::npos) {
      LOG(ERROR)
          << "Malformed stream descriptor (stream selector unspecified).";
      return false;
    }
    MuxerOptions stream_muxer_options(muxer_options);
    std::string file_path(descriptor[0].substr(0, hash_pos));
    std::string stream_selector(descriptor[0].substr(hash_pos + 1));
    stream_muxer_options.output_file_name = descriptor[1];
    if (descriptor.size() == 3) {
      stream_muxer_options.segment_template = descriptor[2];
      if (!ValidateSegmentTemplate(stream_muxer_options.segment_template)) {
        LOG(ERROR) << "ERROR: segment template with '"
                   << stream_muxer_options.segment_template << "' is invalid.";
        return false;
      }
    }

    if (file_path != previous_file_path) {
      // New remux job needed. Create demux and job thread.
      scoped_ptr<Demuxer> demux(new Demuxer(file_path, NULL));
      Status status = demux->Initialize();
      if (!status.ok()) {
        LOG(ERROR) << "Demuxer failed to initialize: " << status.ToString();
        return false;
      }
      if (FLAGS_dump_stream_info) {
        printf("\nFile \"%s\":\n", file_path.c_str());
        DumpStreamInfo(demux->streams());
      }
      remux_jobs->push_back(new RemuxJob(demux.Pass()));
      previous_file_path = file_path;
    }
    DCHECK(!remux_jobs->empty());

    scoped_ptr<Muxer> muxer(new mp4::MP4Muxer(stream_muxer_options));
    if (key_source) {
      muxer->SetEncryptionKeySource(key_source,
                                    FLAGS_max_sd_pixels,
                                    FLAGS_clear_lead,
                                    FLAGS_crypto_period_duration);
    }

    if (mpd_notifier) {
      scoped_ptr<MpdNotifyMuxerListener> mpd_notify_muxer_listener(
          new MpdNotifyMuxerListener(mpd_notifier));
      mpd_notify_muxer_listener->SetContentProtectionSchemeIdUri(
          FLAGS_scheme_id_uri);
      muxer_listeners->push_back(mpd_notify_muxer_listener.release());
      muxer->SetMuxerListener(muxer_listeners->back());
    }

    if (!AddStreamToMuxer(remux_jobs->back()->demuxer()->streams(),
                          stream_selector,
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

bool RunPackager(const StringVector& stream_descriptors) {
  if (FLAGS_output_media_info) {
    NOTIMPLEMENTED() << "ERROR: --output_media_info is not supported yet.";
    return false;
  }

  // Get basic muxer options.
  MuxerOptions muxer_options;
  if (!GetMuxerOptions(&muxer_options))
    return false;

  // Create encryption key source if needed.
  scoped_ptr<EncryptionKeySource> encryption_key_source;
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
    // TODO(rkuroiwa,kqyang): Get mpd options from command line.
    mpd_notifier.reset(new SimpleMpdNotifier(profile, MpdOptions(), base_urls,
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
  StringVector stream_descriptors;
  for (int i = 1; i < argc; ++i)
    stream_descriptors.push_back(argv[i]);
  return media::RunPackager(stream_descriptors) ? 0 : 1;
}
