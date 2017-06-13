// Copyright 2017 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/packager.h"

#include "packager/app/libcrypto_threading.h"
#include "packager/app/packager_util.h"
#include "packager/app/stream_descriptor.h"
#include "packager/base/at_exit.h"
#include "packager/base/files/file_path.h"
#include "packager/base/logging.h"
#include "packager/base/path_service.h"
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

namespace shaka {

// TODO(kqyang): Clean up namespaces.
using media::ChunkingOptions;
using media::Demuxer;
using media::EncryptionOptions;
using media::KeySource;
using media::MuxerOptions;
using media::Status;
namespace error = media::error;

namespace media {
namespace {

const char kMediaInfoSuffix[] = ".media_info";

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

MediaContainerName GetOutputFormat(const StreamDescriptor& descriptor) {
  MediaContainerName output_format = CONTAINER_UNKNOWN;
  if (!descriptor.output_format.empty()) {
    output_format = DetermineContainerFromFormatName(descriptor.output_format);
    if (output_format == CONTAINER_UNKNOWN) {
      LOG(ERROR) << "Unable to determine output format from '"
                 << descriptor.output_format << "'.";
    }
  } else {
    const std::string& output_name = descriptor.output.empty()
                                         ? descriptor.segment_template
                                         : descriptor.output;
    if (output_name.empty())
      return CONTAINER_UNKNOWN;
    output_format = DetermineContainerFromFileName(output_name);
    if (output_format == CONTAINER_UNKNOWN) {
      LOG(ERROR) << "Unable to determine output format from '" << output_name
                 << "'.";
    }
  }
  return output_format;
}

bool ValidateStreamDescriptor(bool dump_stream_info,
                              const StreamDescriptor& descriptor) {
  // Validate and insert the descriptor
  if (descriptor.input.empty()) {
    LOG(ERROR) << "Stream input not specified.";
    return false;
  }
  if (!dump_stream_info && descriptor.stream_selector.empty()) {
    LOG(ERROR) << "Stream stream_selector not specified.";
    return false;
  }

  // We should have either output or segment_template specified.
  const bool output_specified =
      !descriptor.output.empty() || !descriptor.segment_template.empty();
  if (!output_specified) {
    if (!dump_stream_info) {
      LOG(ERROR) << "Stream output not specified.";
      return false;
    }
  } else {
    const MediaContainerName output_format = GetOutputFormat(descriptor);
    if (output_format == CONTAINER_UNKNOWN)
      return false;

    if (output_format == MediaContainerName::CONTAINER_MPEG2TS) {
      if (descriptor.segment_template.empty()) {
        LOG(ERROR) << "Please specify segment_template. Single file TS output "
                      "is not supported.";
        return false;
      }
      // Note that MPEG2 TS doesn't need a separate initialization segment, so
      // output field is not needed.
      if (!descriptor.output.empty()) {
        LOG(WARNING) << "TS init_segment '" << descriptor.output
                     << "' ignored. TS muxer does not support initialization "
                        "segment generation.";
      }
    } else {
      if (descriptor.output.empty()) {
        LOG(ERROR) << "init_segment is required for format " << output_format;
        return false;
      }
    }
  }
  return true;
}

bool ValidateParams(const PackagingParams& packaging_params,
                    const std::vector<StreamDescriptor>& stream_descriptors) {
  if (!packaging_params.chunking_params.segment_sap_aligned &&
      packaging_params.chunking_params.subsegment_sap_aligned) {
    LOG(ERROR) << "Setting segment_sap_aligned to false but "
                  "subsegment_sap_aligned to true is not allowed.";
    return false;
  }

  if (packaging_params.output_media_info &&
      !packaging_params.mpd_params.mpd_output.empty()) {
    LOG(ERROR) << "output_media_info and MPD output do not work together.";
    return false;
  }

  if (packaging_params.output_media_info &&
      !packaging_params.hls_params.master_playlist_output.empty()) {
    LOG(ERROR) << "output_media_info and HLS output do not work together.";
    return false;
  }

  // Since there isn't a muxer listener that can output both MPD and HLS,
  // disallow specifying both MPD and HLS flags.
  if (!packaging_params.mpd_params.mpd_output.empty() &&
      !packaging_params.hls_params.master_playlist_output.empty()) {
    LOG(ERROR) << "output both MPD and HLS are not supported.";
    return false;
  }

  if (stream_descriptors.empty()) {
    LOG(ERROR) << "Stream descriptors cannot be empty.";
    return false;
  }

  // On demand profile generates single file segment while live profile
  // generates multiple segments specified using segment template.
  const bool on_demand_dash_profile =
      stream_descriptors.begin()->segment_template.empty();
  for (const auto& descriptor : stream_descriptors) {
    if (on_demand_dash_profile != descriptor.segment_template.empty()) {
      LOG(ERROR) << "Inconsistent stream descriptor specification: "
                    "segment_template should be specified for none or all "
                    "stream descriptors.";
      return false;
    }
    if (!ValidateStreamDescriptor(packaging_params.test_params.dump_stream_info,
                                  descriptor))
      return false;
  }
  if (packaging_params.output_media_info && !on_demand_dash_profile) {
    // TODO(rkuroiwa, kqyang): Support partial media info dump for live.
    NOTIMPLEMENTED() << "ERROR: --output_media_info is only supported for "
                        "on-demand profile (not using segment_template).";
    return false;
  }

  return true;
}

class StreamDescriptorCompareFn {
 public:
  bool operator()(const StreamDescriptor& a, const StreamDescriptor& b) {
    if (a.input == b.input) {
      if (a.stream_selector == b.stream_selector)
        // Stream with high trick_play_factor is at the beginning.
        return a.trick_play_factor > b.trick_play_factor;
      else
        return a.stream_selector < b.stream_selector;
    }

    return a.input < b.input;
  }
};

/// Sorted list of StreamDescriptor.
typedef std::multiset<StreamDescriptor, StreamDescriptorCompareFn>
    StreamDescriptorList;

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
  RemuxJob(const RemuxJob&) = delete;
  RemuxJob& operator=(const RemuxJob&) = delete;

