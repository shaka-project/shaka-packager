// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Playback agent.
 */

var Benchmark = Benchmark || {};

/**
 * Playback agent class.
 * @param {Object} data Test data.
 * @constructor
 */
Benchmark.Agent = function(data) {
  this.timeline = data.timeline;
  this.timelinePosition = 0;
  this.steps = data.steps;
  this.stepsPosition = 0;
  this.randoms = data.randoms;
  this.randomsPosition = 0;
  this.ticks = data.ticks;
  this.ticksPosition = 0;
  this.delayedScriptElements = {};
  this.callStackDepth = 0;
  document.cookie = data.cookie;
  if (window.innerWidth != data.width || window.innerHeight != data.height) {
    Benchmark.die('Wrong window size: ' +
                  window.innerWidth + 'x' + window.innerHeight +
                  ' instead of ' + data.width + 'x' + data.height);
  }
  this.startTime = Benchmark.originals.Date.now();
};

/**
 * Returns current timeline event.
 * @return {Object} Event.
 */
Benchmark.Agent.prototype.getCurrentEvent = function() {
  return this.timeline[this.timelinePosition];
};

/**
 * Returns next recorded event in timeline. If event is the last event in
 * timeline, posts test results to driver.
 * @param {Object} event Event that actually happened, should correspond to
 * the recorded one (used for debug only).
 * @return {Object} Recorded event from timeline.
 */
Benchmark.Agent.prototype.getNextEvent = function(event) {
  var recordedEvent = this.getCurrentEvent();
  this.ensureEqual(event, recordedEvent);
  if (event.type == 'random' || event.type == 'ticks') {
    recordedEvent.count -= 1;
    if (recordedEvent.count == 0) {
      this.timelinePosition += 1;
    }
  } else {
    this.timelinePosition += 1;
  }
  if (this.timelinePosition == this.steps[this.stepsPosition][1]) {
    var score = Benchmark.originals.Date.now() - this.startTime;
    Benchmark.reportScore(score);
  }
  return recordedEvent;
};

/**
 * Checks if two events can be considered equal. Throws exception if events
 * differ.
 * @param {Object} event Event that actually happened.
 * @param {Object} recordedEvent Event taken from timeline.
 */
Benchmark.Agent.prototype.ensureEqual = function(event, recordedEvent) {
  var equal = false;
  if (event.type == recordedEvent.type &&
      event.type in Benchmark.eventPropertiesMap) {
    equal = true;
    var properties = Benchmark.eventPropertiesMap[event.type];
    for (var i = 0; i < properties.length && equal; ++i)
      if (event[properties[i]] != recordedEvent[properties[i]])
        equal = false;
  }
  if (!equal) {
    Benchmark.die('unexpected event: ' + JSON.stringify(event) +
                  ' instead of ' + JSON.stringify(recordedEvent));
  }
};

/**
 * Gets next event from timeline and returns its identifier.
 * @param {Object} event Object with event information.
 * @return {number} Event identifier.
 */
Benchmark.Agent.prototype.createAsyncEvent = function(event) {
  return this.getNextEvent(event).id;
};

/**
 * Stores callback to be invoked according to timeline order.
 * @param {number} eventId 'Parent' event identifier.
 * @param {function} callback Callback.
 */
Benchmark.Agent.prototype.fireAsyncEvent = function(eventId, callback) {
  var event = this.timeline[eventId];
  if (!event.callbackReference) return;
  this.timeline[event.callbackReference].callback = callback;
  this.fireSome();
};

/**
 * Ensures that things are happening according to recorded timeline.
 * @param {number} eventId Identifier of cancelled event.
 */
Benchmark.Agent.prototype.cancelAsyncEvent = function(eventId) {
  this.getNextEvent({type: 'cancel', reference: eventId});
};

/**
 * Checks if script isn't going to be executed too early and delays script
 * execution if necessary.
 * @param {number} scriptId Unique script identifier.
 * @param {HTMLElement} doc Document element.
 * @param {boolean} inlined Indicates whether script is a text block in the page
 * or resides in a separate file.
 * @param {string} src Script url (if script is not inlined).
 */
