// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/mpd/base/bandwidth_estimator.h"

#include <cmath>

#include "packager/base/logging.h"

namespace shaka {

BandwidthEstimator::BandwidthEstimator(size_t num_blocks)
    : sliding_queue_(num_blocks) {}
BandwidthEstimator::~BandwidthEstimator() {}

void BandwidthEstimator::AddBlock(uint64_t size, double duration) {
  DCHECK_GT(duration, 0.0);
  DCHECK_GT(size, 0u);

  const int kBitsInByte = 8;
  const double bits_per_second_reciprocal = duration / (kBitsInByte * size);
  sliding_queue_.Add(bits_per_second_reciprocal);
}

uint64_t BandwidthEstimator::Estimate() const {
  return sliding_queue_.size() == 0
             ? 0
             : static_cast<uint64_t>(
                   ceil(sliding_queue_.size() / sliding_queue_.sum()));
}

uint64_t BandwidthEstimator::Max() const {
  // The first element has minimum "bits per second reciprocal", thus the
  // reverse is maximum "bits per second".
  return sliding_queue_.size() == 0
             ? 0
             : static_cast<uint64_t>(ceil(1 / sliding_queue_.min()));
}

BandwidthEstimator::SlidingQueue::SlidingQueue(size_t window_size)
    : window_size_(window_size) {}

void BandwidthEstimator::SlidingQueue::Add(double value) {
  // Remove elements if needed to form a monotonic non-decreasing sequence.
  while (!min_.empty() && min_.back() > value)
    min_.pop_back();
  min_.push_back(value);

  if (window_size_ == kUseAllBlocks) {
    size_++;
    sum_ += value;
    min_.resize(1);  // Keep only the minimum one.
    return;
  }

  window_.push_back(value);
  sum_ += value;

  if (window_.size() <= window_size_) {
    size_++;
    return;
  }

  if (min_.front() == window_.front())
    min_.pop_front();

  sum_ -= window_.front();
  window_.pop_front();
}

}  // namespace shaka
