// Copyright 2017 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/packager.h"

#include <algorithm>

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
#include "packager/file/file.h"
#include "packager/hls/base/hls_notifier.h"
#include "packager/hls/base/simple_hls_notifier.h"
#include "packager/media/base/container_names.h"
#include "packager/media/base/fourccs.h"
#include "packager/media/base/key_source.h"
#include "packager/media/base/language_utils.h"
#include "packager/media/base/muxer_options.h"
#include "packager/media/base/muxer_util.h"
#include "packager/media/chunking/chunking_handler.h"
#include "packager/media/crypto/encryption_handler.h"
#include "packager/media/demuxer/demuxer.h"
#include "packager/media/event/combined_muxer_listener.h"
#include "packager/media/event/hls_notify_muxer_listener.h"
#include "packager/media/event/mpd_notify_muxer_listener.h"
#include "packager/media/event/vod_media_info_dump_muxer_listener.h"
#include "packager/media/formats/mp2t/ts_muxer.h"
#include "packager/media/formats/mp4/mp4_muxer.h"
#include "packager/media/formats/webm/webm_muxer.h"
#include "packager/media/replicator/replicator.h"
#include "packager/media/trick_play/trick_play_handler.h"
#include "packager/mpd/base/dash_iop_mpd_notifier.h"
#include "packager/mpd/base/media_info.pb.h"
#include "packager/mpd/base/mpd_builder.h"
#include "packager/mpd/base/simple_mpd_notifier.h"
#include "packager/version/version.h"

