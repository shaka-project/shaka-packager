#include "packager/media/formats/webvtt/cue.h"

#include "packager/base/strings/string_util.h"

namespace shaka {
namespace media {

Cue::Cue() : start_time(0), duration(0) {}
Cue::~Cue() {}

}  // namespace media
}  // namespace shaka
