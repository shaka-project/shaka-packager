# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from metrics import histogram_util

BROWSER_HISTOGRAM = 'browser_histogram'
RENDERER_HISTOGRAM = 'renderer_histogram'

class HistogramMetric(object):
  def __init__(self, histogram, histogram_type):
    self.name = histogram['name']
    self.units = histogram['units']
    self.histogram_type = histogram_type
    self._start_values = dict()

  def Start(self, page, tab):
    """Get the starting value for the histogram. This value will then be
    subtracted from the actual measurement."""
    data = self._GetHistogramFromDomAutomation(tab)
    if data:
      self._start_values[page.url + self.name] = data

  def GetValue(self, page, tab, results):
    data = self._GetHistogramFromDomAutomation(tab)
    if not data:
      return
    new_histogram = histogram_util.SubtractHistogram(
        data, self._start_values[page.url + self.name])
    results.Add(self.name, self.units, new_histogram,
                data_type='unimportant-histogram')

  @property
  def histogram_function(self):
    if self.histogram_type == BROWSER_HISTOGRAM:
      return 'getBrowserHistogram'
    return 'getHistogram'

  def _GetHistogramFromDomAutomation(self, tab):
    return histogram_util.GetHistogramFromDomAutomation(
        self.histogram_function, self.name, tab)
