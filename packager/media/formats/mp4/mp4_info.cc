#include "packager/media/formats/mp4/mp4_info.h"
#include "packager/file/file_closer.h"
#include "packager/media/base/media_sample.h"
#include "packager/media/base/stream_info.h"
#include "packager/media/formats/mp4/mp4_media_parser.h"

namespace shaka {
namespace media {
namespace mp4 {

static float roundToMs(float sec) {
  return round(sec * 1000.0) / 1000.0;
}

MP4Info::MP4Info(std::string filePath, size_t read_size)
    : file_path_(std::move(filePath)), read_chunk_size_(read_size) {
  parser_.reset(new MP4MediaParser());
}

bool MP4Info::Parse() {
  if (read_chunk_size_ == 0) {
    return false;
  }
  parser_->Init(std::bind(&MP4Info::InitF, this, std::placeholders::_1),
                std::bind(&MP4Info::NewSampleF, this, std::placeholders::_1,
                          std::placeholders::_2),
                std::bind(&MP4Info::NewTextSampleF, this, std::placeholders::_1,
                          std::placeholders::_2),
                nullptr);
  return FeedParserWithData(file_path_);
}

float MP4Info::GetVideoSamplesDurationSec() const {
  auto video_stream =
      std::find_if(streams_.begin(), streams_.end(),
                   [](const std::shared_ptr<StreamInfo>& stream) {
                     return stream->stream_type() == kStreamVideo;
                   });
  if (video_stream == streams_.end()) {  // no video stream
    return 0.0;
  }
  return roundToMs(
      float(samples_duration_map_.at((*video_stream)->track_id())) /
      (*video_stream)->time_scale());
}

bool MP4Info::FeedParserWithData(const std::string& name) {
  std::vector<uint8_t> buffer;
  buffer.reserve(read_chunk_size_);

  std::unique_ptr<shaka::File, shaka::FileCloser> mp4_file(
      File::Open(name.c_str(), "r"));
  if (!mp4_file) {
    return false;
  }

  int64_t read_size = 0;
  while (true) {
    read_size = mp4_file->Read(buffer.data(), read_chunk_size_);
    if (read_size < 0) {
      return false;
    }
    if (read_size == 0) {  // EOF
      break;
    }
    if (!parser_->Parse(buffer.data(), static_cast<int>(read_size))) {
      return false;
    }
  }
  return true;
}
void MP4Info::InitF(const std::vector<std::shared_ptr<StreamInfo> >& streams) {
  streams_ = streams;
}

bool MP4Info::NewSampleF(uint32_t track_id,
                         std::shared_ptr<MediaSample> sample) {
  samples_duration_map_[track_id] += sample->duration();
  return true;
}

bool MP4Info::NewTextSampleF(uint32_t track_id,
                             std::shared_ptr<TextSample> sample) {
  return false;
}

}  // namespace mp4
}  // namespace media
}  // namespace shaka