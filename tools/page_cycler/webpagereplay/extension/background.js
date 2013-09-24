// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// start.js sends a "start" message to set this.
window.benchmarkConfiguration = {};

// The callback (e.g. report writer) is set via AddBenchmarckCallback.
window.benchmarkCallback;

// Url to load before loading target page.
var kWaitUrl = "http://wprwprwpr/web-page-replay-generate-200";

// Constant StatCounter Names
var kTcpReadBytes = "tcp.read_bytes";
var kTcpWriteBytes = "tcp.write_bytes";
var kRequestCount = "HttpNetworkTransaction.Count";
var kConnectCount = "tcp.connect";

function CHECK(expr, comment) {
  if (!expr) {
    console.log(comment);
    alert(comment);
  }
}

function Result() {
  var me_ = this;
  this.url = "";
  this.firstPaintTime = 0;
  this.readBytesKB = 0;
  this.writeBytesKB = 0;
  this.numRequests = 0;
  this.numConnects = 0;
  this.timing = {};  // window.performance.timing
  this.getTotalTime = function() {
    var totalTime = 0
    if (me_.timing.navigationStart && me_.timing.loadEventEnd) {
      totalTime = me_.timing.loadEventEnd - me_.timing.navigationStart;
    }
    CHECK(totalTime >= 0);
    return totalTime;
  }
}

// Collect all the results for a session (i.e. different pages).
function ResultsCollection() {
  var results_ = [];
  var pages_ = [];
  var pageResults_ = {};

  this.addResult = function(result) {
    results_.push(result);
    var url = result.url;
    if (!(url in pageResults_)) {
      pages_.push(url);
      pageResults_[url] = [];
    }
    pageResults_[url].push(result);
  }

  this.getPages = function() {
    return pages_;
  }

  this.getResults = function() {
    return results_;
  }

  this.getTotalTimes = function() {
    return results_.map(function (t) { return t.getTotalTime(); });
  }
}

// Load a url in the default tab and record the time.
function PageLoader(url, resultReadyCallback) {
  var me_ = this;
  var url_ = url;
  var resultReadyCallback_ = resultReadyCallback;

  // If it record mode, wait a little longer for lazy loaded resources.
  var postLoadGraceMs_ = window.isRecordMode ? 5000 : 0;
  var loadInterval_ = window.loadInterval;
  var checkInterval_ = window.checkInterval;
  var timeout_ = window.timeout;
  var maxLoadChecks_ = window.maxLoadChecks;

  var preloadFunc_;
  var timeoutId_;
  var isFinished_;
  var result_;

  var initialReadBytes_;
  var initialWriteBytes_;
  var initialRequestCount_;
  var initialConnectCount_;

  this.result = function() { return result_; };

  this.run = function() {
    timeoutId_ = null;
    isFinished_ = false;
    result_ = null;
    initialReadBytes_ = chrome.benchmarking.counter(kTcpReadBytes);
    initialWriteBytes_ = chrome.benchmarking.counter(kTcpWriteBytes);
    initialRequestCount_ = chrome.benchmarking.counter(kRequestCount);
    initialConnectCount_ = chrome.benchmarking.counter(kConnectCount);

    if (me_.preloadFunc_) {
      me_.preloadFunc_(me_.load_);
    } else {
      me_.load_();
    }
  };

  this.setClearAll = function() {
    me_.preloadFunc_ = me_.clearAll_;
  };

  this.setClearConnections = function() {
    me_.preloadFunc_ = me_.clearConnections_;
  };

  this.clearAll_ = function(callback) {
    chrome.tabs.getSelected(null, function(tab) {
        chrome.tabs.update(tab.id, {"url": kWaitUrl}, function() {
            chrome.benchmarking.clearHostResolverCache();
            chrome.benchmarking.clearPredictorCache();
            chrome.benchmarking.closeConnections();
            var dataToRemove = {
                "appcache": true,
                "cache": true,
                "cookies": true,
                "downloads": true,
                "fileSystems": true,
                "formData": true,
                "history": true,
                "indexedDB": true,
                "localStorage": true,
                "passwords": true,
                "pluginData": true,
                "webSQL": true
            };
            // Add any items new to the API.
            for (var prop in chrome.browsingData) {
              var dataName = prop.replace("remove", "");
              if (dataName && dataName != prop) {
                dataName = dataName.charAt(0).toLowerCase() +
                    dataName.substr(1);
                if (!dataToRemove.hasOwnProperty(dataName)) {
                  console.log("New browsingData API item: " + dataName);
                  dataToRemove[dataName] = true;
                }
              }
            }
            chrome.browsingData.remove({}, dataToRemove, callback);
          });
      });
  };

  this.clearConnections_ = function(callback) {
    chrome.benchmarking.closeConnections();
    callback();
  };

  this.load_ = function() {
    console.log("LOAD started: " + url_);
    setTimeout(function() {
      chrome.extension.onRequest.addListener(me_.finishLoad_);
      timeoutId_ = setTimeout(function() {
          me_.finishLoad_({"loadTimes": null, "timing": null});
      }, timeout_);
      chrome.tabs.getSelected(null, function(tab) {
          chrome.tabs.update(tab.id, {"url": url_});
      });
    }, loadInterval_);
  };

  this.finishLoad_ = function(msg) {
    if (!isFinished_) {
      isFinished_ = true;
      clearTimeout(timeoutId_);
      chrome.extension.onRequest.removeListener(me_.finishLoad_);
      me_.saveResult_(msg.loadTimes, msg.timing);
    }
  };

  this.saveResult_ = function(loadTimes, timing) {
    result_ = new Result()
    result_.url = url_;
    if (!loadTimes || !timing) {
      console.log("LOAD INCOMPLETE: " + url_);
    } else {
      console.log("LOAD complete: " + url_);
      result_.timing = timing;
      var baseTime = timing.navigationStart;
      CHECK(baseTime);
      result_.firstPaintTime = Math.max(0,
          Math.round((1000.0 * loadTimes.firstPaintTime) - baseTime));
    }
    result_.readBytesKB = (chrome.benchmarking.counter(kTcpReadBytes) -
                           initialReadBytes_) / 1024;
    result_.writeBytesKB = (chrome.benchmarking.counter(kTcpWriteBytes) -
                            initialWriteBytes_) / 1024;
    result_.numRequests = (chrome.benchmarking.counter(kRequestCount) -
                           initialRequestCount_);
    result_.numConnects = (chrome.benchmarking.counter(kConnectCount) -
                           initialConnectCount_);
    setTimeout(function() { resultReadyCallback_(me_); }, postLoadGraceMs_);
  };
}

