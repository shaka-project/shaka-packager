# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

from telemetry import test
from telemetry.core import util
from telemetry.page import page_measurement
from telemetry.page import page_set


class OctaneMeasurement(page_measurement.PageMeasurement):
  def MeasurePage(self, _, tab, results):
    js_is_done = """
completed && !document.getElementById("progress-bar-container")"""
    def _IsDone():
      return bool(tab.EvaluateJavaScript(js_is_done))
    util.WaitFor(_IsDone, 300, poll_interval=5)

    js_get_results = """
var results = {};
var result_divs = document.querySelectorAll('.p-result');
for (var r in result_divs) {
  if (result_divs[r].id && result_divs[r].id.indexOf('Result-') == 0) {
    var key = result_divs[r].id.replace('Result-', '');
    results[key] = result_divs[r].innerHTML;
  }
}
var main_banner = document.getElementById("main-banner").innerHTML;
var octane_score = main_banner.substr(main_banner.lastIndexOf(':') + 2);
results['score'] = octane_score;
JSON.stringify(results);
"""
    result_dict = eval(tab.EvaluateJavaScript(js_get_results))
    for key, value in result_dict.iteritems():
      if value == '...':
        continue
      data_type = 'unimportant'
      if key == 'score':
        data_type = 'default'
      results.Add(key, 'score (bigger is better)', value, data_type=data_type)


class Octane(test.Test):
  """Google's Octane JavaScript benchmark."""
  test = OctaneMeasurement

  def CreatePageSet(self, options):
    octane_dir = os.path.join(util.GetChromiumSrcDir(), 'chrome', 'test',
                              'data', 'perf', 'third_party', 'octane')
    return page_set.PageSet.FromDict({
        'pages': [
          { 'url': 'file:///index.html?auto=1' }
          ]
        }, octane_dir)