namespace shaka {

// TODO(kqyang): Clean up namespaces.
using media::Demuxer;
using media::KeySource;
using media::MuxerOptions;

namespace media {
namespace {

const char kMediaInfoSuffix[] = ".media_info";

// TODO(rkuroiwa): Write TTML and WebVTT parser (demuxing) for a better check
// and for supporting live/segmenting (muxing).  With a demuxer and a muxer,
// CreateAllJobs() shouldn't treat text as a special case.
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

Status ValidateStreamDescriptor(bool dump_stream_info,
                                const StreamDescriptor& stream) {
  if (stream.input.empty()) {
    return Status(error::INVALID_ARGUMENT, "Stream input not specified.");
  }

  // The only time a stream can have no outputs, is when dump stream info is
  // set.
  if (dump_stream_info && stream.output.empty() &&
      stream.segment_template.empty()) {
    return Status::OK;
  }

  if (stream.output.empty() && stream.segment_template.empty()) {
    return Status(error::INVALID_ARGUMENT,
                  "Streams must specify 'output' or 'segment template'.");
  }

  // Whenever there is output, a stream must be selected.
  if (stream.stream_selector.empty()) {
    return Status(error::INVALID_ARGUMENT,
                  "Stream stream_selector not specified.");
  }

  // If a segment template is provided, it must be valid.
  if (stream.segment_template.length()) {
    Status template_check = ValidateSegmentTemplate(stream.segment_template);
    if (!template_check.ok()) {
      return template_check;
    }
  }

  // There are some specifics that must be checked based on which format
  // we are writing to.
  const MediaContainerName output_format = GetOutputFormat(stream);

  if (output_format == CONTAINER_UNKNOWN) {
    return Status(error::INVALID_ARGUMENT, "Unsupported output format.");
  } else if (output_format == MediaContainerName::CONTAINER_MPEG2TS) {
    if (stream.segment_template.empty()) {
      return Status(error::INVALID_ARGUMENT,
                    "Please specify segment_template. Single file TS output is "
                    "not supported.");
    }

    // Right now the init segment is saved in |output| for multi-segment
    // content. However, for TS all segments must be self-initializing so
    // there cannot be an init segment.
    if (stream.output.length()) {
      return Status(error::INVALID_ARGUMENT,
                    "All TS segments must be self-initializing. Stream "
                    "descriptors 'output' or 'init_segment' are not allowed.");
    }
  } else {
    // For any other format, if there is a segment template, there must be an
    // init segment provided.
    if (stream.segment_template.length() && stream.output.empty()) {
      return Status(error::INVALID_ARGUMENT,
                    "Please specify 'init_segment'. All non-TS multi-segment "
                    "content must provide an init segment.");
    }
  }

  return Status::OK;
}

Status ValidateParams(const PackagingParams& packaging_params,
                      const std::vector<StreamDescriptor>& stream_descriptors) {
  if (!packaging_params.chunking_params.segment_sap_aligned &&
      packaging_params.chunking_params.subsegment_sap_aligned) {
    return Status(error::INVALID_ARGUMENT,
                  "Setting segment_sap_aligned to false but "
                  "subsegment_sap_aligned to true is not allowed.");
  }

  if (stream_descriptors.empty()) {
    return Status(error::INVALID_ARGUMENT,
                  "Stream descriptors cannot be empty.");
  }

  // On demand profile generates single file segment while live profile
  // generates multiple segments specified using segment template.
  const bool on_demand_dash_profile =
      stream_descriptors.begin()->segment_template.empty();
  for (const auto& descriptor : stream_descriptors) {
    if (on_demand_dash_profile != descriptor.segment_template.empty()) {
      return Status(error::INVALID_ARGUMENT,
                    "Inconsistent stream descriptor specification: "
                    "segment_template should be specified for none or all "
                    "stream descriptors.");
    }

    Status stream_check = ValidateStreamDescriptor(
        packaging_params.test_params.dump_stream_info, descriptor);

    if (!stream_check.ok()) {
      return stream_check;
    }
  }

  if (packaging_params.output_media_info && !on_demand_dash_profile) {
    // TODO(rkuroiwa, kqyang): Support partial media info dump for live.
    return Status(error::UNIMPLEMENTED,
                  "--output_media_info is only supported for on-demand profile "
                  "(not using segment_template).");
  }

  return Status::OK;
}

bool StreamDescriptorCompareFn(const StreamDescriptor& a,
                               const StreamDescriptor& b) {
  if (a.input == b.input) {
    if (a.stream_selector == b.stream_selector) {
      // The MPD notifier requires that the main track comes first, so make
      // sure that happens.
      if (a.trick_play_factor == 0 || b.trick_play_factor == 0) {
        return a.trick_play_factor == 0;
      } else {
        return a.trick_play_factor > b.trick_play_factor;
      }
    } else {
      return a.stream_selector < b.stream_selector;
    }
  }

  return a.input < b.input;
}

// A fake clock that always return time 0 (epoch). Should only be used for
// testing.
class FakeClock : public base::Clock {
 public:
  base::Time Now() override { return base::Time(); }
};

class Job : public base::SimpleThread {
 public:
  Job(const std::string& name, std::shared_ptr<OriginHandler> work)
      : SimpleThread(name),
        work_(work),
        wait_(base::WaitableEvent::ResetPolicy::MANUAL,
              base::WaitableEvent::InitialState::NOT_SIGNALED) {}

  void Initialize() {
    DCHECK(work_);
    status_ = work_->Initialize();
  }

  void Cancel() {
    DCHECK(work_);
    work_->Cancel();
  }

  const Status& status() const { return status_; }

  base::WaitableEvent* wait() { return &wait_; }

 private:
  Job(const Job&) = delete;
  Job& operator=(const Job&) = delete;

  void Run() override {
    DCHECK(work_);
    status_ = work_->Run();
    wait_.Signal();
  }

  std::shared_ptr<OriginHandler> work_;
  Status status_;

