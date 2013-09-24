// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// webpagereplay/start.js - Start Web Page Replay (WPR) test.
//
// This script is included by webpagereplay/start.html.
// The query parameter "test=TEST_NAME" is required to load the
// test configuration from webpagereplay/tests/TEST_NAME.js
// That JavaScript file defines a global, "pageSets", as a list of lists:
//       [ [url_1, url_2], [url_3], ...],
//       - Before each sublist:
//         Run chrome.browingData.remove and close the connections.
//       - Before each url in a sublist:
//         Close the connections.
//
// These WPR tests use a Chrome extension to load the test URLs.
// The extension loads the test configuration via a DOM elemment
// (id=json).  This script sets the content of that DOM element.
//
// The test runs immediately after being loaded.
//


var options = location.search.substring(1).split('&');
function getopt(name) {
  var r = new RegExp('^' + name + '=');
  for (i = 0; i < options.length; ++i) {
    if (options[i].match(r)) {
      return options[i].substring(name.length + 1);
    }
  }
  return null;
}

function LoadTestConfigurationScript(testUrl, callback) {
  var testjs = document.createElement('script');
  testjs.type = 'text/javascript';
  testjs.async = true;
  testjs.src = testUrl
  var s = document.getElementsByTagName('script')[0];
  testjs.addEventListener('load', callback, false);
  s.parentNode.insertBefore(testjs, s);
}

function ReloadIfStuck() {
  setTimeout(function() {
      var status = document.getElementById('status');
      // The status text is set to 'STARTING' by the extension.
      if (status.textContent != 'STARTING') {
        console.log('Benchmark stuck?  Reloading.');
        window.location.reload(true);
      }
    }, 30000);
}

function RenderForm() {
  var form = document.createElement('FORM');
  form.setAttribute('action', 'start.html');

  var label = document.createTextNode('Iterations: ');
  form.appendChild(label);

  var input = document.createElement('INPUT');
  var iterations = getopt('iterations');
  input.setAttribute('name', 'iterations');
  input.setAttribute('value', iterations ? iterations : '5');
  form.appendChild(input);

  form.appendChild(document.createElement('P'));

  var label = document.createTextNode('Test: ');
  form.appendChild(label);

  var input = document.createElement('INPUT');
  input.setAttribute('name', 'test');
  var test = getopt('test');
  input.setAttribute('value', test ? test : '');
  form.appendChild(input);

  var input = document.createElement('INPUT');
  input.setAttribute('name', 'auto');
  var auto = getopt('auto');
  input.setAttribute('value', 1);
  input.setAttribute('type', 'hidden');
  form.appendChild(input);

  form.appendChild(document.createElement('P'));

  input = document.createElement('INPUT');
  input.setAttribute('type', 'submit');
  input.setAttribute('value', 'Start');
  form.appendChild(input);

  document.getElementById('startform').appendChild(form);
}


var iterations = getopt('iterations');
var test_name = getopt('test');
var is_auto_start = getopt('auto');

RenderForm();
if (test_name) {
  var testUrl = 'tests/' + test_name + '.js';
  LoadTestConfigurationScript(testUrl, function() {
      var testConfig = {};
      if (iterations) {
        testConfig['iterations'] = iterations;
      }
      // The pageSets global is set by test configuration script.
      testConfig['pageSets'] = pageSets;

      if (is_auto_start) {
        testConfig['shouldStart'] = 1;
        ReloadIfStuck();
      }
      // Write testConfig to "json" DOM element for the Chrome extension.
      document.getElementById('json').textContent = JSON.stringify(testConfig);
    });
} else {
  console.log('Need "test=TEST_NAME" query parameter.');
}
