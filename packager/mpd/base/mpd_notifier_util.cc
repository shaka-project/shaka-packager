// Copyright 2015 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/mpd/base/mpd_notifier_util.h>

#include <absl/log/check.h>
#include <absl/log/log.h>

#include <packager/file.h>
#include <packager/macros/logging.h>
#include <packager/mpd/base/mpd_utils.h>

namespace shaka {

bool WriteMpdToFile(const std::string& output_path, MpdBuilder* mpd_builder) {
  CHECK(!output_path.empty());

  std::string mpd;
  if (!mpd_builder->ToString(&mpd)) {
    LOG(ERROR) << "Failed to write MPD to string.";
    return false;
  }

  if (!File::WriteFileAtomically(output_path.c_str(), mpd)) {
    LOG(ERROR) << "Failed to write mpd to: " << output_path;
    return false;
  }
  return true;
}

ContentType GetContentType(const MediaInfo& media_info) {
  const bool has_video = media_info.has_video_info();
  const bool has_audio = media_info.has_audio_info();
  const bool has_text = media_info.has_text_info();

  if (MoreThanOneTrue(has_video, has_audio, has_text)) {
    NOTIMPLEMENTED() << "MediaInfo with more than one stream is not supported.";
    return kContentTypeUnknown;
  }
  if (!AtLeastOneTrue(has_video, has_audio, has_text)) {
    LOG(ERROR) << "MediaInfo should contain one audio, video, or text stream.";
    return kContentTypeUnknown;
  }
  return has_video ? kContentTypeVideo
                   : (has_audio ? kContentTypeAudio : kContentTypeText);
}

std::string Uint8VectorToBase64(const std::vector<uint8_t>& input) {
  std::string output;
  std::string input_in_string(input.begin(), input.end());
  absl::Base64Escape(input_in_string, &output);
  return output;
}

}  // namespace shaka
