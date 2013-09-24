// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

document.cookie = "__navigated_to_report=0; path=/";
document.cookie = "__pc_done=0; path=/";
document.cookie = "__pc_timings=; path=/";

function dirname(path) {
  var match = path.match(/(.*)\//);
  if (match) {
    return match[1];
  } else {
    return ".";
  }
}

function IsWprRecordMode() {
  var kStatusUrl = "http://wprwprwpr/web-page-replay-command-status";
  var isRecordMode;
  var xhr = new XMLHttpRequest();
  var useAsync = false;
  xhr.open("GET", kStatusUrl, useAsync);
  xhr.onreadystatechange = function() {
    if (xhr.readyState == 4 && xhr.status == 200) {
      var status = JSON.parse(xhr.responseText);
      isRecordMode = status["is_record_mode"];
      console.log("WPR record mode?: " + isRecordMode);
    }
  };
  try {
    xhr.send();
  } catch(e) {
    throw "Web Page Replay is not responding. Start WPR to continue."
  }
  return isRecordMode;
}


function TryStart() {
  console.log("try start");
  var status_element = document.getElementById("status");

  var config_json;
  var config;
  try {
    config_json = document.getElementById("json").textContent;
    config = JSON.parse(config_json);
  } catch(err) {
    console.log("Bad json data: " + config_json);
    status_element.textContent = "Exception: " + err + "\njson data: " +
        config_json;
    return;
  }
  var isRecordMode = false;
  try {
    isRecordMode = IsWprRecordMode();
  } catch (err) {
    status_element.textContent = err;
    setTimeout(TryStart, 5000);
    return;
  }

  if (!config["shouldStart"]) {
    status_element.textContent =
        "Press 'Start' to continue (or load this page with 'auto=1').";
    return;
  }

  try {
    var reportDir = dirname(dirname(window.location.pathname)) + "/common";
    config["reportUrl"] = "file://" + reportDir + "/report.html";
    config["isRecordMode"] = isRecordMode;
    var port = chrome.runtime.connect();
    port.postMessage({message: "start", benchmark: config});
    console.log("sending start message: page count, " +
                config["pageSets"].length);
  } catch(err) {
    console.log("TryStart retrying after exception: " + err);
    status_element.textContent = "Exception: " + err;
    setTimeout(TryStart, 1000);
    return;
  }
  status_element.textContent = "STARTING";
}

// We wait before starting the test just to let chrome warm up better.
setTimeout(TryStart, 250);
