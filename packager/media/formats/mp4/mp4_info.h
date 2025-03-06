#ifndef PACKAGER_MEDIA_FORMATS_MP4_MP4_INFO_H_
#define PACKAGER_MEDIA_FORMATS_MP4_MP4_INFO_H_

#include <stdint.h>
#include <memory>
#include <vector>
#include "packager/media/formats/mp4/mp4_media_parser.h"

namespace shaka {
namespace media {
namespace mp4 {

const size_t kDefaultInfoReadSize = 0x10000;  // 64 KB

class MP4Info {
 private:
  std::vector<std::shared_ptr<StreamInfo>> streams_;
  std::map<uint32_t, size_t> samples_duration_map_;
  std::unique_ptr<MP4MediaParser> parser_;
  std::string file_path_;
  size_t read_chunk_size_;

 public:
  MP4Info(std::string filePath, size_t read_size);
  bool Parse();
  // Info
  float GetVideoSamplesDurationSec() const;

 protected:
  bool FeedParserWithData(const std::string& name);
  void InitF(const std::vector<std::shared_ptr<StreamInfo>>& streams);
  bool NewSampleF(uint32_t track_id, std::shared_ptr<MediaSample> sample);
  bool NewTextSampleF(uint32_t track_id, std::shared_ptr<TextSample> sample);

  DISALLOW_COPY_AND_ASSIGN(MP4Info);
};

}  // namespace mp4
}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_FORMATS_MP4_MP4_INFO_H_