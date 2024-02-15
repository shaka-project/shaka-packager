// Copyright 2018 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/media/chunking/sync_point_queue.h>

#include <algorithm>
#include <limits>

#include <absl/log/check.h>

#include <packager/media/base/media_handler.h>

namespace shaka {
namespace media {

SyncPointQueue::SyncPointQueue(const AdCueGeneratorParams& params) {
  for (const Cuepoint& point : params.cue_points) {
    std::shared_ptr<CueEvent> event = std::make_shared<CueEvent>();
    event->time_in_seconds = point.start_time_in_seconds;
    unpromoted_[point.start_time_in_seconds] = std::move(event);
  }
}

void SyncPointQueue::AddThread() {
  absl::MutexLock lock(&mutex_);
  thread_count_++;
}

void SyncPointQueue::Cancel() {
  {
    absl::MutexLock lock(&mutex_);
    cancelled_ = true;
  }
  sync_condition_.SignalAll();
}

double SyncPointQueue::GetHint(double time_in_seconds) {
  absl::MutexLock lock(&mutex_);

  auto iter = promoted_.upper_bound(time_in_seconds);
  if (iter != promoted_.end())
    return iter->first;

  iter = unpromoted_.upper_bound(time_in_seconds);
  if (iter != unpromoted_.end())
    return iter->first;

  // Use MAX DOUBLE as the fall back so that we can force all streams to run
  // out all their samples even when there are no cues.
  return std::numeric_limits<double>::max();
}

std::shared_ptr<const CueEvent> SyncPointQueue::GetNext(
    double hint_in_seconds) {
  absl::MutexLock lock(&mutex_);
  while (!cancelled_) {
    // Find the promoted cue that would line up with our hint, which is the
    // first cue that is not less than |hint_in_seconds|.
    auto iter = promoted_.lower_bound(hint_in_seconds);
    if (iter != promoted_.end()) {
      return iter->second;
    }

    // Promote |hint_in_seconds| if everyone is waiting.
    if (waiting_thread_count_ + 1 == thread_count_) {
      std::shared_ptr<const CueEvent> cue = PromoteAtNoLocking(hint_in_seconds);
      CHECK(cue);
      return cue;
    }

    waiting_thread_count_++;
    // This blocks until either a cue is promoted or all threads are blocked
    // (in which case, the unpromoted cue at the hint will be self-promoted
    // and returned - see section above). Spurious signal events are possible
    // with most condition variable implementations, so if it returns, we go
    // back and check if a cue is actually promoted or not.
    sync_condition_.Wait(&mutex_);
    waiting_thread_count_--;
  }
  return nullptr;
}

std::shared_ptr<const CueEvent> SyncPointQueue::PromoteAt(
    double time_in_seconds) {
  absl::MutexLock lock(&mutex_);
  return PromoteAtNoLocking(time_in_seconds);
}

bool SyncPointQueue::HasMore(double hint_in_seconds) const {
  return hint_in_seconds < std::numeric_limits<double>::max();
}

std::shared_ptr<const CueEvent> SyncPointQueue::PromoteAtNoLocking(
    double time_in_seconds) {
  mutex_.AssertHeld();

  // It is possible that |time_in_seconds| has been promoted.
  auto iter = promoted_.find(time_in_seconds);
  if (iter != promoted_.end())
    return iter->second;

  // Find the unpromoted cue that would work for the given time, which is the
  // first cue that is not greater than |time_in_seconds|.
  // So find the the first cue that is greater than |time_in_seconds| first and
  // then get the previous one.
  iter = unpromoted_.upper_bound(time_in_seconds);
  // The first cue in |unpromoted_| should not be greater than
  // |time_in_seconds|. It could happen only if it has been promoted at a
  // different timestamp, which can only be the result of unaligned GOPs.
  if (iter == unpromoted_.begin())
    return nullptr;
  auto prev_iter = std::prev(iter);
  DCHECK(prev_iter != unpromoted_.end());

  std::shared_ptr<CueEvent> cue = prev_iter->second;
  cue->time_in_seconds = time_in_seconds;

  promoted_[time_in_seconds] = cue;
  // Remove all unpromoted cues up to the cue that was just promoted.
  // User may provide multiple cue points at the same or similar timestamps. The
  // extra unused cues are simply ignored.
  unpromoted_.erase(unpromoted_.begin(), iter);

  // Wake up other threads that may be waiting.
  sync_condition_.SignalAll();
  return cue;
}

}  // namespace media
}  // namespace shaka
