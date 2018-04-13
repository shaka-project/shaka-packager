// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef MPD_BASE_BANDWIDTH_ESTIMATOR_H_
#define MPD_BASE_BANDWIDTH_ESTIMATOR_H_

#include <stddef.h>
#include <stdint.h>

#include <deque>

namespace shaka {

class BandwidthEstimator {
 public:
  /// @param num_blocks is the number of latest blocks to use. 0 uses all.
  static constexpr size_t kUseAllBlocks = 0;
  explicit BandwidthEstimator(size_t num_blocks);
  ~BandwidthEstimator();

  /// @param size is the size of the block in bytes. Should be positive.
  /// @param duration is the length in seconds. Should be positive.
  void AddBlock(uint64_t size, double duration);

  /// @return The estimate bandwidth, in bits per second, from the harmonic mean
  ///         of the number of blocks specified in the constructor. The value is
  ///         rounded up to the nearest integer.
  uint64_t Estimate() const;

  /// @return The max bandwidth, in bits per second, of the number of blocks
  ///         specified in the constructor. The value is rounded up to the
  ///         nearest integer.
  uint64_t Max() const;

 private:
  BandwidthEstimator(const BandwidthEstimator&) = delete;
  BandwidthEstimator& operator=(const BandwidthEstimator&) = delete;

  // A sliding queue that provide convenient functions to get the minimum value
  // and the sum when window slides.
  class SlidingQueue {
   public:
    // |window_size| defines the size of the sliding window. 0 uses all.
    explicit SlidingQueue(size_t window_size);

    // Add a new value. Old values may be moved out.
    void Add(double value);

    // Return the sum of the values in the sliding window.
    double sum() const { return sum_; }
    // Return the number of values in the sliding window.
    double size() const { return size_; }
    // Return the minimum value of the values in the sliding window.
    double min() const { return min_.front(); }

   private:
    SlidingQueue(const SlidingQueue&) = delete;
    SlidingQueue& operator=(const SlidingQueue&) = delete;

    const size_t window_size_;
    size_t size_ = 0;
    double sum_ = 0;
    // Keeps track of the values in the sliding window. Not needed if
    // |window_size| is kUseAllBlocks.
    std::deque<double> window_;
    // Keeps track of a monotonic non-decreasing sequence of values, i.e.
    // local minimum values in the sliding window. The front() is always the
    // global minimum, i.e. the minimum value in the sliding window.
    // This is achieved through:
    //   (1) New value is added to the back with the original values before it
    //   that are larger removed as they are no longer useful.
    //   (2) When a value is removed from |window_|, if it is the minimum value,
    //   it is also removed from |min_|; if it is not, it means the value is not
    //   present in |min_|.
    std::deque<double> min_;
  };
  SlidingQueue sliding_queue_;
};

}  // namespace shaka

#endif  // MPD_BASE_BANDWIDTH_ESTIMATOR_H_
