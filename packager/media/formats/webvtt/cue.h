#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "packager/media/base/media_sample.h"

namespace shaka {
namespace media {

// If comment is not empty, then this is metadata and other fields must
// be empty.
// Data that can be multiline are vector of strings.
struct Cue {
  Cue();
  ~Cue();

  std::string identifier;
  uint64_t start_time;
  uint64_t duration;
  std::string settings;
  std::vector<std::string> payload;
  std::vector<std::string> comment;
};

/// Convert Cue to MediaSample.
/// @param cue data.
/// @return @a cue converted to a MediaSample.
std::shared_ptr<MediaSample> CueToMediaSample(const Cue& cue);

/// Convert MediaSample to Cue.
/// @param sample to be converted.
/// @return @a sample converted to Cue.
Cue MediaSampleToCue(const MediaSample& sample);

}  // namespace media
}  // namespace shaka
