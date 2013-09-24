// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * @fileoverview This file provides the RenderingStats object, used
 * to characterize rendering smoothness.
 */
(function() {
  var getTimeMs = (function() {
    if (window.performance)
      return (performance.now       ||
              performance.mozNow    ||
              performance.msNow     ||
              performance.oNow      ||
              performance.webkitNow).bind(window.performance);
    else
      return function() { return new Date().getTime(); };
  })();

  var requestAnimationFrame = (function() {
    return window.requestAnimationFrame       ||
           window.webkitRequestAnimationFrame ||
           window.mozRequestAnimationFrame    ||
           window.oRequestAnimationFrame      ||
           window.msRequestAnimationFrame     ||
           function(callback) {
             window.setTimeout(callback, 1000 / 60);
           };
  })().bind(window);

  /**
   * Tracks rendering performance using the gpuBenchmarking.renderingStats API.
   * @constructor
   */
  function GpuBenchmarkingRenderingStats() {
  }

  GpuBenchmarkingRenderingStats.prototype.start = function() {
    this.startTime_ = getTimeMs();
    this.initialStats_ = this.getRenderingStats_();
  }

  GpuBenchmarkingRenderingStats.prototype.stop = function() {
    this.stopTime_ = getTimeMs();
    this.finalStats_ = this.getRenderingStats_();
  }

  GpuBenchmarkingRenderingStats.prototype.getStartValues = function() {
    if (!this.initialStats_)
      throw new Error('Start not called.');

    if (!this.finalStats_)
      throw new Error('Stop was not called.');

    return this.initialStats_;
  }

  GpuBenchmarkingRenderingStats.prototype.getEndValues = function() {
    if (!this.initialStats_)
      throw new Error('Start not called.');

    if (!this.finalStats_)
      throw new Error('Stop was not called.');

    return this.finalStats_;
  }

  GpuBenchmarkingRenderingStats.prototype.getDeltas = function() {
    if (!this.initialStats_)
      throw new Error('Start not called.');

    if (!this.finalStats_)
      throw new Error('Stop was not called.');

    var stats = {}
    for (var key in this.finalStats_)
      stats[key] = this.finalStats_[key] - this.initialStats_[key];
    return stats;
  };

  GpuBenchmarkingRenderingStats.prototype.getRenderingStats_ = function() {
    var stats = chrome.gpuBenchmarking.renderingStats();
    stats.totalTimeInSeconds = getTimeMs() / 1000;
    return stats;
  };

  /**
   * Tracks rendering performance using requestAnimationFrame.
   * @constructor
   */
  function RafRenderingStats() {
    this.recording_ = false;
    this.frameTimes_ = [];
  }

  RafRenderingStats.prototype.start = function() {
    if (this.recording_)
      throw new Error('Already started.');
    this.recording_ = true;
    requestAnimationFrame(this.recordFrameTime_.bind(this));
  }

  RafRenderingStats.prototype.stop = function() {
    this.recording_ = false;
  }

  RafRenderingStats.prototype.getStartValues = function() {
    var results = {};
    results.numAnimationFrames = 0;
    results.numFramesSentToScreen = 0;
    results.droppedFrameCount = 0;
    return results;
  }

  RafRenderingStats.prototype.getEndValues = function() {
    var results = {};
    results.numAnimationFrames = this.frameTimes_.length - 1;
    results.numFramesSentToScreen = results.numAnimationFrames;
    results.droppedFrameCount = this.getDroppedFrameCount_(this.frameTimes_);
    return results;
  }

  RafRenderingStats.prototype.getDeltas = function() {
    var endValues = this.getEndValues();
    endValues.totalTimeInSeconds = (
        this.frameTimes_[this.frameTimes_.length - 1] -
        this.frameTimes_[0]) / 1000;
    return endValues;
  };

  RafRenderingStats.prototype.recordFrameTime_ = function(timestamp) {
    if (!this.recording_)
      return;

    this.frameTimes_.push(timestamp);
    requestAnimationFrame(this.recordFrameTime_.bind(this));
  };

  RafRenderingStats.prototype.getDroppedFrameCount_ = function(frameTimes) {
    var droppedFrameCount = 0;
    for (var i = 1; i < frameTimes.length; i++) {
      var frameTime = frameTimes[i] - frameTimes[i-1];
      if (frameTime > 1000 / 55)
        droppedFrameCount++;
    }
    return droppedFrameCount;
  };

  function RenderingStats() {
    if (window.chrome && chrome.gpuBenchmarking &&
        chrome.gpuBenchmarking.renderingStats) {
      return new GpuBenchmarkingRenderingStats();
    }
    return new RafRenderingStats();
  }

  window.__RenderingStats = RenderingStats;
})();
