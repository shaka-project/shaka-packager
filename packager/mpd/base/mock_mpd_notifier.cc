#include "packager/mpd/base/mock_mpd_notifier.h"

namespace shaka {

MockMpdNotifier::MockMpdNotifier(const MpdOptions& mpd_options)
    : MpdNotifier(mpd_options) {}
MockMpdNotifier::~MockMpdNotifier() {}

}  // namespace shaka