  base::WaitableEvent wait_;
};

bool StreamInfoToTextMediaInfo(const StreamDescriptor& stream_descriptor,
                               MediaInfo* text_media_info) {
  const std::string& language = stream_descriptor.language;
  const std::string format = DetermineTextFileFormat(stream_descriptor.input);
  if (format.empty()) {
    LOG(ERROR) << "Failed to determine the text file format for "
               << stream_descriptor.input;
    return false;
  }

  if (!File::Copy(stream_descriptor.input.c_str(),
                  stream_descriptor.output.c_str())) {
    LOG(ERROR) << "Failed to copy the input file (" << stream_descriptor.input
               << ") to output file (" << stream_descriptor.output << ").";
    return false;
  }

  text_media_info->set_media_file_name(stream_descriptor.output);
  text_media_info->set_container_type(MediaInfo::CONTAINER_TEXT);

  if (stream_descriptor.bandwidth != 0) {
    text_media_info->set_bandwidth(stream_descriptor.bandwidth);
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

std::unique_ptr<MuxerListener> CreateMuxerListener(
    const StreamDescriptor& stream,
    int stream_number,
    bool output_media_info,
    MpdNotifier* mpd_notifier,
    hls::HlsNotifier* hls_notifier) {
  std::unique_ptr<CombinedMuxerListener> combined_listener(
      new CombinedMuxerListener);

  if (output_media_info) {
    std::unique_ptr<MuxerListener> listener(
        new VodMediaInfoDumpMuxerListener(stream.output + kMediaInfoSuffix));
    combined_listener->AddListener(std::move(listener));
  }

  if (mpd_notifier) {
    std::unique_ptr<MuxerListener> listener(
        new MpdNotifyMuxerListener(mpd_notifier));
    combined_listener->AddListener(std::move(listener));
  }

  if (hls_notifier) {
    // TODO(rkuroiwa): Do some smart stuff to group the audios, e.g. detect
    // languages.
    std::string group_id = stream.hls_group_id;
    std::string name = stream.hls_name;
    std::string hls_playlist_name = stream.hls_playlist_name;
    if (group_id.empty())
      group_id = "audio";
    if (name.empty())
      name = base::StringPrintf("stream_%d", stream_number);
    if (hls_playlist_name.empty())
      hls_playlist_name = base::StringPrintf("stream_%d.m3u8", stream_number);

    std::unique_ptr<MuxerListener> listener(new HlsNotifyMuxerListener(
        hls_playlist_name, name, group_id, hls_notifier));
    combined_listener->AddListener(std::move(listener));
  }

  return std::move(combined_listener);
}

std::shared_ptr<Muxer> CreateMuxer(const PackagingParams& packaging_params,
                                   const StreamDescriptor& stream,
                                   base::Clock* clock,
                                   std::unique_ptr<MuxerListener> listener) {
  const MediaContainerName format = GetOutputFormat(stream);

  MuxerOptions options;
  options.mp4_params = packaging_params.mp4_output_params;
  options.temp_dir = packaging_params.temp_dir;
  options.bandwidth = stream.bandwidth;
  options.output_file_name = stream.output;
  options.segment_template = stream.segment_template;

  std::shared_ptr<Muxer> muxer;

  switch (format) {
    case CONTAINER_WEBM:
      muxer = std::make_shared<webm::WebMMuxer>(options);
      break;
    case CONTAINER_MPEG2TS:
      muxer = std::make_shared<mp2t::TsMuxer>(options);
      break;
    case CONTAINER_MOV:
      muxer = std::make_shared<mp4::MP4Muxer>(options);
      break;
    default:
      LOG(ERROR) << "Cannot support muxing to " << format;
      break;
  }

  if (!muxer) {
    return nullptr;
  }

  // We successfully created a muxer, then there is a couple settings
  // we should set before returning it.
  if (clock) {
    muxer->set_clock(clock);
  }

  if (listener) {
    muxer->SetMuxerListener(std::move(listener));
  }

  return muxer;
}

std::shared_ptr<MediaHandler> CreateEncryptionHandler(
    const PackagingParams& packaging_params,
    const StreamDescriptor& stream,
    KeySource* key_source) {
  if (stream.skip_encryption) {
    return nullptr;
  }

  if (!key_source) {
    return nullptr;
  }

  // Make a copy so that we can modify it for this specific stream.
  EncryptionParams encryption_params = packaging_params.encryption_params;

  // Use Sample AES in MPEG2TS.
  // TODO(kqyang): Consider adding a new flag to enable Sample AES as we
  // will support CENC in TS in the future.
  if (GetOutputFormat(stream) == CONTAINER_MPEG2TS) {
    VLOG(1) << "Use Apple Sample AES encryption for MPEG2TS.";
    encryption_params.protection_scheme = kAppleSampleAesProtectionScheme;
  }

  if (!stream.drm_label.empty()) {
    const std::string& drm_label = stream.drm_label;
    encryption_params.stream_label_func =
        [drm_label](const EncryptionParams::EncryptedStreamAttributes&) {
          return drm_label;
        };
  } else if (!encryption_params.stream_label_func) {
    const int kDefaultMaxSdPixels = 768 * 576;
    const int kDefaultMaxHdPixels = 1920 * 1080;
    const int kDefaultMaxUhd1Pixels = 4096 * 2160;
    encryption_params.stream_label_func = std::bind(
        &Packager::DefaultStreamLabelFunction, kDefaultMaxSdPixels,
        kDefaultMaxHdPixels, kDefaultMaxUhd1Pixels, std::placeholders::_1);
  }

  return std::make_shared<EncryptionHandler>(encryption_params, key_source);
}

Status CreateTextJobs(
    int first_stream_number,
    const std::vector<std::reference_wrapper<const StreamDescriptor>>& streams,
    const PackagingParams& packaging_params,
    MpdNotifier* mpd_notifier,
    std::vector<std::unique_ptr<Job>>* jobs) {
  // -1 so that it will be back at |first_stream_number| on the first
  // iteration.
  int stream_number = first_stream_number - 1;

  for (const StreamDescriptor& stream : streams) {
    stream_number += 1;

    const MediaContainerName output_format = GetOutputFormat(stream);

    if (output_format == CONTAINER_MOV) {
      // TODO(vaage): Complete this part of the text pipeline. This path will
      // be similar to the audio/video pipeline but with some components
      // removed (e.g. trick play).
    } else {
      MediaInfo text_media_info;
      if (!StreamInfoToTextMediaInfo(stream, &text_media_info)) {
        return Status(error::INVALID_ARGUMENT,
                      "Could not create media info for stream.");
      }

      if (mpd_notifier) {
        uint32_t unused;
        if (mpd_notifier->NotifyNewContainer(text_media_info, &unused)) {
          mpd_notifier->Flush();
        } else {
          return Status(error::PARSER_FAILURE,
                        "Failed to process text file " + stream.input);
        }
      }

      if (packaging_params.output_media_info) {
        VodMediaInfoDumpMuxerListener::WriteMediaInfoToFile(
            text_media_info, stream.output + kMediaInfoSuffix);
      }
    }
  }

  return Status::OK;
}

Status CreateAudioVideoJobs(
    int first_stream_number,
    const std::vector<std::reference_wrapper<const StreamDescriptor>>& streams,
    const PackagingParams& packaging_params,
    FakeClock* fake_clock,
    KeySource* encryption_key_source,
    MpdNotifier* mpd_notifier,
    hls::HlsNotifier* hls_notifier,
    std::vector<std::unique_ptr<Job>>* jobs) {
  DCHECK(jobs);

  // Demuxers are shared among all streams with the same input.
  std::shared_ptr<Demuxer> demuxer;
  // Replicators are shared among all streams with the same input and stream
  // selector.
  std::shared_ptr<MediaHandler> replicator;

  std::string previous_input;
  std::string previous_selector;

  // -1 so that it will be back at |first_stream_number| on the first
  // iteration.
  int stream_number = first_stream_number - 1;

  for (const StreamDescriptor& stream : streams) {
    stream_number += 1;

    // If we changed our input files, we need a new demuxer.
    if (previous_input != stream.input) {
      demuxer = std::make_shared<Demuxer>(stream.input);

      demuxer->set_dump_stream_info(
          packaging_params.test_params.dump_stream_info);
      if (packaging_params.decryption_params.key_provider !=
          KeyProvider::kNone) {
        std::unique_ptr<KeySource> decryption_key_source(
            CreateDecryptionKeySource(packaging_params.decryption_params));
        if (!decryption_key_source) {
          return Status(
              error::INVALID_ARGUMENT,
              "Must define decryption key source when defining key provider");
        }
        demuxer->SetKeySource(std::move(decryption_key_source));
      }

      jobs->emplace_back(new media::Job("RemuxJob", demuxer));
    }

    if (!stream.language.empty()) {
      demuxer->SetLanguageOverride(stream.stream_selector, stream.language);
    }

    const bool new_source = previous_input != stream.input ||
                            previous_selector != stream.stream_selector;
    previous_input = stream.input;
    previous_selector = stream.stream_selector;

    // If the stream has no output, then there is no reason setting-up the rest
    // of the pipeline.
    if (stream.output.empty() && stream.segment_template.empty()) {
      continue;
    }

    if (new_source) {
      replicator = std::make_shared<Replicator>();

      std::shared_ptr<MediaHandler> chunker =
          std::make_shared<ChunkingHandler>(packaging_params.chunking_params);

      std::shared_ptr<MediaHandler> encryptor = CreateEncryptionHandler(
          packaging_params, stream, encryption_key_source);

      Status status;

      // The path is different if there is encryption. Even though there are
      // common elements, it is easier to understand the path if they are
      // expressed independently of each other.
      if (encryptor) {
        status.Update(demuxer->SetHandler(stream.stream_selector, chunker));
        status.Update(chunker->AddHandler(encryptor));
        status.Update(encryptor->AddHandler(replicator));
      } else {
        status.Update(demuxer->SetHandler(stream.stream_selector, chunker));
        status.Update(chunker->AddHandler(replicator));
      }

      if (!status.ok()) {
        return status;
      }

      if (!stream.language.empty()) {
        demuxer->SetLanguageOverride(stream.stream_selector, stream.language);
      }
    }

    // Create the muxer (output) for this track.
    std::unique_ptr<MuxerListener> muxer_listener = CreateMuxerListener(
        stream, stream_number, packaging_params.output_media_info, mpd_notifier,
        hls_notifier);
    std::shared_ptr<Muxer> muxer = CreateMuxer(
        packaging_params, stream,
        packaging_params.test_params.inject_fake_clock ? fake_clock : nullptr,
        std::move(muxer_listener));

    if (!muxer) {
      return Status(error::INVALID_ARGUMENT, "Failed to create muxer for " +
                                                 stream.input + ":" +
                                                 stream.stream_selector);
    }

    std::shared_ptr<MediaHandler> trick_play;
    if (stream.trick_play_factor) {
      trick_play = std::make_shared<TrickPlayHandler>(stream.trick_play_factor);
    }

    Status status;
    if (trick_play) {
      status.Update(replicator->AddHandler(trick_play));
      status.Update(trick_play->AddHandler(muxer));
    } else {
      status.Update(replicator->AddHandler(muxer));
    }

    if (!status.ok()) {
      return status;
    }
  }

  return Status::OK;
}

Status CreateAllJobs(const std::vector<StreamDescriptor>& stream_descriptors,
                     const PackagingParams& packaging_params,
                     FakeClock* fake_clock,
                     KeySource* encryption_key_source,
                     MpdNotifier* mpd_notifier,
                     hls::HlsNotifier* hls_notifier,
                     std::vector<std::unique_ptr<Job>>* jobs) {
  DCHECK(jobs);

  // Group all streams based on which pipeline they will use.
  std::vector<std::reference_wrapper<const StreamDescriptor>> text_streams;
  std::vector<std::reference_wrapper<const StreamDescriptor>>
      audio_video_streams;

  for (const StreamDescriptor& stream : stream_descriptors) {
    // TODO: Find a better way to determine what stream type a stream
    // descriptor is as |stream_selector| may use an index. This would
    // also allow us to use a simpler audio pipeline.
    if (stream.stream_selector == "text") {
      text_streams.push_back(stream);
    } else {
      audio_video_streams.push_back(stream);
    }
  }

  // Audio/Video streams need to be in sorted order so that demuxers and trick
  // play handlers get setup correctly.
  std::sort(audio_video_streams.begin(), audio_video_streams.end(),
            media::StreamDescriptorCompareFn);

  int stream_number = 0;

  Status status;

  status.Update(CreateTextJobs(stream_number, text_streams, packaging_params,
                               mpd_notifier, jobs));

  stream_number += text_streams.size();

  status.Update(CreateAudioVideoJobs(
      stream_number, audio_video_streams, packaging_params, fake_clock,
      encryption_key_source, mpd_notifier, hls_notifier, jobs));

  if (!status.ok()) {
    return status;
  }

  // Initialize processing graph.
  for (const std::unique_ptr<Job>& job : *jobs) {
    job->Initialize();
    status.Update(job->status());
  }

  return status;
}

Status RunJobs(const std::vector<std::unique_ptr<Job>>& jobs) {
  // We need to store the jobs and the waits separately in order to use the
  // |WaitMany| function. |WaitMany| takes an array of WaitableEvents but we
  // need to access the jobs in order to join the thread and check the status.
  // The indexes needs to be check in sync or else we won't be able to relate a
  // WaitableEvent back to the job.
  std::vector<Job*> active_jobs;
  std::vector<base::WaitableEvent*> active_waits;

  // Start every job and add it to the active jobs list so that we can wait
  // on each one.
  for (auto& job : jobs) {
    job->Start();

    active_jobs.push_back(job.get());
    active_waits.push_back(job->wait());
  }

  // Wait for all jobs to complete or an error occurs.
  Status status;
  while (status.ok() && active_jobs.size()) {
    // Wait for an event to finish and then update our status so that we can
    // quit if something has gone wrong.
    const size_t done =
        base::WaitableEvent::WaitMany(active_waits.data(), active_waits.size());
    Job* job = active_jobs[done];

    job->Join();
    status.Update(job->status());

    // Remove the job and the wait from our tracking.
    active_jobs.erase(active_jobs.begin() + done);
    active_waits.erase(active_waits.begin() + done);
  }

  // If the main loop has exited and there are still jobs running,
  // we need to cancel them and clean-up.
  for (auto& job : active_jobs) {
    job->Cancel();
  }

  for (auto& job : active_jobs) {
    job->Join();
  }

  return status;
}

}  // namespace
}  // namespace media

struct Packager::PackagerInternal {
  media::FakeClock fake_clock;
  std::unique_ptr<KeySource> encryption_key_source;
  std::unique_ptr<MpdNotifier> mpd_notifier;
  std::unique_ptr<hls::HlsNotifier> hls_notifier;
  std::vector<std::unique_ptr<media::Job>> jobs;
  BufferCallbackParams buffer_callback_params;
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

