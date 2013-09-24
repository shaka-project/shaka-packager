# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Runs Facebook's JSGameBench benchmark."""

import os

from telemetry import test
from telemetry.core import util
from telemetry.page import page_measurement
from telemetry.page import page_set


class JsgamebenchMeasurement(page_measurement.PageMeasurement):
  def MeasurePage(self, _, tab, results):
    tab.ExecuteJavaScript('UI.call({}, "perftest")')

    js_is_done = 'document.getElementById("perfscore0") != null'
    def _IsDone():
      return bool(tab.EvaluateJavaScript(js_is_done))
    util.WaitFor(_IsDone, 1200)

    js_get_results = 'document.getElementById("perfscore0").innerHTML'
    result = int(tab.EvaluateJavaScript(js_get_results))
    results.Add('Score', 'score (bigger is better)', result)


class Jsgamebench(test.Test):
  """Counts how many animating sprites can move around on the screen at once."""
  test = JsgamebenchMeasurement

  def CreatePageSet(self, options):
    return page_set.PageSet.FromDict({
        'archive_data_file': '../data/jsgamebench.json',
        'pages': [
          { 'url': 'http://localhost/' }
          ]
        }, os.path.dirname(__file__))
