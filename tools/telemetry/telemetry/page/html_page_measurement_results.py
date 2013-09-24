# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import datetime
import json
import logging
import os
import re
import sys

from telemetry.core import util
from telemetry.page import buildbot_page_measurement_results

# Get build/util scripts into our path.
sys.path.append(os.path.abspath(os.path.join(
    util.GetChromiumSrcDir(), 'build', 'util')))
import lastchange  # pylint: disable=F0401


_TEMPLATE_HTML_PATH = os.path.join(
    util.GetTelemetryDir(), 'support', 'html_output', 'results-template.html')
_PLUGINS = [('third_party', 'flot', 'jquery.flot.min.js'),
            ('third_party', 'WebKit', 'PerformanceTests', 'resources',
             'jquery.tablesorter.min.js'),
            ('third_party', 'WebKit', 'PerformanceTests', 'resources',
             'statistics.js')]
_UNIT_JSON = ('tools', 'perf', 'unit-info.json')


class HtmlPageMeasurementResults(
    buildbot_page_measurement_results.BuildbotPageMeasurementResults):
  def __init__(self, output_stream, test_name, reset_results, browser_type,
               trace_tag=''):
    super(HtmlPageMeasurementResults, self).__init__(trace_tag)

    self._output_stream = output_stream
    self._reset_results = reset_results
    self._test_name = test_name
    self._result_json = {
        'buildTime': self._GetBuildTime(),
        'revision': self._GetRevision(),
        'platform': browser_type,
        'tests': {}
        }

  def _GetBuildTime(self):
    def _DatetimeInEs5CompatibleFormat(dt):
      return dt.strftime('%Y-%m-%dT%H:%M:%S.%f')
    return _DatetimeInEs5CompatibleFormat(datetime.datetime.utcnow())

  def _GetRevision(self):
    return lastchange.FetchVersionInfo(None).revision

  def _GetHtmlTemplate(self):
    return open(_TEMPLATE_HTML_PATH, 'r').read()

  def _GetPlugins(self):
    plugins = ''
    for p in _PLUGINS:
      plugins += open(os.path.join(util.GetChromiumSrcDir(), *p), 'r').read()
    return plugins

  def _GetUnitJson(self):
    return open(os.path.join(util.GetChromiumSrcDir(), *_UNIT_JSON), 'r').read()

  def _GetResultsJson(self):
    results_html = self._output_stream.read()
    if self._reset_results or not results_html:
      return []
    m = re.search(
        '^<script id="results-json" type="application/json">(.*?)</script>$',
        results_html, re.MULTILINE | re.DOTALL)
    if not m:
      logging.warn('Failed to extract previous results from HTML output')
      return []
    return json.loads(m.group(1))

  def _SaveResults(self, results):
    self._output_stream.seek(0)
    self._output_stream.write(results)
    self._output_stream.truncate()

  def _PrintPerfResult(self, measurement, trace, values, units,
                       result_type='default'):
    super(HtmlPageMeasurementResults, self)._PrintPerfResult(
        measurement, trace, values, units, result_type)

    metric_name = measurement
    if trace != measurement:
      metric_name += '.' + trace
    self._result_json['tests'].setdefault(self._test_name, {})
    self._result_json['tests'][self._test_name].setdefault('metrics', {})
    self._result_json['tests'][self._test_name]['metrics'][metric_name] = {
        'current': values,
        'units': units,
        'important': result_type == 'default'
        }

  def PrintSummary(self):
    super(HtmlPageMeasurementResults, self).PrintSummary()

    json_results = self._GetResultsJson()
    json_results.append(self._result_json)
    html = self._GetHtmlTemplate()
    html = html.replace('%json_results%', json.dumps(json_results))
    html = html.replace('%json_units%', self._GetUnitJson())
    html = html.replace('%plugins%', self._GetPlugins())
    self._SaveResults(html)

    print
    print 'View result at file://%s' % os.path.abspath(self._output_stream.name)
