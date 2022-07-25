// Copyright 2022 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_BASE_PRODUCER_CONSUMER_QUEUE_H_
#define PACKAGER_MEDIA_BASE_PRODUCER_CONSUMER_QUEUE_H_

#include <deque>

#include "packager/base/strings/stringprintf.h"
#include "packager/base/synchronization/condition_variable.h"
#include "packager/base/synchronization/lock.h"
#include "packager/base/timer/elapsed_timer.h"
#include "packager/status.h"

namespace shaka {
namespace media {

static const size_t kUnlimitedCapacity = 0u;
static const int64_t kInfiniteTimeout = -1;

/// A thread safe producer consumer queue implementation. It allows the standard
/// push and pop operations. It also maintains a monotonically-increasing
/// element position and allows peeking at the element at certain position.
template <class T>
class ProducerConsumerQueue {
 public:
  /// Create a ProducerConsumerQueue starting from position 0.
  /// @param capacity is the maximum number of elements that the queue can hold
  ///        at once. A value of zero means unlimited capacity.
  explicit ProducerConsumerQueue(size_t capacity);

  /// Create a ProducerConsumerQueue starting from indicated position.
  /// @param capacity is the maximum number of elements that the queue can hold
  ///        at once. A value of zero means unlimited capacity.
  /// @param starting_pos is the starting head position.
  ProducerConsumerQueue(size_t capacity, size_t starting_pos);

  ~ProducerConsumerQueue();

  /// Push an element to the back of the queue. If the queue has reached its
  /// capacity limit, block until spare capacity is available or time out or
  /// stopped.
  /// @param element refers the element to be pushed.
  /// @param timeout_ms indicates timeout in milliseconds. A value of zero means
  ///        return immediately. A negative value means waiting indefinitely.
  /// @return OK if the element was pushed successfully, STOPPED if Stop has
  ///         has been called, TIME_OUT if times out.
  Status Push(const T& element, int64_t timeout_ms);

  /// Pop an element from the front of the queue. If the queue is empty, block
  /// for an element to be available to be consumed or time out or stopped.
  /// @param[out] element receives the popped element.
  /// @param timeout_ms indicates timeout in milliseconds. A value of zero means
  ///        return immediately. A negative value means waiting indefinitely.
  /// @return STOPPED if Stop has been called and the queue is completely empty,
  ///         TIME_OUT if times out, OK otherwise.
  Status Pop(T* element, int64_t timeout_ms);

  /// Peek at the element at the specified position from the queue. If the
  /// element is not available yet, block until it to be available or time out
  /// or stopped.
  /// NOTE: Elements before (pos - capacity/2) will be removed from the queue
  /// after Peek operation.
  /// @param pos refers to the element position.
  /// @param[out] element receives the peeked element.
  /// @param timeout_ms indicates timeout in milliseconds. A value of zero means
  ///        return immediately. A negative value means waiting indefinitely.
  /// @return STOPPED if Stop has been called and @a pos is out of range,
  ///         INVALID_ARGUMENT if the pos < Head(), TIME_OUT if times out,
  ///         OK otherwise.
  Status Peek(size_t pos, T* element, int64_t timeout_ms);

  /// Terminate Pop and Peek requests once the queue drains entirely.
  /// Also terminate all waiting and future Push requests immediately.
  /// Stop cannot stall.
  void Stop() {
    base::AutoLock l(lock_);
    stop_requested_ = true;
    not_empty_cv_.Broadcast();
    not_full_cv_.Broadcast();
    new_element_cv_.Broadcast();
  }

  /// @return true if there are no elements in the queue.
  bool Empty() const {
    base::AutoLock l(lock_);
    return q_.empty();
  }

  /// @return The number of elements in the queue.
  size_t Size() const {
    base::AutoLock l(lock_);
    return q_.size();
  }

  /// @return The position of the head element in the queue. Note that the
  ///         returned value may be meaningless if the queue is empty.
  size_t HeadPos() const {
    base::AutoLock l(lock_);
    return head_pos_;
  }

  /// @return The position of the tail element in the queue. Note that the
  ///         returned value may be meaningless if the queue is empty.
  size_t TailPos() const {
    base::AutoLock l(lock_);
    return head_pos_ + q_.size() - 1;
  }

  /// @return true if the queue has been stopped using Stop(). This allows
  ///         producers to check if they can add new elements to the queue.
  bool Stopped() const {
    base::AutoLock l(lock_);
    return stop_requested_;
  }

 private:
  // Move head_pos_ to center on pos.
  void SlideHeadOnCenter(size_t pos);

  const size_t capacity_;  // Maximum number of elements; zero means unlimited.
  mutable base::Lock lock_;  // Lock protecting all other variables below.
  size_t head_pos_;          // Head position.
  std::deque<T> q_;          // Internal queue holding the elements.
  base::ConditionVariable not_empty_cv_;
  base::ConditionVariable not_full_cv_;
  base::ConditionVariable new_element_cv_;
  bool stop_requested_;  // True after Stop has been called.

