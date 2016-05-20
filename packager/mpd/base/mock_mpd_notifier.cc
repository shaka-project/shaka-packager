#include "packager/mpd/base/mock_mpd_notifier.h"

namespace shaka {

MockMpdNotifier::MockMpdNotifier(DashProfile profile) : MpdNotifier(profile) {}
MockMpdNotifier::~MockMpdNotifier() {}

}  // namespace shaka
