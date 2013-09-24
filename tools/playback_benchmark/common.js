// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Classes and functions used during recording and playback.
 */

var Benchmark = Benchmark || {};

Benchmark.functionList = [
  ['setTimeout', 'setTimeout'],
  ['clearTimeout', 'clearTimeout'],
  ['setInterval', 'setInterval'],
  ['clearInterval', 'clearInterval'],
  ['XMLHttpRequest', 'XMLHttpRequest'],
  ['addEventListenerToWindow', 'addEventListener'],
  ['addEventListenerToNode', 'addEventListener', ['Node', 'prototype']],
  ['removeEventListenerFromNode', 'removeEventListener', ['Node', 'prototype']],
  ['addEventListenerToXHR', 'addEventListener',
   ['XMLHttpRequest', 'prototype']],
  ['random', 'random', ['Math']],
  ['Date', 'Date'],
  ['documentWriteln', 'writeln', ['document']],
  ['documentWrite', 'write', ['document']]
];

Benchmark.timeoutMapping = [];

Benchmark.ignoredListeners = ['mousemove', 'mouseover', 'mouseout'];

Benchmark.originals = {};

Benchmark.overrides = {
  setTimeout: function(callback, timeout) {
    var event = {type: 'timeout', timeout: timeout};
    var eventId = Benchmark.agent.createAsyncEvent(event);
    var timerId = Benchmark.originals.setTimeout.call(this, function() {
      Benchmark.agent.fireAsyncEvent(eventId, callback);
    }, Benchmark.playback ? 0 : timeout);
    Benchmark.timeoutMapping[timerId] = eventId;
    return timerId;
  },

  clearTimeout: function(timerId) {
    var eventId = Benchmark.timeoutMapping[timerId];
    if (eventId == undefined) return;
    Benchmark.agent.cancelAsyncEvent(eventId);
    Benchmark.originals.clearTimeout.call(this, timerId);
  },

  setInterval: function(callback, timeout) {
    console.warn('setInterval');
  },

  clearInterval: function(timerId) {
    console.warn('clearInterval');
  },

  XMLHttpRequest: function() {
    return new Benchmark.XMLHttpRequestWrapper();
  },

  addEventListener: function(type, listener, useCapture, target, targetType,
                             originalFunction) {
    var event = {type: 'addEventListener', target: targetType, eventType: type};
    var eventId = Benchmark.agent.createAsyncEvent(event);
    listener.eventId = eventId;
    listener.wrapper = function(e) {
      Benchmark.agent.fireAsyncEvent(eventId, function() {
        listener.call(target, e);
      });
    };
    originalFunction.call(target, type, listener.wrapper, useCapture);
  },

  addEventListenerToWindow: function(type, listener, useCapture) {
    if (Benchmark.ignoredListeners.indexOf(type) != -1) return;
    Benchmark.overrides.addEventListener(
        type, listener, useCapture, this, 'window',
        Benchmark.originals.addEventListenerToWindow);
  },

  addEventListenerToNode: function(type, listener, useCapture) {
    if (Benchmark.ignoredListeners.indexOf(type) != -1) return;
    Benchmark.overrides.addEventListener(
        type, listener, useCapture, this, 'node',
        Benchmark.originals.addEventListenerToNode);
  },

  addEventListenerToXHR: function(type, listener, useCapture) {
    Benchmark.overrides.addEventListener(
        type, listener, useCapture, this, 'xhr',
        Benchmark.originals.addEventListenerToXHR);
  },

  removeEventListener: function(type, listener, useCapture, target,
                                originalFunction) {
    Benchmark.agent.cancelAsyncEvent(listener.eventId);
    originalFunction.call(target, listener.wrapper, useCapture);
  },

  removeEventListenerFromWindow: function(type, listener, useCapture) {
    removeEventListener(type, listener, useCapture, this,
                        Benchmark.originals.removeEventListenerFromWindow);
  },

  removeEventListenerFromNode: function(type, listener, useCapture) {
    removeEventListener(type, listener, useCapture, this,
        Benchmark.originals.removeEventListenerFromNode);
  },

  removeEventListenerFromXHR: function(type, listener, useCapture) {
    removeEventListener(type, listener, useCapture, this,
        Benchmark.originals.removeEventListenerFromXHR);
  },

  random: function() {
    return Benchmark.agent.random();
  },

  Date: function() {
    var a = arguments;
    var D = Benchmark.originals.Date, d;
    switch(a.length) {
      case 0: d = new D(Benchmark.agent.dateNow()); break;
      case 1: d = new D(a[0]); break;
      case 2: d = new D(a[0], a[1]); break;
      case 3: d = new D(a[0], a[1], a[2]); break;
      default: Benchmark.die('window.Date', arguments);
    }
    d.getTimezoneOffset = function() { return -240; };
    return d;
  },

  dateNow: function() {
    return Benchmark.agent.dateNow();
  },

  documentWriteln: function() {
    console.warn('writeln');
  },

  documentWrite: function() {
    console.warn('write');
  }
};

/**
 * Replaces window functions specified by Benchmark.functionList with overrides
 * and optionally saves original functions to Benchmark.originals.
 * @param {Object} wnd Window object.
 * @param {boolean} storeOriginals When true, original functions are saved to
 * Benchmark.originals.
 */
