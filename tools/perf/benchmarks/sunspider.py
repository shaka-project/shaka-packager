# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import collections
import json
import os

from telemetry import test
from telemetry.core import util
from telemetry.page import page_measurement
from telemetry.page import page_set


class SunspiderMeasurement(page_measurement.PageMeasurement):
  def MeasurePage(self, _, tab, results):
    js_is_done = ('window.location.pathname.indexOf("results.html") >= 0'
                  '&& typeof(output) != "undefined"')
    def _IsDone():
      return tab.EvaluateJavaScript(js_is_done)
    util.WaitFor(_IsDone, 300, poll_interval=1)

    js_get_results = 'JSON.stringify(output);'
    js_results = json.loads(tab.EvaluateJavaScript(js_get_results))
    r = collections.defaultdict(list)
    totals = []
    # js_results is: [{'foo': v1, 'bar': v2},
    #                 {'foo': v3, 'bar': v4},
    #                 ...]
    for result in js_results:
      total = 0
      for key, value in result.iteritems():
        r[key].append(value)
        total += value
      totals.append(total)
    for key, values in r.iteritems():
      results.Add(key, 'ms', values, data_type='unimportant')
    results.Add('Total', 'ms', totals)


class Sunspider(test.Test):
  """Apple's SunSpider JavaScript benchmark."""
  test = SunspiderMeasurement

  def CreatePageSet(self, options):
    sunspider_dir = os.path.join(util.GetChromiumSrcDir(),
                                 'chrome', 'test', 'data', 'sunspider')
    return page_set.PageSet.FromDict(
        {
          'serving_dirs': [''],
          'pages': [{ 'url': 'file:///sunspider-1.0/driver.html' }],
        },
        sunspider_dir)
