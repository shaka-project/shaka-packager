#include "packager/utils/clock.h"

namespace shaka {

Clock::time_point Clock::now() noexcept {
  return std::chrono::system_clock::now();
}

}  // namespace shaka