Benchmark.installOverrides = function(wnd, storeOriginals) {
  // Substitute window functions with overrides.
  for (var i = 0; i < Benchmark.functionList.length; ++i) {
    var info = Benchmark.functionList[i], object = wnd;
    var propertyName = info[1], pathToProperty = info[2];
    if (pathToProperty)
      for (var j = 0; j < pathToProperty.length; ++j)
        object = object[pathToProperty[j]];
    if (storeOriginals)
      Benchmark.originals[info[0]] = object[propertyName];
    object[propertyName] = Benchmark.overrides[info[0]];
  }
  wnd.__defineSetter__('onload', function() {
    console.warn('window.onload setter')}
  );

  // Substitute window functions of static frames when DOM content is loaded.
  Benchmark.originals.addEventListenerToWindow.call(wnd, 'DOMContentLoaded',
                                                    function() {
    var frames = document.getElementsByTagName('iframe');
    for (var i = 0, frame; frame = frames[i]; ++i) {
      Benchmark.installOverrides(frame.contentWindow);
    }
  }, true);

  // Substitute window functions of dynamically added frames.
  Benchmark.originals.addEventListenerToWindow.call(
      wnd, 'DOMNodeInsertedIntoDocument', function(e) {
        if (e.target.tagName && e.target.tagName.toLowerCase() != 'iframe')
          return;
        if (e.target.contentWindow)
          Benchmark.installOverrides(e.target.contentWindow);
      }, true);
};

// Install overrides on top window.
Benchmark.installOverrides(window, true);

/**
 * window.XMLHttpRequest wrapper. Notifies Benchmark.agent when request is
 * opened, aborted, and when it's ready state changes to DONE.
 * @constructor
 */
Benchmark.XMLHttpRequestWrapper = function() {
  this.request = new Benchmark.originals.XMLHttpRequest();
  this.wrapperReadyState = 0;
};

// Create XMLHttpRequestWrapper functions and property accessors using original
// ones.
(function() {
  var request = new Benchmark.originals.XMLHttpRequest();
  for (var property in request) {
    if (property === 'channel') continue; // Quick fix for FF.
    if (typeof(request[property]) == 'function') {
      (function(property) {
        var f = Benchmark.originals.XMLHttpRequest.prototype[property];
        Benchmark.XMLHttpRequestWrapper.prototype[property] = function() {
          f.apply(this.request, arguments);
        };
      })(property);
    } else {
      (function(property) {
        Benchmark.XMLHttpRequestWrapper.prototype.__defineGetter__(property,
            function() { return this.request[property]; });
        Benchmark.XMLHttpRequestWrapper.prototype.__defineSetter__(property,
            function(value) {
              this.request[property] = value;
            });

      })(property);
    }
  }
})();

// Define onreadystatechange getter.
Benchmark.XMLHttpRequestWrapper.prototype.__defineGetter__('onreadystatechange',
    function() { return this.clientOnReadyStateChange; });

// Define onreadystatechange setter.
Benchmark.XMLHttpRequestWrapper.prototype.__defineSetter__('onreadystatechange',
    function(value) { this.clientOnReadyStateChange = value; });

Benchmark.XMLHttpRequestWrapper.prototype.__defineGetter__('readyState',
    function() { return this.wrapperReadyState; });

Benchmark.XMLHttpRequestWrapper.prototype.__defineSetter__('readyState',
    function() {});


/**
 * Wrapper for XMLHttpRequest.open.
 */
Benchmark.XMLHttpRequestWrapper.prototype.open = function() {
  var url = Benchmark.extractURL(arguments[1]);
  var event = {type: 'request', method: arguments[0], url: url};
  this.eventId = Benchmark.agent.createAsyncEvent(event);

  var request = this.request;
  var requestWrapper = this;
  Benchmark.originals.XMLHttpRequest.prototype.open.apply(request, arguments);
  request.onreadystatechange = function() {
    if (this.readyState != 4 || requestWrapper.cancelled) return;
    var callback = requestWrapper.clientOnReadyStateChange || function() {};
    Benchmark.agent.fireAsyncEvent(requestWrapper.eventId, function() {
      requestWrapper.wrapperReadyState = 4;
      callback.call(request);
    });
  }
};

/**
 * Wrapper for XMLHttpRequest.abort.
 */
Benchmark.XMLHttpRequestWrapper.prototype.abort = function() {
  this.cancelled = true;
  Benchmark.originals.XMLHttpRequest.prototype.abort.apply(
      this.request, arguments);
  Benchmark.agent.cancelAsyncEvent(this.eventId);
};

/**
 * Driver url for reporting results.
 * @const {string}
 */
Benchmark.DRIVER_URL = '/benchmark/';

/**
 * Posts request as json to Benchmark.DRIVER_URL.
 * @param {Object} request Request to post.
 */
Benchmark.post = function(request, async) {
  if (async === undefined) async = true;
  var xmlHttpRequest = new Benchmark.originals.XMLHttpRequest();
  xmlHttpRequest.open("POST", Benchmark.DRIVER_URL, async);
  xmlHttpRequest.setRequestHeader("Content-type", "application/json");
  xmlHttpRequest.send(JSON.stringify(request));
};

/**
 * Extracts url string.
 * @param {(string|Object)} url Object or string representing url.
 * @return {string} Extracted url.
 */
Benchmark.extractURL = function(url) {
  if (typeof(url) == 'string') return url;
  return url.nI || url.G || '';
};


/**
 * Logs error message to console and throws an exception.
 * @param {string} message Error message
 */
Benchmark.die = function(message) {
  // Debugging stuff.
  var position = top.Benchmark.playback ? top.Benchmark.agent.timelinePosition :
                 top.Benchmark.agent.timeline.length;
  message = message + ' at position ' + position;
  console.error(message);
  Benchmark.post({error: message});
  console.log(Benchmark.originals.setTimeout.call(window, function() {}, 9999));
  try { (0)() } catch(ex) { console.error(ex.stack); }
  throw message;
};
