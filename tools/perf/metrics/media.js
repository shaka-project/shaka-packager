// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains common utilities to find video/audio elements on a page
// and collect metrics for each.

(function() {
  // MediaMetric class responsible for collecting metrics on a media element.
  // It attaches required event listeners in order to collect different metrics.
  function MediaMetricBase(element) {
    checkElementIsNotBound(element);
    this.metrics = {};
    this.id = '';
    this.element = element;
  }

  MediaMetricBase.prototype.getMetrics = function() {
    return this.metrics;
  };

  MediaMetricBase.prototype.getSummary = function() {
    return {
      'id': this.id,
      'metrics': this.getMetrics()
    };
  };

  function HTMLMediaMetric(element) {
    MediaMetricBase.prototype.constructor.call(this, element);
    // Set the basic event handlers for HTML5 media element.
    var metric = this;
    function onVideoLoad(event) {
      // If a 'Play' action is performed, then playback_timer != undefined.
      if (metric.playbackTimer == undefined)
        metric.playbackTimer = new Timer();
    }
    // For the cases where autoplay=true, and without a 'play' action, we want
    // to start playbackTimer at 'play' or 'loadedmetadata' events.
    this.element.addEventListener('play', onVideoLoad);
    this.element.addEventListener('loadedmetadata', onVideoLoad);
    this.element.addEventListener('playing', function(e) {
        metric.onPlaying(e);
      });
    this.element.addEventListener('ended', function(e) {
        metric.onEnded(e);
      });
    this.setID();

    // Listen to when a Telemetry actions gets called.
    this.element.addEventListener('willPlay', function (e) {
        metric.onWillPlay(e);
      }, false);
    this.element.addEventListener('willSeek', function (e) {
        metric.onWillSeek(e);
      }, false);
  }

  HTMLMediaMetric.prototype = new MediaMetricBase();
  HTMLMediaMetric.prototype.constructor = HTMLMediaMetric;

  HTMLMediaMetric.prototype.setID = function() {
    if (this.element.src)
      this.id = this.element.src.substring(this.element.src.lastIndexOf("/")+1);
    else if (this.element.id)
      this.id = this.element.id;
    else
      this.id = 'media_' + window.__globalCounter++;
  };

  HTMLMediaMetric.prototype.onWillPlay = function(e) {
    this.playbackTimer = new Timer();
  };

  HTMLMediaMetric.prototype.onWillSeek = function(e) {
    var seekLabel = '';
    if (e.seekLabel)
      seekLabel = '_' + e.seekLabel;
    var metric = this;
    var onSeeked = function(e) {
        metric.appendMetric('seek' + seekLabel, metric.seekTimer.stop())
        e.target.removeEventListener('seeked', onSeeked);
      };
    this.seekTimer = new Timer();
    this.element.addEventListener('seeked', onSeeked);
  };

  HTMLMediaMetric.prototype.appendMetric = function(metric, value) {
    if (!this.metrics[metric])
      this.metrics[metric] = [];
    this.metrics[metric].push(value);
  }

  HTMLMediaMetric.prototype.onPlaying = function(event) {
    // Playing event can fire more than once if seeking.
    if (!this.metrics['time_to_play'])
      this.metrics['time_to_play'] = this.playbackTimer.stop();
  };

  HTMLMediaMetric.prototype.onEnded = function(event) {
    this.metrics['playback_time'] = this.playbackTimer.stop();
  };

  HTMLMediaMetric.prototype.getMetrics = function() {
    this.metrics['decoded_frame_count'] = this.element.webkitDecodedFrameCount;
    this.metrics['dropped_frame_count'] = this.element.webkitDroppedFrameCount;
    this.metrics['decoded_video_bytes'] =
        this.element.webkitVideoDecodedByteCount;
    this.metrics['decoded_audio_bytes'] =
        this.element.webkitAudioDecodedByteCount;
    return this.metrics;
  };

  function MediaMetric(element) {
    if (element instanceof HTMLMediaElement)
      return new HTMLMediaMetric(element);
    throw new Error('Unrecognized media element type.');
  }

  function Timer() {
    this.start_ = 0;
    this.start();
  }

  Timer.prototype = {
    start: function() {
      this.start_ = getCurrentTime();
    },

    stop: function() {
      // Return delta time since start in secs.
      return ((getCurrentTime() - this.start_) / 1000).toFixed(3);
    }
  };

  function checkElementIsNotBound(element) {
    if (!element)
      return;
    for (var i = 0; i < window.__mediaMetrics.length; i++) {
      if (window.__mediaMetrics[i].element == element)
        throw new Error('Can not create MediaMetric for same element twice.');
    }
  }

  function createMediaMetricsForDocument() {
    // Searches for all video and audio elements on the page and creates a
    // corresponding media metric instance for each.
    var mediaElements = document.querySelectorAll('video, audio');
    for (var i = 0; i < mediaElements.length; i++)
      window.__mediaMetrics.push(new MediaMetric(mediaElements[i]));
  }

  function getCurrentTime() {
    if (window.performance)
      return (performance.now ||
              performance.mozNow ||
              performance.msNow ||
              performance.oNow ||
              performance.webkitNow).call(window.performance);
    else
      return Date.now();
  }

  function getAllMetrics() {
    // Returns a summary (info + metrics) for all media metrics.
    var metrics = [];
    for (var i = 0; i < window.__mediaMetrics.length; i++)
      metrics.push(window.__mediaMetrics[i].getSummary());
    return metrics;
  }

  window.__globalCounter = 0;
  window.__mediaMetrics = [];
  window.__getAllMetrics = getAllMetrics;
  window.__createMediaMetricsForDocument = createMediaMetricsForDocument;
})();