  Status param_check =
      media::ValidateParams(packaging_params, stream_descriptors);
  if (!param_check.ok()) {
    return param_check;
  }

  if (!packaging_params.test_params.injected_library_version.empty()) {
    SetPackagerVersionForTesting(
        packaging_params.test_params.injected_library_version);
  }

  std::unique_ptr<PackagerInternal> internal(new PackagerInternal);

  // Create encryption key source if needed.
  if (packaging_params.encryption_params.key_provider != KeyProvider::kNone) {
    internal->encryption_key_source = CreateEncryptionKeySource(
        static_cast<media::FourCC>(
            packaging_params.encryption_params.protection_scheme),
        packaging_params.encryption_params);
    if (!internal->encryption_key_source)
      return Status(error::INVALID_ARGUMENT, "Failed to create key source.");
  }

  // Store callback params to make it available during packaging.
  internal->buffer_callback_params = packaging_params.buffer_callback_params;

  // Update mpd output and hls output if callback param is specified.
  MpdParams mpd_params = packaging_params.mpd_params;
  HlsParams hls_params = packaging_params.hls_params;
  if (internal->buffer_callback_params.write_func) {
    mpd_params.mpd_output = File::MakeCallbackFileName(
        internal->buffer_callback_params, mpd_params.mpd_output);
    hls_params.master_playlist_output = File::MakeCallbackFileName(
        internal->buffer_callback_params, hls_params.master_playlist_output);
  }