  DISALLOW_COPY_AND_ASSIGN(ProducerConsumerQueue);
};

// Implementations of non-inline functions.
template <class T>
ProducerConsumerQueue<T>::ProducerConsumerQueue(size_t capacity)
    : capacity_(capacity),
      head_pos_(0),
      not_empty_cv_(&lock_),
      not_full_cv_(&lock_),
      new_element_cv_(&lock_),
      stop_requested_(false) {}

template <class T>
ProducerConsumerQueue<T>::ProducerConsumerQueue(size_t capacity,
                                                size_t starting_pos)
    : capacity_(capacity),
      head_pos_(starting_pos),
      not_empty_cv_(&lock_),
      not_full_cv_(&lock_),
      new_element_cv_(&lock_),
      stop_requested_(false) {
}

template <class T>
ProducerConsumerQueue<T>::~ProducerConsumerQueue() {}

template <class T>
Status ProducerConsumerQueue<T>::Push(const T& element, int64_t timeout_ms) {
  base::AutoLock l(lock_);
  bool woken = false;

  // Check for queue shutdown.
  if (stop_requested_)
    return Status(error::STOPPED, "");

  base::ElapsedTimer timer;
  base::TimeDelta timeout_delta = base::TimeDelta::FromMilliseconds(timeout_ms);

  if (capacity_) {
    while (q_.size() == capacity_) {
      if (timeout_ms < 0) {
        // Wait forever, or until Stop.
        not_full_cv_.Wait();
      } else {
        base::TimeDelta elapsed = timer.Elapsed();
        if (elapsed < timeout_delta) {
          // Wait with timeout, or until Stop.
          not_full_cv_.TimedWait(timeout_delta - elapsed);
        } else {
          // We're through waiting.
          return Status(error::TIME_OUT, "Time out on pushing.");
        }
      }
      // Re-check for queue shutdown after waking from Wait.
      if (stop_requested_)
        return Status(error::STOPPED, "");

      woken = true;
    }
    DCHECK_LT(q_.size(), capacity_);
  }

  // Signal consumer to proceed if we are going to create some elements.
  if (q_.empty())
    not_empty_cv_.Signal();
  new_element_cv_.Signal();

  q_.push_back(element);

  // Signal other producers if we just acquired more capacity.
  if (woken && q_.size() != capacity_)
    not_full_cv_.Signal();
  return Status::OK;
}

template <class T>
Status ProducerConsumerQueue<T>::Pop(T* element, int64_t timeout_ms) {
  base::AutoLock l(lock_);
  bool woken = false;

  base::ElapsedTimer timer;
  base::TimeDelta timeout_delta = base::TimeDelta::FromMilliseconds(timeout_ms);

  while (q_.empty()) {
    if (stop_requested_)
      return Status(error::STOPPED, "");

    if (timeout_ms < 0) {
      // Wait forever, or until Stop.
      not_empty_cv_.Wait();
    } else {
      base::TimeDelta elapsed = timer.Elapsed();
      if (elapsed < timeout_delta) {
        // Wait with timeout, or until Stop.
        not_empty_cv_.TimedWait(timeout_delta - elapsed);
      } else {
        // We're through waiting.
        return Status(error::TIME_OUT, "Time out on popping.");
      }
    }
    woken = true;
  }

  // Signal producer to proceed if we are going to create some capacity.
  if (q_.size() == capacity_)
    not_full_cv_.Signal();

  *element = q_.front();
  q_.pop_front();
  ++head_pos_;

  // Signal other consumers if we have more elements.
  if (woken && !q_.empty())
    not_empty_cv_.Signal();
  return Status::OK;
}

template <class T>
Status ProducerConsumerQueue<T>::Peek(size_t pos,
                                      T* element,
                                      int64_t timeout_ms) {
  base::AutoLock l(lock_);
  if (pos < head_pos_) {
    return Status(
        error::INVALID_ARGUMENT,
        base::StringPrintf(
            "pos (%zu) is too small; head is at %zu.", pos, head_pos_));
  }

  bool woken = false;

  base::ElapsedTimer timer;
  base::TimeDelta timeout_delta = base::TimeDelta::FromMilliseconds(timeout_ms);

  // Move head to create some space (move the sliding window centered @ pos).
  SlideHeadOnCenter(pos);

  while (pos >= head_pos_ + q_.size()) {
    if (stop_requested_)
      return Status(error::STOPPED, "");

    if (timeout_ms < 0) {
      // Wait forever, or until Stop.
      new_element_cv_.Wait();
    } else {
      base::TimeDelta elapsed = timer.Elapsed();
      if (elapsed < timeout_delta) {
        // Wait with timeout, or until Stop.
        new_element_cv_.TimedWait(timeout_delta - elapsed);
      } else {
        // We're through waiting.
        return Status(error::TIME_OUT, "Time out on peeking.");
      }
    }
    // Move head to create some space (move the sliding window centered @ pos).
    SlideHeadOnCenter(pos);
    woken = true;
  }

  *element = q_[pos - head_pos_];

  // Signal other consumers if we have more elements.
  if (woken && !q_.empty())
    new_element_cv_.Signal();
  return Status::OK;
}

template <class T>
void ProducerConsumerQueue<T>::SlideHeadOnCenter(size_t pos) {
  lock_.AssertAcquired();

  if (capacity_) {
    // Signal producer to proceed if we are going to create some capacity.
    if (q_.size() == capacity_ && pos > head_pos_ + capacity_ / 2)
      not_full_cv_.Signal();

    while (!q_.empty() && pos > head_pos_ + capacity_ / 2) {
      ++head_pos_;
      q_.pop_front();
    }
  }
}

}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_BASE_PRODUCER_CONSUMER_QUEUE_H_
