// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Returns the sum of all values in the array.
Array.sum = function(array) {
  var sum = 0;
  for (var i = array.length - 1; i >= 0; i--) {
    sum += array[i];
  }
  return sum;
};


function WriteReport(sessionLoader) {
  var iterations = window.iterations;
  var reportUrl = window.benchmarkConfiguration.reportUrl;
  var resultsCollection = sessionLoader.getResultsCollection();
  var times = resultsCollection.getTotalTimes();
  var pages = resultsCollection.getPages();

  reportUrl += "?n=" + iterations;
  reportUrl += "&i=" + iterations * pages.length;  // "cycles"
  reportUrl += "&td=" + Array.sum(times);  // total time
  reportUrl += "&tf=" + 0;  // fudge time
  console.log('reportUrl: ' + reportUrl);
  chrome.cookies.set({
    "url": reportUrl,
    "name": "__pc_done",
    "value": "1",
    "path": "/",
  });
  chrome.cookies.set({
    "url": reportUrl,
    "name": "__pc_pages",
    "value": pages.map(function(x) {
      return x.replace(/=/g, "%3D");
    }).join(","),
    "path": "/",
  });
  chrome.cookies.set({
    "url": reportUrl,
    "name": "__pc_timings",
    "value": times.join(","),
    "path": "/",
  });

  chrome.tabs.getSelected(null, function(tab) {
    console.log("Navigate to the report.");
    chrome.tabs.update(tab.id, {"url": reportUrl}, null);
  });
}

AddBenchmarkCallback(WriteReport);