  if (!mpd_params.mpd_output.empty()) {
    const bool on_demand_dash_profile =
        stream_descriptors.begin()->segment_template.empty();
    MpdOptions mpd_options =
        media::GetMpdOptions(on_demand_dash_profile, mpd_params);
    if (mpd_params.generate_dash_if_iop_compliant_mpd) {
      internal->mpd_notifier.reset(new DashIopMpdNotifier(mpd_options));
    } else {
      internal->mpd_notifier.reset(new SimpleMpdNotifier(mpd_options));
    }
    if (!internal->mpd_notifier->Init()) {
      LOG(ERROR) << "MpdNotifier failed to initialize.";
      return Status(error::INVALID_ARGUMENT,
                    "Failed to initialize MpdNotifier.");
    }
  }

  if (!hls_params.master_playlist_output.empty()) {
    base::FilePath master_playlist_path(
        base::FilePath::FromUTF8Unsafe(hls_params.master_playlist_output));
    base::FilePath master_playlist_name = master_playlist_path.BaseName();

    internal->hls_notifier.reset(new hls::SimpleHlsNotifier(
        hls_params.playlist_type, hls_params.time_shift_buffer_depth,
        hls_params.base_url,
        master_playlist_path.DirName().AsEndingWithSeparator().AsUTF8Unsafe(),
        master_playlist_name.AsUTF8Unsafe()));
  }

