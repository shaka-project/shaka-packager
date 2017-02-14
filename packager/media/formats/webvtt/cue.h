#ifndef PACKAGER_MEDIA_FORMATS_WEBVTT_CUE_H_
#define PACKAGER_MEDIA_FORMATS_WEBVTT_CUE_H_

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

  // |payload| and |comment| may have trailing "\n" character.
  std::string payload;
  std::string comment;
};

}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_FORMATS_WEBVTT_CUE_H_