  void Run() override {
    DCHECK(demuxer_);
    status_ = demuxer_->Run();
  }

  std::unique_ptr<Demuxer> demuxer_;
  Status status_;
};

bool StreamInfoToTextMediaInfo(const StreamDescriptor& stream_descriptor,
                               const MuxerOptions& stream_muxer_options,
                               MediaInfo* text_media_info) {
  const std::string& language = stream_descriptor.language;
  const std::string format = DetermineTextFileFormat(stream_descriptor.input);
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
    const int kDefaultTextBandwidth = 256;
    text_media_info->set_bandwidth(kDefaultTextBandwidth);
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
                     const PackagingParams& packaging_params,
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
    MediaContainerName output_format = GetOutputFormat(*stream_iter);

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
        output_format != CONTAINER_MOV) {
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
      } else if (packaging_params.output_media_info) {
        VodMediaInfoDumpMuxerListener::WriteMediaInfoToFile(
            text_media_info,
            stream_muxer_options.output_file_name + kMediaInfoSuffix);
      }
      continue;
    }

    if (stream_iter->input != previous_input) {
      // New remux job needed. Create demux and job thread.
      std::unique_ptr<Demuxer> demuxer(new Demuxer(stream_iter->input));
      demuxer->set_dump_stream_info(
          packaging_params.test_params.dump_stream_info);
      if (packaging_params.decryption_params.key_provider !=
          KeyProvider::kNone) {
        std::unique_ptr<KeySource> decryption_key_source(
            CreateDecryptionKeySource(packaging_params.decryption_params));
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
        CreateOutputMuxer(stream_muxer_options, output_format));
    if (packaging_params.test_params.inject_fake_clock)
      muxer->set_clock(fake_clock);

    std::unique_ptr<MuxerListener> muxer_listener;
    DCHECK(!(packaging_params.output_media_info && mpd_notifier));
    if (packaging_params.output_media_info) {
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
    if (stream_iter->trick_play_factor > 0) {
      if (!trick_play_handler) {
        trick_play_handler.reset(new TrickPlayHandler());
      }
      trick_play_handler->SetHandlerForTrickPlay(stream_iter->trick_play_factor,
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
    if (encryption_key_source && !stream_iter->skip_encryption) {
      auto new_encryption_options = encryption_options;
      // Use Sample AES in MPEG2TS.
      // TODO(kqyang): Consider adding a new flag to enable Sample AES as we
      // will support CENC in TS in the future.
      if (output_format == CONTAINER_MPEG2TS) {
        VLOG(1) << "Use Apple Sample AES encryption for MPEG2TS.";
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

}  // namespace
}  // namespace media

std::string EncryptionParams::DefaultStreamLabelFunction(
    int max_sd_pixels,
    int max_hd_pixels,
    int max_uhd1_pixels,
    const EncryptedStreamAttributes& stream_attributes) {
  if (stream_attributes.stream_type == EncryptedStreamAttributes::kAudio)
    return "AUDIO";
  if (stream_attributes.stream_type == EncryptedStreamAttributes::kVideo) {
    const int pixels = stream_attributes.oneof.video.width *
                       stream_attributes.oneof.video.height;
    if (pixels <= max_sd_pixels) return "SD";
    if (pixels <= max_hd_pixels) return "HD";
    if (pixels <= max_uhd1_pixels) return "UHD1";
    return "UHD2";
  }
  return "";
}

struct Packager::PackagerInternal {
  media::FakeClock fake_clock;
  std::unique_ptr<KeySource> encryption_key_source;
  std::unique_ptr<MpdNotifier> mpd_notifier;
  std::unique_ptr<hls::HlsNotifier> hls_notifier;
  std::vector<std::unique_ptr<media::RemuxJob>> remux_jobs;
};

Packager::Packager() {}

Packager::~Packager() {}

Status Packager::Initialize(
    const PackagingParams& packaging_params,
    const std::vector<StreamDescriptor>& stream_descriptors) {
  // Needed by base::WorkedPool used in ThreadedIoFile.
  static base::AtExitManager exit;
  static media::LibcryptoThreading libcrypto_threading;

  if (internal_)
    return Status(error::INVALID_ARGUMENT, "Already initialized.");

  if (!media::ValidateParams(packaging_params, stream_descriptors))
    return Status(error::INVALID_ARGUMENT, "Invalid packaging params.");

  if (!packaging_params.test_params.injected_library_version.empty()) {
    SetPackagerVersionForTesting(
        packaging_params.test_params.injected_library_version);
  }

  std::unique_ptr<PackagerInternal> internal(new PackagerInternal);

  ChunkingOptions chunking_options =
      media::GetChunkingOptions(packaging_params.chunking_params);
  EncryptionOptions encryption_options =
      media::GetEncryptionOptions(packaging_params.encryption_params);
  MuxerOptions muxer_options = media::GetMuxerOptions(
      packaging_params.temp_dir, packaging_params.mp4_output_params);

  const bool on_demand_dash_profile =
      stream_descriptors.begin()->segment_template.empty();
  MpdOptions mpd_options =
      media::GetMpdOptions(on_demand_dash_profile, packaging_params.mpd_params);

  // Create encryption key source if needed.
  if (packaging_params.encryption_params.key_provider != KeyProvider::kNone) {
    if (encryption_options.protection_scheme == media::FOURCC_NULL)
      return Status(error::INVALID_ARGUMENT, "Invalid protection scheme.");
    internal->encryption_key_source =
        CreateEncryptionKeySource(encryption_options.protection_scheme,
                                  packaging_params.encryption_params);
    if (!internal->encryption_key_source)
      return Status(error::INVALID_ARGUMENT, "Failed to create key source.");
  }

  const MpdParams& mpd_params = packaging_params.mpd_params;
  if (!mpd_params.mpd_output.empty()) {
    if (mpd_params.generate_dash_if_iop_compliant_mpd) {
      internal->mpd_notifier.reset(new DashIopMpdNotifier(
          mpd_options, mpd_params.base_urls, mpd_params.mpd_output));
    } else {
      internal->mpd_notifier.reset(new SimpleMpdNotifier(
          mpd_options, mpd_params.base_urls, mpd_params.mpd_output));
    }
    if (!internal->mpd_notifier->Init()) {
      LOG(ERROR) << "MpdNotifier failed to initialize.";
      return Status(error::INVALID_ARGUMENT,
                    "Failed to initialize MpdNotifier.");
    }
  }

  const HlsParams& hls_params = packaging_params.hls_params;
  if (!hls_params.master_playlist_output.empty()) {
    base::FilePath master_playlist_path(
        base::FilePath::FromUTF8Unsafe(hls_params.master_playlist_output));
    base::FilePath master_playlist_name = master_playlist_path.BaseName();

    internal->hls_notifier.reset(new hls::SimpleHlsNotifier(
        hls::HlsNotifier::HlsProfile::kOnDemandProfile, hls_params.base_url,
        master_playlist_path.DirName().AsEndingWithSeparator().AsUTF8Unsafe(),
        master_playlist_name.AsUTF8Unsafe()));
  }

  media::StreamDescriptorList stream_descriptor_list;
  for (const StreamDescriptor& descriptor : stream_descriptors)
    stream_descriptor_list.insert(descriptor);
  if (!media::CreateRemuxJobs(
          stream_descriptor_list, packaging_params, chunking_options,
          encryption_options, muxer_options, &internal->fake_clock,
          internal->encryption_key_source.get(), internal->mpd_notifier.get(),
          internal->hls_notifier.get(), &internal->remux_jobs)) {
    return Status(error::INVALID_ARGUMENT, "Failed to create remux jobs.");
  }
  internal_ = std::move(internal);
  return Status::OK;
}

Status Packager::Run() {
  if (!internal_)
    return Status(error::INVALID_ARGUMENT, "Not yet initialized.");
  Status status = media::RunRemuxJobs(internal_->remux_jobs);
  if (!status.ok())
    return status;

  if (internal_->hls_notifier) {
    if (!internal_->hls_notifier->Flush())
      return Status(error::INVALID_ARGUMENT, "Failed to flush Hls.");
  }
  if (internal_->mpd_notifier) {
    if (!internal_->mpd_notifier->Flush())
      return Status(error::INVALID_ARGUMENT, "Failed to flush Mpd.");
  }
  return Status::OK;
}

void Packager::Cancel() {
  if (!internal_) {
    LOG(INFO) << "Not yet initialized. Return directly.";
    return;
  }
  for (const std::unique_ptr<media::RemuxJob>& job : internal_->remux_jobs)
    job->demuxer()->Cancel();
}

std::string Packager::GetLibraryVersion() {
  return GetPackagerVersion();
}

}  // namespace shaka