  std::vector<StreamDescriptor> streams_for_jobs;

  for (const StreamDescriptor& descriptor : stream_descriptors) {
    // We may need to overwrite some values, so make a copy first.
    StreamDescriptor copy = descriptor;

    if (internal->buffer_callback_params.read_func) {
      copy.input = File::MakeCallbackFileName(internal->buffer_callback_params,
                                              descriptor.input);
    }

    if (internal->buffer_callback_params.write_func) {
      copy.output = File::MakeCallbackFileName(internal->buffer_callback_params,
                                               descriptor.output);
      copy.segment_template = File::MakeCallbackFileName(
          internal->buffer_callback_params, descriptor.segment_template);
    }

    // Update language to ISO_639_2 code if set.
    if (!copy.language.empty()) {
      copy.language = LanguageToISO_639_2(descriptor.language);
      if (copy.language == "und") {
        return Status(
            error::INVALID_ARGUMENT,
            "Unknown/invalid language specified: " + descriptor.language);
      }
    }

    streams_for_jobs.push_back(copy);
  }

  Status status = media::CreateAllJobs(
      streams_for_jobs, packaging_params, &internal->fake_clock,
      internal->encryption_key_source.get(), internal->mpd_notifier.get(),
      internal->hls_notifier.get(), &internal->jobs);

