#include "packager/media/formats/webvtt/cue.h"

#include "packager/base/strings/string_util.h"

namespace shaka {
namespace media {

Cue::Cue() : start_time(0), duration(0) {}
Cue::~Cue() {}

// Mapping:
// comment --> side data (and side data only sample)
// settings --> side data
// start_time --> pts
std::shared_ptr<MediaSample> CueToMediaSample(const Cue& cue) {
  const bool kKeyFrame = true;
  if (!cue.comment.empty()) {
    const std::string comment = base::JoinString(cue.comment, "\n");
    return MediaSample::FromMetadata(
        reinterpret_cast<const uint8_t*>(comment.data()), comment.size());
  }

  const std::string payload = base::JoinString(cue.payload, "\n");
  std::shared_ptr<MediaSample> media_sample = MediaSample::CopyFrom(
      reinterpret_cast<const uint8_t*>(payload.data()), payload.size(),
      reinterpret_cast<const uint8_t*>(cue.settings.data()),
      cue.settings.size(), !kKeyFrame);

  media_sample->set_config_id(cue.identifier);
  media_sample->set_pts(cue.start_time);
  media_sample->set_duration(cue.duration);
  return media_sample;
}

// TODO(rkuroiwa): Cue gets converted to MediaSample in WebVttMediaParser and
// then back to Cue in the muxer. Consider making MediaSample a protobuf or make
// Cue a protobuf and (ab)use MediaSample::data() to store serialized Cue.
Cue MediaSampleToCue(const MediaSample& sample) {
  Cue cue;
  if (sample.data_size() == 0) {
    std::string comment(sample.side_data(),
                        sample.side_data() + sample.side_data_size());
    cue.comment.push_back(comment);
    return cue;
  }

  std::string payload(sample.data(), sample.data() + sample.data_size());
  cue.payload.push_back(payload);
  cue.identifier.assign(sample.config_id());
  cue.start_time = sample.pts();
  cue.duration = sample.duration();
  if (sample.side_data_size() != 0) {
    cue.settings.assign(sample.side_data(),
                        sample.side_data() + sample.side_data_size());
  }
  return cue;
}

}  // namespace media
}  // namespace shaka