Benchmark.Agent.prototype.readyToExecuteScript = function(scriptId, doc,
                                                          inlined, src) {
  var event = this.getCurrentEvent();
  if (event.type == 'willExecuteScript' && event.scriptId == scriptId) {
    this.timelinePosition += 1;
    return true;
  }
  var element;
  var elements = doc.getElementsByTagName('script');
  for (var i = 0, el; (el = elements[i]) && !element; ++i) {
    if (inlined) {
      if (el.src) continue;
      var text = el.textContent;
      if (scriptId == text.substring(2, text.indexOf("*/")))
        element = elements[i];
    } else {
      if (!el.src) continue;
      if (el.src.indexOf(src) != -1 || src.indexOf(el.src) != -1) {
        element = el;
      }
    }
  }
  if (!element) {
    Benchmark.die('script element not found', scriptId, src);
  }
  for (var el2 = element; el2; el2 = el2.parentElement) {
    if (el2.onload) {
      console.log('found', el2);
    }
  }
  this.delayedScriptElements[scriptId] = element;
  return false;
};

/**
 * Ensures that things are happening according to recorded timeline.
 * @param {Object} event Object with event information.
 */
Benchmark.Agent.prototype.didExecuteScript = function(scriptId ) {
  this.getNextEvent({type: 'didExecuteScript', scriptId: scriptId});
  this.fireSome();
};

/**
 * Invokes async events' callbacks according to timeline order.
 */
Benchmark.Agent.prototype.fireSome = function() {
  while (this.timelinePosition < this.timeline.length) {
    var event = this.getCurrentEvent();
    if (event.type == 'willFire') {
      if(!event.callback) break;
      this.timelinePosition += 1;
      this.callStackDepth += 1;
      event.callback();
      this.callStackDepth -= 1;
      this.getNextEvent({type: 'didFire', reference: event.reference});
    } else if (event.type == 'willExecuteScript') {
      if (event.scriptId in this.delayedScriptElements) {
        var element = this.delayedScriptElements[event.scriptId];
        var parent = element.parentElement;
        var cloneElement = element.cloneNode();
        delete this.delayedScriptElements[event.scriptId];
        parent.replaceChild(cloneElement, element);
      }
      break;
    } else if (this.callStackDepth > 0) {
      break;
    } else {
      Benchmark.die('unexpected event in fireSome:' + JSON.stringify(event));
    }
  }
};

/**
 * Returns recorded random.
 * @return {number} Recorded random.
 */
Benchmark.Agent.prototype.random = function() {
  this.getNextEvent({type: 'random'});
  return this.randoms[this.randomsPosition++];
};

/**
 * Returns recorded ticks.
 * @return {number} Recorded ticks.
 */
Benchmark.Agent.prototype.dateNow = function(event) {
  this.getNextEvent({type: 'ticks'});
  return this.ticks[this.ticksPosition++];
};

/**
 * Event type -> property list mapping used for matching events.
 * @const
 */
Benchmark.eventPropertiesMap = {
  'timeout': ['timeout'],
  'request': ['url'],
  'addEventListener': ['eventType'],
  'script load': ['src'],
  'willExecuteScript': ['scriptId'],
  'didExecuteScript': ['scriptId'],
  'willFire': ['reference'],
  'didFire': ['reference'],
  'cancel': ['reference'],
  'random': [],
  'ticks': []
};

/**
 * Agent used by native window functions wrappers.
 */
Benchmark.agent = new Benchmark.Agent(Benchmark.data);

/**
 * Playback flag.
 * @const
 */
Benchmark.playback = true;

Benchmark.reportScore = function(score) {
  Benchmark.score = score;
};

Benchmark.originals.addEventListenerToWindow.call(
    window, 'message', function(event) {
      if (Benchmark.score) {
        event.source.postMessage(Benchmark.score, event.origin);
      }
    }, false);