  if (!status.ok()) {
    return status;
  }

  internal_ = std::move(internal);
  return Status::OK;
}

Status Packager::Run() {
  if (!internal_)
    return Status(error::INVALID_ARGUMENT, "Not yet initialized.");
  Status status = media::RunJobs(internal_->jobs);
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
  for (const std::unique_ptr<media::Job>& job : internal_->jobs)
    job->Cancel();
}

std::string Packager::GetLibraryVersion() {
  return GetPackagerVersion();
}

std::string Packager::DefaultStreamLabelFunction(
    int max_sd_pixels,
    int max_hd_pixels,
    int max_uhd1_pixels,
    const EncryptionParams::EncryptedStreamAttributes& stream_attributes) {
  if (stream_attributes.stream_type ==
      EncryptionParams::EncryptedStreamAttributes::kAudio)
    return "AUDIO";
  if (stream_attributes.stream_type ==
      EncryptionParams::EncryptedStreamAttributes::kVideo) {
    const int pixels = stream_attributes.oneof.video.width *
                       stream_attributes.oneof.video.height;
    if (pixels <= max_sd_pixels)
      return "SD";
    if (pixels <= max_hd_pixels)
      return "HD";
    if (pixels <= max_uhd1_pixels)
      return "UHD1";
    return "UHD2";
  }
  return "";
}

}  // namespace shaka
