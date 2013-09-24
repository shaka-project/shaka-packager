# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Runs spaceport.io's PerfMarks benchmark."""

import logging
import os

from telemetry import test
from telemetry.core import util
from telemetry.page import page_measurement
from telemetry.page import page_set


class SpaceportMeasurement(page_measurement.PageMeasurement):
  def CustomizeBrowserOptions(self, options):
    options.extra_browser_args.extend(['--disable-gpu-vsync'])

  def MeasurePage(self, _, tab, results):
    util.WaitFor(lambda: tab.EvaluateJavaScript(
        '!document.getElementById("start-performance-tests").disabled'), 60)

    tab.ExecuteJavaScript("""
        window.__results = {};
        window.console.log = function(str) {
            if (!str) return;
            var key_val = str.split(': ');
            if (!key_val.length == 2) return;
            __results[key_val[0]] = key_val[1];
        };
        document.getElementById('start-performance-tests').click();
    """)

    js_get_results = 'JSON.stringify(window.__results)'
    num_tests_complete = [0]  # A list to work around closure issue.
    def _IsDone():
      num_tests_in_measurement = 24
      num_results = len(eval(tab.EvaluateJavaScript(js_get_results)))
      if num_results > num_tests_complete[0]:
        num_tests_complete[0] = num_results
        logging.info('Completed measurement %d of %d'
                     % (num_tests_complete[0],
                        num_tests_in_measurement))
      return num_tests_complete[0] >= num_tests_in_measurement
    util.WaitFor(_IsDone, 1200, poll_interval=5)

    result_dict = eval(tab.EvaluateJavaScript(js_get_results))
    for key in result_dict:
      chart, trace = key.split('.', 1)
      results.Add(trace, 'objects (bigger is better)', float(result_dict[key]),
                  chart_name=chart, data_type='unimportant')
    results.Add('Score', 'objects (bigger is better)',
                [float(x) for x in result_dict.values()])


class Spaceport(test.Test):
  """spaceport.io's PerfMarks benchmark."""
  test = SpaceportMeasurement

  def CreatePageSet(self, options):
    spaceport_dir = os.path.join(util.GetChromiumSrcDir(), 'chrome', 'test',
        'data', 'third_party', 'spaceport')
    return page_set.PageSet.FromDict(
        {'pages': [{'url': 'file:///index.html'}]},
        spaceport_dir)
