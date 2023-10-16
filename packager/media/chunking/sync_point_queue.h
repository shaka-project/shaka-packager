// Copyright 2018 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <map>
#include <memory>

#include <absl/synchronization/mutex.h>

#include <packager/ad_cue_generator_params.h>

namespace shaka {
namespace media {

struct CueEvent;

/// A synchronized queue for cue points.
class SyncPointQueue {
 public:
  explicit SyncPointQueue(const AdCueGeneratorParams& params);
  ~SyncPointQueue() = default;

  /// Add a new thread. Each thread using this instance must call this method in
  /// order to keep track of its clients.
  void AddThread();

  /// Cancel the queue and unblock all threads.
  void Cancel();

  /// @return A hint for when the next cue event would be. The returned hint is
  ///         not less than @a time_in_seconds. The actual time for the next cue
  ///         event will not be less than the returned hint, with the exact
  ///         value depends on promotion.
  double GetHint(double time_in_seconds);

  /// @return The next cue based on a previous hint. If a cue has been promoted
  ///         that comes after @a hint_in_seconds it is returned. If no cue
  ///         after @a hint_in_seconds has been promoted, this will block until
  ///         either a cue is promoted or all threads are blocked (in which
  ///         case, the unpromoted cue at @a hint_in_seconds will be
  ///         self-promoted and returned) or Cancel() is called.
  std::shared_ptr<const CueEvent> GetNext(double hint_in_seconds);

  /// Promote the first cue that is not greater than @a time_in_seconds. All
  /// unpromoted cues before the cue will be discarded.
  std::shared_ptr<const CueEvent> PromoteAt(double time_in_seconds);

  /// @return True if there are more cues after the given hint. The hint must
  ///         be a hint returned from |GetHint|. Using any other value results
  ///         in undefined behavior.
  bool HasMore(double hint_in_seconds) const;

 private:
  SyncPointQueue(const SyncPointQueue&) = delete;
  SyncPointQueue& operator=(const SyncPointQueue&) = delete;

  // PromoteAt() without locking. It is called by PromoteAt() and other
  // functions that have locks.
  std::shared_ptr<const CueEvent> PromoteAtNoLocking(double time_in_seconds);

  absl::Mutex mutex_;
  absl::CondVar sync_condition_ ABSL_GUARDED_BY(mutex_);
  size_t thread_count_ = 0;
  size_t waiting_thread_count_ = 0;
  bool cancelled_ = false;

  std::map<double, std::shared_ptr<CueEvent>> unpromoted_;
  std::map<double, std::shared_ptr<CueEvent>> promoted_;
};

}  // namespace media
}  // namespace shaka
