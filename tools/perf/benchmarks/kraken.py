# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Runs Mozilla's Kraken JavaScript benchmark."""

import os

from telemetry import test
from telemetry.core import util
from telemetry.page import page_measurement
from telemetry.page import page_set


def _Mean(l):
  return float(sum(l)) / len(l) if len(l) > 0 else 0.0


class KrakenMeasurement(page_measurement.PageMeasurement):
  def MeasurePage(self, _, tab, results):
    js_is_done = """
document.title.indexOf("Results") != -1 && document.readyState == "complete"
"""
    def _IsDone():
      return bool(tab.EvaluateJavaScript(js_is_done))
    util.WaitFor(_IsDone, 500, poll_interval=5)

    js_get_results = """
var formElement = document.getElementsByTagName("input")[0];
decodeURIComponent(formElement.value.split("?")[1]);
"""
    result_dict = eval(tab.EvaluateJavaScript(js_get_results))
    total = 0
    for key in result_dict:
      if key == 'v':
        continue
      results.Add(key, 'ms', result_dict[key], data_type='unimportant')
      total += _Mean(result_dict[key])
    results.Add('Total', 'ms', total)


class Kraken(test.Test):
  """Mozilla's Kraken JavaScript benchmark."""
  test = KrakenMeasurement

  def CreatePageSet(self, options):
    return page_set.PageSet.FromDict({
        'archive_data_file': '../data/kraken.json',
        'pages': [
          { 'url': 'http://krakenbenchmark.mozilla.org/kraken-1.1/driver.html' }
          ]
        }, os.path.abspath(__file__))
