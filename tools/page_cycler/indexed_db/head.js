// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var totalTime;
var fudgeTime;
var elapsedTime;
var endTime;
var iterations;
var cycle;
var results = false;
var TIMEOUT = 15;

/**
 * Returns the value of the given property stored in the cookie.
 * @param {string} name The property name.
 * @return {string} The value of the given property, or empty string
 *     if the property was not found.
 */
function __get_cookie(name) {
  var cookies = document.cookie.split('; ');
  for (var i = 0; i < cookies.length; ++i) {
    var t = cookies[i].split('=');
    if ((t[0] == name) && t[1])
      return t[1];
  }
  return '';
}

function __get_timings() {
  if (sessionStorage == null)
    return __get_cookie("__pc_timings");
  else {
    if (sessionStorage.getItem("__pc_timings") == null)
      return "";
    else
      return sessionStorage["__pc_timings"];
  }
}
function __set_timings(timings) {
  if (sessionStorage == null)
    document.cookie = "__pc_timings=" + timings + "; path=/";
  else
    sessionStorage["__pc_timings"]=timings;
}

/**
 * Starts the next test cycle or redirects the browser to the results page.
 */
function nextCycleOrResults() {
  // Call GC twice to cleanup JS heap before starting a new test.
  if (window.gc) {
    window.gc();
    window.gc();
  }

  var timings = elapsedTime;
  var oldTimings = __get_timings();
  if (oldTimings != '')
    timings = oldTimings + ',' + timings;
  __set_timings(timings);

  var tLag = Date.now() - endTime - TIMEOUT;
  if (tLag > 0)
    fudgeTime += tLag;

  var doc;
  if (cycle == iterations) {
    document.cookie = '__pc_done=1; path=/';
    doc = '../../common/report.html';
    if (window.console) {
      console.log("Pages: [" + __get_cookie('__pc_pages') + "]");
      console.log("times: [" + __get_timings() + "]");
    }
  } else {
    doc = 'index.html';
  }

  var url = doc + '?n=' + iterations + '&i=' + cycle +
      '&td=' + totalTime + '&tf=' + fudgeTime;
  document.location.href = url;
}

/**
 * Computes various running times and updates the stats reported at the end.
 * @param {!number} cycleTime The running time of the test cycle.
 */
function testComplete(cycleTime) {
  if (results)
    return;

  var oldTotalTime = 0;
  var cycleEndTime = Date.now();
  var cycleFudgeTime = 0;

  var s = document.location.search;
  if (s) {
    var params = s.substring(1).split('&');
    for (var i = 0; i < params.length; i++) {
      var f = params[i].split('=');
      switch (f[0]) {
        case 'skip':
          return;  // No calculation, just viewing
        case 'n':
          iterations = f[1];
          break;
        case 'i':
          cycle = f[1] - 0 + 1;
          break;
        case 'td':
          oldTotalTime = f[1] - 0;
          break;
        case 'tf':
          cycleFudgeTime = f[1] - 0;
          break;
      }
    }
  }
  elapsedTime = cycleTime;
  totalTime = oldTotalTime + elapsedTime;
  endTime = cycleEndTime;
  fudgeTime = cycleFudgeTime;

  setTimeout(nextCycleOrResults, TIMEOUT);
}
