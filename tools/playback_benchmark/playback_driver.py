# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Playback driver."""

import cgi
import simplejson as json
import os
import string
import sys
import threading
import urlparse

START_PAGE = """<html>
  <script type="text/javascript">
var runCount = $run_count;
var results = [];

function run() {
  var wnd = window.open('?resource=start_page_popup', '',
                        'width=$width, height=$height');
  var timerId = setInterval(function() {
    wnd.postMessage('ping', '$target_origin');
  }, 300);
  var handleMessage = function(event) {
    clearInterval(timerId);
    wnd.close();
    document.writeln('<div>' + event.data + '</div>');
    results.push(event.data);
    runCount -= 1;
    window.removeEventListener('message', handleMessage);
    if (runCount > 0) {
      run();
    } else {
      var xmlHttpRequest = new XMLHttpRequest();
      xmlHttpRequest.open("POST", '/benchmark/', true);
      xmlHttpRequest.setRequestHeader("Content-type", "application/json");
      xmlHttpRequest.send(JSON.stringify({results: results}));
    }
  }
  window.addEventListener('message', handleMessage, false);
}

run();
  </script>
</html>
"""

START_PAGE_POPUP = """<html>
  <script type="text/javascript">
window.setTimeout(function() {
  console.log(window.innerWidth, window.innerHeight);
  if (window.innerWidth == $width && window.innerHeight == $height) {
    window.location = '$start_url';
  } else {
    window.resizeBy($width - window.innerWidth, $height - window.innerHeight);
    window.location = window.location;
  }
}, 200);
  </script>
</html>
"""

DATA_JS = 'Benchmark.data = $data;'


def ReadFile(file_name, mode='r'):
  f = open(file_name, mode)
  data = f.read()
  f.close()
  return data


def ReadJSON(file_name):
  f = open(file_name, 'r')
  data = json.load(f)
  f.close()
  return data


class PlaybackRequestHandler(object):
  """This class is used to process HTTP requests during test playback.

  Attributes:
    test_dir: directory containing test files.
    test_callback: function to be called when the test is finished.
    script_dir: directory where javascript files are located.
  """

  def __init__(self, test_dir, test_callback=None, script_dir=os.getcwd()):
    self.test_dir = test_dir
    self.test_callback = test_callback
    self.script_dir = script_dir

  def ProcessRequest(self, handler):
    "Processes single HTTP request."

    parse_result = urlparse.urlparse(handler.path)
    if parse_result.path.endswith('/benchmark/'):
      query = cgi.parse_qs(parse_result.query)
      if 'run_test' in query:
        run_count = 1
        if 'run_count' in query:
          run_count = query['run_count'][0]
        self._StartTest(handler, self.test_dir, run_count)
      elif 'resource' in query:
        self._GetBenchmarkResource(query['resource'][0], handler)
      else:
        self._ProcessBenchmarkReport(handler.body, handler)
    else:
      self._GetApplicationResource(handler)

  def _StartTest(self, handler, test_dir, run_count):
    "Sends test start page to browser."

    cache_data = ReadJSON(os.path.join(test_dir, 'cache.json'))

    # Load cached responses.
    self.cache = {}
    responses_dir = os.path.join(test_dir, 'responses')
    for request in cache_data['requests']:
      response_file = os.path.join(responses_dir, request['response_file'])
      response = ReadFile(response_file, 'rb')
      key = (request['method'], request['path'])
      self.cache[key] = {'response': response, 'headers': request['headers']}

    # Load benchmark scripts.
    self.benchmark_resources = {}
    data = ReadFile(os.path.join(test_dir, 'data.json'))
    data = string.Template(DATA_JS).substitute(data=data)
    self.benchmark_resources['data.js'] = {'data': data,
                                           'type': 'application/javascript'}
    for resource in ('common.js', 'playback.js'):
      resource_file = os.path.join(self.script_dir, resource)
      self.benchmark_resources[resource] = {'data': ReadFile(resource_file),
                                            'type': 'application/javascript'}

    # Format start page.
    parse_result = urlparse.urlparse(cache_data['start_url'])
    target_origin = '%s://%s' % (parse_result.scheme, parse_result.netloc)
    start_page = string.Template(START_PAGE).substitute(
        run_count=run_count, target_origin=target_origin,
        width=cache_data['width'], height=cache_data['height'])
    self.benchmark_resources['start_page'] = {
      'data': start_page,
      'type': 'text/html; charset=UTF-8'
    }

    start_page_popup = string.Template(START_PAGE_POPUP).substitute(
        start_url=cache_data['start_url'],
        width=cache_data['width'], height=cache_data['height'])
    self.benchmark_resources['start_page_popup'] = {
      'data': start_page_popup,
      'type': 'text/html; charset=UTF-8'
    }

    self._GetBenchmarkResource('start_page', handler)

  def _GetBenchmarkResource(self, resource, handler):
    "Sends requested resource to browser."

    if resource in self.benchmark_resources:
      resource = self.benchmark_resources[resource]
      handler.send_response(200)
      handler.send_header('content-length', len(resource['data']))
      handler.send_header('content-type', resource['type'])
      handler.end_headers()
      handler.wfile.write(resource['data'])
    else:
      handler.send_response(404)
      handler.end_headers()

  def _ProcessBenchmarkReport(self, content, handler):
    "Reads benchmark score from report content and invokes callback."

    handler.send_response(204)
    handler.end_headers()
    content = json.loads(content)
    if 'results' in content:
      results = content['results']
      sys.stdout.write('Results: %s\n' % results)
      if self.test_callback: self.test_callback(results)
    elif 'error' in content:
      sys.stderr.write('Error: %s\n' % content['error'])

  def _GetApplicationResource(self, handler):
    "Searches for response in cache. If not found, responds with 204."
    key = (handler.command, handler.path)
    if key in self.cache:
      sys.stdout.write('%s %s -> found\n' % key)
      handler.wfile.write(self.cache[key]['response'])
    else:
      sys.stderr.write('%s %s -> not found\n' % key)
      handler.send_response(204, "not in cache")
      handler.end_headers()
