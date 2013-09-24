// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file provides the ScrollAction object, which scrolls a page
// from top to bottom:
//   1. var action = new __ScrollAction(callback)
//   2. action.start(scroll_options)
'use strict';

(function() {
  var MAX_SCROLL_LENGTH_PIXELS = 5000;

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

  function ScrollGestureOptions(opt_options) {
    if (opt_options) {
      this.element_ = opt_options.element;
      this.left_start_percentage_ = opt_options.left_start_percentage;
      this.top_start_percentage_ = opt_options.top_start_percentage;
    } else {
      this.element_ = document.body;
      this.left_start_percentage_ = 0.5;
      this.top_start_percentage_ = 0.5;
    }
  }

  /**
   * Scrolls a given element down a certain amount to emulate user scroll.
   * Uses smooth scroll capabilities provided by the platform, if available.
   * @constructor
   */
  function SmoothScrollDownGesture(options) {
    this.options_ = options;
  };

  function min(a, b) {
    if (a > b) {
      return b;
    }
    return a;
  };

  function getBoundingVisibleRect(el) {
    var bound = el.getBoundingClientRect();
    var rect = { top: bound.top,
                 left: bound.left,
                 width: bound.width,
                 height: bound.height };
    var outsideHeight = (rect.top + rect.height) - window.innerHeight;
    var outsideWidth = (rect.left + rect.width) - window.innerWidth;

    if (outsideHeight > 0) {
      rect.height -= outsideHeight;
    }
    if (outsideWidth > 0) {
      rect.width -= outsideWidth;
    }
    return rect;
  };

  SmoothScrollDownGesture.prototype.start = function(distance, callback) {
    this.callback_ = callback;
    if (window.chrome &&
        chrome.gpuBenchmarking &&
        chrome.gpuBenchmarking.smoothScrollBy) {
      var rect = getBoundingVisibleRect(this.options_.element_);
      var start_left =
          rect.left + rect.width * this.options_.left_start_percentage_;
      var start_top =
          rect.top + rect.height * this.options_.top_start_percentage_;
      chrome.gpuBenchmarking.smoothScrollBy(distance, function() {
        callback();
      }, start_left, start_top);
      return;
    }

    var SCROLL_DELTA = 100;
    this.options_.element_.scrollTop += SCROLL_DELTA;
    requestAnimationFrame(callback);
  };

  // This class scrolls a page from the top to the bottom once.
  //
  // The page is scrolled down by a set of scroll gestures. These gestures
  // correspond to a reading gesture on that platform.
  //
  // start -> startPass_ -> ...scroll... -> onGestureComplete_ ->
  //       -> startPass_ -> .. scroll... -> onGestureComplete_ -> callback_
  function ScrollAction(opt_callback, opt_remaining_distance_func) {
    var self = this;

    this.beginMeasuringHook = function() {}
    this.endMeasuringHook = function() {}

    this.callback_ = opt_callback;
    this.remaining_distance_func_ = opt_remaining_distance_func;
  }

  ScrollAction.prototype.getRemainingScrollDistance_ = function() {
    if (this.remaining_distance_func_)
      return this.remaining_distance_func_();

    var clientHeight;
    // clientHeight is "special" for the body element.
    if (this.element_ == document.body)
      clientHeight = window.innerHeight;
    else
      clientHeight = this.element_.clientHeight;

    return this.scrollHeight_ - this.element_.scrollTop - clientHeight;
  }

  ScrollAction.prototype.start = function(opt_options) {
    this.options_ = new ScrollGestureOptions(opt_options);
    // Assign this.element_ here instead of constructor, because the constructor
    // ensures this method will be called after the document is loaded.
    this.element_ = this.options_.element_;
    // Some pages load more content when you scroll to the bottom. Record
    // the original element height here and only scroll to that point.
    // -1 to allow for rounding errors on scaled viewports (like mobile).
    this.scrollHeight_ = Math.min(MAX_SCROLL_LENGTH_PIXELS,
                                  this.element_.scrollHeight - 1);
    requestAnimationFrame(this.startPass_.bind(this));
  };

  ScrollAction.prototype.startPass_ = function() {
    this.element_.scrollTop = 0;

    this.beginMeasuringHook();

    this.gesture_ = new SmoothScrollDownGesture(this.options_);
    this.gesture_.start(this.getRemainingScrollDistance_(),
                        this.onGestureComplete_.bind(this));
  };

  ScrollAction.prototype.getResults = function() {
    return this.renderingStats_;
  }

  ScrollAction.prototype.onGestureComplete_ = function() {
    // If the scrollHeight went down, only scroll to the new scrollHeight.
    // -1 to allow for rounding errors on scaled viewports (like mobile).
    this.scrollHeight_ = Math.min(this.scrollHeight_,
                                  this.element_.scrollHeight - 1);

    if (this.getRemainingScrollDistance_() > 0) {
      this.gesture_.start(this.getRemainingScrollDistance_(),
                          this.onGestureComplete_.bind(this));
      return;
    }

    this.endMeasuringHook();

    // We're done.
    if (this.callback_)
      this.callback_();
  };

  window.__ScrollAction = ScrollAction;
  window.__ScrollAction_GetBoundingVisibleRect = getBoundingVisibleRect;
})();
