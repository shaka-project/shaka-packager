// Copyright 2014 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/mpd/util/mpd_writer.h>

#include <cstdint>

#include <absl/flags/flag.h>
#include <absl/log/check.h>
#include <absl/log/log.h>
#include <google/protobuf/text_format.h>

#include <packager/file.h>
#include <packager/mpd/base/mpd_builder.h>
#include <packager/mpd/base/mpd_notifier.h>
#include <packager/mpd/base/mpd_utils.h>
#include <packager/mpd/base/simple_mpd_notifier.h>

ABSL_FLAG(bool,
          generate_dash_if_iop_compliant_mpd,
          true,
          "Try to generate DASH-IF IOP compliant MPD. This is best effort "
          "and does not guarantee compliance.");

namespace shaka {

namespace {

// Factory that creates SimpleMpdNotifier instances.
class SimpleMpdNotifierFactory : public MpdNotifierFactory {
 public:
  SimpleMpdNotifierFactory() {}
  ~SimpleMpdNotifierFactory() override {}

  std::unique_ptr<MpdNotifier> Create(const MpdOptions& mpd_options) override {
    return std::unique_ptr<MpdNotifier>(new SimpleMpdNotifier(mpd_options));
  }
};

}  // namespace

MpdWriter::MpdWriter() : notifier_factory_(new SimpleMpdNotifierFactory()) {}
MpdWriter::~MpdWriter() {}

bool MpdWriter::AddFile(const std::string& media_info_path) {
  std::string file_content;
  if (!File::ReadFileToString(media_info_path.c_str(), &file_content)) {
    LOG(ERROR) << "Failed to read " << media_info_path << " to string.";
    return false;
  }

  MediaInfo media_info;
  if (!::google::protobuf::TextFormat::ParseFromString(file_content,
                                                       &media_info)) {
    LOG(ERROR) << "Failed to parse " << file_content << " to MediaInfo.";
    return false;
  }

  media_infos_.push_back(media_info);
  return true;
}

void MpdWriter::AddBaseUrl(const std::string& base_url) {
  base_urls_.push_back(base_url);
}

bool MpdWriter::WriteMpdToFile(const char* file_name) {
  CHECK(file_name);
  MpdOptions mpd_options;
  mpd_options.mpd_params.base_urls = base_urls_;
  mpd_options.mpd_params.mpd_output = file_name;
  mpd_options.mpd_params.generate_dash_if_iop_compliant_mpd =
      absl::GetFlag(FLAGS_generate_dash_if_iop_compliant_mpd);
  std::unique_ptr<MpdNotifier> notifier =
      notifier_factory_->Create(mpd_options);
  if (!notifier->Init()) {
    LOG(ERROR) << "failed to initialize MpdNotifier.";
    return false;
  }

  for (const MediaInfo& media_info : media_infos_) {
    uint32_t unused_conatiner_id;
    if (!notifier->NotifyNewContainer(media_info, &unused_conatiner_id)) {
      LOG(ERROR) << "Failed to add MediaInfo for media file: "
                 << media_info.media_file_name();
      return false;
    }
  }

  if (!notifier->Flush()) {
    LOG(ERROR) << "Failed to flush MPD notifier.";
    return false;
  }
  return true;
}

void MpdWriter::SetMpdNotifierFactoryForTest(
    std::unique_ptr<MpdNotifierFactory> factory) {
  notifier_factory_ = std::move(factory);
}

}  // namespace shaka
