#include "packager/mpd/base/mock_mpd_notifier.h"

namespace edash_packager {

MockMpdNotifier::MockMpdNotifier(DashProfile profile) : MpdNotifier(profile) {}
MockMpdNotifier::~MockMpdNotifier() {}

}  // namespace edash_packager
