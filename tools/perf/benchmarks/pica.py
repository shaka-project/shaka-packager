# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from telemetry import test
from telemetry.page import page_measurement

class PicaMeasurement(page_measurement.PageMeasurement):
  def CustomizeBrowserOptions(self, options):
    # Needed for native custom elements (document.register)
    options.AppendExtraBrowserArg('--enable-experimental-web-platform-features')

  def MeasurePage(self, _, tab, results):
    result = int(tab.EvaluateJavaScript('__pica_load_time'))
    results.Add('Total', 'ms', result)


class Pica(test.Test):
  test = PicaMeasurement
  page_set = 'page_sets/pica.json'
