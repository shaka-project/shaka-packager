# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""The page cycler measurement.

This measurement registers a window load handler in which is forces a layout and
then records the value of performance.now(). This call to now() measures the
time from navigationStart (immediately after the previous page's beforeunload
event) until after the layout in the page's load event. In addition, two garbage
collections are performed in between the page loads (in the beforeunload event).
This extra garbage collection time is not included in the measurement times.

Finally, various memory and IO statistics are gathered at the very end of
cycling all pages.
"""

import os
import sys

from metrics import histogram
from metrics import io
from metrics import memory
from telemetry.core import util
from telemetry.page import page_measurement


MEMORY_HISTOGRAMS = [
    {'name': 'V8.MemoryExternalFragmentationTotal', 'units': 'percent'},
    {'name': 'V8.MemoryHeapSampleTotalCommitted', 'units': 'kb'},
    {'name': 'V8.MemoryHeapSampleTotalUsed', 'units': 'kb'}]


class PageCycler(page_measurement.PageMeasurement):
  def __init__(self, *args, **kwargs):
    super(PageCycler, self).__init__(*args, **kwargs)

    with open(os.path.join(os.path.dirname(__file__),
                           'page_cycler.js'), 'r') as f:
      self._page_cycler_js = f.read()

    self._memory_metric = None
    self._histograms = None

  def AddCommandLineOptions(self, parser):
    # The page cyclers should default to 10 iterations. In order to change the
    # default of an option, we must remove and re-add it.
    # TODO: Remove this after transition to run_benchmark.
    pageset_repeat_option = parser.get_option('--pageset-repeat')
    pageset_repeat_option.default = 10
    parser.remove_option('--pageset-repeat')
    parser.add_option(pageset_repeat_option)

  def DidStartBrowser(self, browser):
    """Initialize metrics once right after the browser has been launched."""
    self._memory_metric = memory.MemoryMetric(browser)
    self._memory_metric.Start()
    self._histograms = [histogram.HistogramMetric(
                           h, histogram.RENDERER_HISTOGRAM)
                       for h in MEMORY_HISTOGRAMS]

  def DidStartHTTPServer(self, tab):
    # Avoid paying for a cross-renderer navigation on the first page on legacy
    # page cyclers which use the filesystem.
    tab.Navigate(tab.browser.http_server.UrlOf('nonexistent.html'))

  def WillNavigateToPage(self, page, tab):
    page.script_to_evaluate_on_commit = self._page_cycler_js

  def DidNavigateToPage(self, page, tab):
    for h in self._histograms:
      h.Start(page, tab)

  def CustomizeBrowserOptions(self, options):
    options.AppendExtraBrowserArg('--enable-stats-collection-bindings')
    options.AppendExtraBrowserArg('--js-flags=--expose_gc')
    options.AppendExtraBrowserArg('--no-sandbox')

    # Old commandline flags used for reference builds.
    options.AppendExtraBrowserArg('--dom-automation')

    # Temporarily disable typical_25 page set on mac.
    if sys.platform == 'darwin' and sys.argv[-1].endswith('/typical_25.json'):
      print 'typical_25 is currently disabled on mac. Skipping test.'
      sys.exit(0)


  def MeasurePage(self, page, tab, results):
    def _IsDone():
      return bool(tab.EvaluateJavaScript('__pc_load_time'))
    util.WaitFor(_IsDone, 60)

    for h in self._histograms:
      h.GetValue(page, tab, results)

    results.Add('page_load_time', 'ms',
                int(float(tab.EvaluateJavaScript('__pc_load_time'))),
                chart_name='times')

  def DidRunTest(self, tab, results):
    self._memory_metric.Stop()
    self._memory_metric.AddResults(tab, results)
    io.IOMetric().AddSummaryResults(tab, results)

