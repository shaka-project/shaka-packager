# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Runs Microsoft's RoboHornet Pro benchmark."""

import os

from telemetry import test
from telemetry.core import util
from telemetry.page import page_measurement
from telemetry.page import page_set


class RobohornetProMeasurement(page_measurement.PageMeasurement):
  def MeasurePage(self, _, tab, results):
    tab.ExecuteJavaScript('ToggleRoboHornet()')

    done = 'document.getElementById("results").innerHTML.indexOf("Total") != -1'
    def _IsDone():
      return tab.EvaluateJavaScript(done)
    util.WaitFor(_IsDone, 120)

    result = int(tab.EvaluateJavaScript('stopTime - startTime'))
    results.Add('Total', 'ms', result)


class RobohornetPro(test.Test):
  test = RobohornetProMeasurement

  def CreatePageSet(self, options):
    return page_set.PageSet.FromDict({
        'archive_data_file': '../data/robohornetpro.json',
        # Measurement require use of real Date.now() for measurement.
        'make_javascript_deterministic': False,
        'pages': [
          { 'url':
            'http://ie.microsoft.com/testdrive/performance/robohornetpro/' }
          ]
        }, os.path.abspath(__file__))