// Load page sets and prepare performance results.
function SessionLoader(resultsReadyCallback) {
  var me_ = this;
  var resultsReadyCallback_ = resultsReadyCallback;
  var pageSets_ = benchmarkConfiguration.pageSets;
  var iterations_ = window.iterations;
  var retries_ = window.retries;

  var pageLoaders_ = [];
  var resultsCollection_ = new ResultsCollection();
  var loaderIndex_ = 0;
  var retryIndex_ = 0;
  var iterationIndex_ = 0;

  this.run = function() {
    me_.createLoaders_();
    me_.loadPage_();
  }

  this.getResultsCollection = function() {
    return resultsCollection_;
  }

  this.createLoaders_ = function() {
    // Each url becomes one benchmark.
    for (var i = 0; i < pageSets_.length; i++) {
      for (var j = 0; j < pageSets_[i].length; j++) {
        // Remove extra space at the beginning or end of a url.
        var url = pageSets_[i][j].trim();
        // Alert about and ignore blank page which does not get loaded.
        if (url == "about:blank") {
          alert("blank page loaded!");
        } else if (!url.match(/https?:\/\//)) {
          // Alert about url that is not in scheme http:// or https://.
          alert("Skipping url without http:// or https://: " + url);
        } else {
          var loader = new PageLoader(url, me_.handleResult_)
          if (j == 0) {
            // Clear all browser data for the first page in a sub list.
            loader.setClearAll();
          } else {
            // Otherwise, only clear the connections.
            loader.setClearConnections();
          }
          pageLoaders_.push(loader);
        }
      }
    }
  }

  this.loadPage_ = function() {
    console.log("LOAD url " + (loaderIndex_ + 1) + " of " +
                pageLoaders_.length +
                ", iteration " + (iterationIndex_ + 1) + " of " +
                iterations_);
    pageLoaders_[loaderIndex_].run();
  }

  this.handleResult_ = function(loader) {
    var result = loader.result();
    resultsCollection_.addResult(result);
    var totalTime = result.getTotalTime();
    if (!totalTime && retryIndex_ < retries_) {
      retryIndex_++;
      console.log("LOAD retry, " + retryIndex_);
    } else {
      retryIndex_ = 0;
      console.log("RESULTS url " + (loaderIndex_ + 1) + " of " +
                  pageLoaders_.length +
                  ", iteration " + (iterationIndex_ + 1) + " of " +
                  iterations_ + ": " + totalTime);
      loaderIndex_++;
      if (loaderIndex_ >= pageLoaders_.length) {
        iterationIndex_++;
        if (iterationIndex_ < iterations_) {
          loaderIndex_ = 0;
        } else {
          resultsReadyCallback_(me_);
          return;
        }
      }
    }
    me_.loadPage_();
  }
}

function AddBenchmarkCallback(callback) {
  window.benchmarkCallback = callback;
}

function Run() {
  window.checkInterval = 500;
  window.loadInterval = 1000;
  window.timeout = 20000;  // max ms before killing page.
  window.retries = 0;
  window.isRecordMode = benchmarkConfiguration.isRecordMode;
  if (window.isRecordMode) {
    window.iterations = 1;
    window.timeout = 40000;
    window.retries = 2;
  } else {
    window.iterations = benchmarkConfiguration["iterations"] || 3;
  }
  var sessionLoader = new SessionLoader(benchmarkCallback);
  console.log("pageSets: " + JSON.stringify(benchmarkConfiguration.pageSets));
  sessionLoader.run();
}

chrome.runtime.onConnect.addListener(function(port) {
  port.onMessage.addListener(function(data) {
    if (data.message == "start") {
      window.benchmarkConfiguration = data.benchmark;
      Run()
    }
  });
});
