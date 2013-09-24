# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from metrics import loading
from metrics import timeline
from telemetry.core import util
from telemetry.page import page_measurement

class LoadingTrace(page_measurement.PageMeasurement):
  def __init__(self, *args, **kwargs):
    super(LoadingTrace, self).__init__(*args, **kwargs)
    self._metrics = None

  @property
  def results_are_the_same_on_every_page(self):
    return False

  def WillNavigateToPage(self, page, tab):
    self._metrics = timeline.TimelineMetrics(timeline.TRACING_MODE)
    self._metrics.Start(tab)

  def MeasurePage(self, page, tab, results):
    # In current telemetry tests, all tests wait for DocumentComplete state,
    # but we need to wait for the load event.
    def IsLoaded():
      return bool(tab.EvaluateJavaScript('performance.timing.loadEventStart'))
    util.WaitFor(IsLoaded, 300)

    # TODO(nduca): when crbug.com/168431 is fixed, modify the page sets to
    # recognize loading as a toplevel action.
    self._metrics.Stop(tab)

    loading.LoadingMetric().AddResults(tab, results)
    self._metrics.AddResults(results)
