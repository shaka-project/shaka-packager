// Class for reading in MediaInfo from files and writing out an MPD.
#ifndef MPD_UTIL_MPD_WRITER_H_
#define MPD_UTIL_MPD_WRITER_H_

#include <list>
#include <string>

#include "base/basictypes.h"

namespace media {

class File;

}  // namespace media

namespace dash_packager {

class MediaInfo;

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

  // Add |file_name| for MPD generation. |file_name| should not be NULL.
  // The content of |media_info_file| should be a string representation of
  // MediaInfo, i.e. the content should be a result of using
  // google::protobuf::TestFormat::Print*() methods.
  // If necessary, this method can be called after WriteMpd*() methods.
  bool AddFile(const char* file_name);

  // |base_url| will be used for <BaseURL> element for the MPD. The BaseURL
  // element will be a direct child element of the <MPD> element.
  void AddBaseUrl(const std::string& base_url);

  // Write the MPD to |output|. |output| should not be NULL.
  // AddFile() should be called before calling this function to generate an MPD.
  // On success, MPD is set to |output| and returns true, otherwise returns
  // false.
  // This method can be called multiple times, if necessary.
  bool WriteMpdToString(std::string* output);

  // Write the MPD to |file_name|. |file_name| should not be NULL.
  // This opens the file in write mode, IOW if the
  // file exists this will over write whatever is in the file.
  // AddFile() should be called before calling this function to generate an MPD.
  // On success, the MPD gets written to |file| and returns true, otherwise
  // returns false.
  // This method can be called multiple times, if necessary.
  bool WriteMpdToFile(const char* file_name);

 private:
  std::list<MediaInfo> media_infos_;
  std::list<std::string> base_urls_;

  DISALLOW_COPY_AND_ASSIGN(MpdWriter);
};

}  // namespace dash_packager

#endif  // MPD_UTIL_MPD_WRITER_H_
