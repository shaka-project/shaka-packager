// Copyright 2014 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd
//
// Class for reading in MediaInfo from files and writing out an MPD.

#ifndef MPD_UTIL_MPD_WRITER_H_
#define MPD_UTIL_MPD_WRITER_H_

#include <list>
#include <memory>
#include <string>
#include <vector>

#include <packager/macros/classes.h>
#include <packager/mpd/base/mpd_notifier.h>
#include <packager/mpd/base/mpd_options.h>

namespace shaka {

namespace media {
class File;
}  // namespace media

class MediaInfo;

/// This is mainly for testing, and is implementation detail. No need to worry
/// about this class if you are just using the API.
/// Inject a factory and mock MpdNotifier to test the MpdWriter implementation.
class MpdNotifierFactory {
 public:
  MpdNotifierFactory() {}
  virtual ~MpdNotifierFactory() {}

  virtual std::unique_ptr<MpdNotifier> Create(
      const MpdOptions& mpd_options) = 0;
};

// An instance of this class takes a set of MediaInfo files and generates an
// MPD when one of WriteMpd* methods are called. This generates an MPD with one
// <Period> element and at most three <AdaptationSet> elements, each for video,
// audio, and text. Information in MediaInfo will be put into one of the
// AdaptationSets by checking the video_info, audio_info, and text_info fields.
// Therefore, this cannot handle an instance of MediaInfo with video, audio, and
// text combination.
class MpdWriter {
 public:
  MpdWriter();
  ~MpdWriter();

  // Add |media_info_path| for MPD generation.
  // The content of |media_info_path| should be a string representation of
  // MediaInfo, i.e. the content should be a result of using
  // google::protobuf::TestFormat::Print*() methods.
  // If necessary, this method can be called after WriteMpd*() methods.
  bool AddFile(const std::string& media_info_path);

  // |base_url| will be used for <BaseURL> element for the MPD. The BaseURL
  // element will be a direct child element of the <MPD> element.
  void AddBaseUrl(const std::string& base_url);

  // Write the MPD to |file_name|. |file_name| should not be NULL.
  // This opens the file in write mode, IOW if the
  // file exists this will over write whatever is in the file.
  // AddFile() should be called before calling this function to generate an MPD.
  // On success, the MPD gets written to |file| and returns true, otherwise
  // returns false.
  // This method can be called multiple times, if necessary.
  bool WriteMpdToFile(const char* file_name);

 private:
  friend class MpdWriterTest;

  void SetMpdNotifierFactoryForTest(
      std::unique_ptr<MpdNotifierFactory> factory);

  std::list<MediaInfo> media_infos_;
  std::vector<std::string> base_urls_;

  std::unique_ptr<MpdNotifierFactory> notifier_factory_;

  DISALLOW_COPY_AND_ASSIGN(MpdWriter);
};

}  // namespace shaka

#endif  // MPD_UTIL_MPD_WRITER_H_
