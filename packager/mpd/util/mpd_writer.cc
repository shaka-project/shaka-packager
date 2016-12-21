// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/mpd/util/mpd_writer.h"

#include <gflags/gflags.h>
#include <google/protobuf/text_format.h>
#include <stdint.h>

#include "packager/base/files/file_path.h"
#include "packager/base/files/file_util.h"
#include "packager/media/file/file.h"
#include "packager/mpd/base/dash_iop_mpd_notifier.h"
#include "packager/mpd/base/mpd_builder.h"
#include "packager/mpd/base/mpd_notifier.h"
#include "packager/mpd/base/mpd_utils.h"
#include "packager/mpd/base/simple_mpd_notifier.h"

DEFINE_bool(generate_dash_if_iop_compliant_mpd,
            false,
            "Try to generate DASH-IF IOPv3 compliant MPD. This is best effort "
            "and does not guarantee compliance. Off by default until players "
            "support IOP MPDs.");

namespace shaka {

namespace {

// Factory that creates DashIopMpdNotifier instances.
class DashIopMpdNotifierFactory : public MpdNotifierFactory {
 public:
  DashIopMpdNotifierFactory() {}
  ~DashIopMpdNotifierFactory() override {}

  std::unique_ptr<MpdNotifier> Create(const MpdOptions& mpd_options,
                                      const std::vector<std::string>& base_urls,
                                      const std::string& output_path) override {
    return std::unique_ptr<MpdNotifier>(
        new DashIopMpdNotifier(mpd_options, base_urls, output_path));
  }
};

// Factory that creates SimpleMpdNotifier instances.
class SimpleMpdNotifierFactory : public MpdNotifierFactory {
 public:
  SimpleMpdNotifierFactory() {}
  ~SimpleMpdNotifierFactory() override {}

  std::unique_ptr<MpdNotifier> Create(const MpdOptions& mpd_options,
                                      const std::vector<std::string>& base_urls,
                                      const std::string& output_path) override {
    return std::unique_ptr<MpdNotifier>(
        new SimpleMpdNotifier(mpd_options, base_urls, output_path));
  }
};

}  // namespace

MpdWriter::MpdWriter()
    : notifier_factory_(FLAGS_generate_dash_if_iop_compliant_mpd
                            ? static_cast<MpdNotifierFactory*>(
                                  new DashIopMpdNotifierFactory())
                            : static_cast<MpdNotifierFactory*>(
                                  new SimpleMpdNotifierFactory())) {}
MpdWriter::~MpdWriter() {}

bool MpdWriter::AddFile(const std::string& media_info_path,
                        const std::string& mpd_path) {
  std::string file_content;
  if (!media::File::ReadFileToString(media_info_path.c_str(),
                                     &file_content)) {
    LOG(ERROR) << "Failed to read " << media_info_path << " to string.";
    return false;
  }

  MediaInfo media_info;
  if (!::google::protobuf::TextFormat::ParseFromString(file_content,
                                                       &media_info)) {
    LOG(ERROR) << "Failed to parse " << file_content << " to MediaInfo.";
    return false;
  }

  MpdBuilder::MakePathsRelativeToMpd(mpd_path, &media_info);
  media_infos_.push_back(media_info);
  return true;
}

void MpdWriter::AddBaseUrl(const std::string& base_url) {
  base_urls_.push_back(base_url);
}

bool MpdWriter::WriteMpdToFile(const char* file_name) {
  CHECK(file_name);
  std::unique_ptr<MpdNotifier> notifier =
      notifier_factory_->Create(MpdOptions(), base_urls_, file_name);
  if (!notifier->Init()) {
    LOG(ERROR) << "failed to initialize MpdNotifier.";
    return false;
  }

  for (std::list<MediaInfo>::const_iterator it = media_infos_.begin();
       it != media_infos_.end();
       ++it) {
    uint32_t unused_conatiner_id;
    if (!notifier->NotifyNewContainer(*it, &unused_conatiner_id)) {
      LOG(ERROR) << "Failed to add MediaInfo for media file: "
                 << it->media_file_name();
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
