# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import tempfile

from metrics import loading
from telemetry.core import util
from telemetry.core.platform.profiler import perf_profiler
from telemetry.page import page_measurement

class LoadingProfile(page_measurement.PageMeasurement):
  def __init__(self):
    super(LoadingProfile, self).__init__(discard_first_result=True)

    if not perf_profiler.PerfProfiler.is_supported(None):
      raise Exception('This measurement is not supported on this platform')

  @property
  def results_are_the_same_on_every_page(self):
    return False

  def AddCommandLineOptions(self, parser):
    # In order to change the default of an option, we must remove and re-add it.
    page_repeat_option = parser.get_option('--page-repeat')
    page_repeat_option.default = 2
    parser.remove_option('--page-repeat')
    parser.add_option(page_repeat_option)

  def CustomizeBrowserOptions(self, options):
    options.AppendExtraBrowserArg('--no-sandbox')
    perf_profiler.PerfProfiler.CustomizeBrowserOptions(options)

  def WillNavigateToPage(self, page, tab):
    tab.browser.StartProfiling(perf_profiler.PerfProfiler.name(),
                               os.path.join(tempfile.mkdtemp(),
                                            page.url_as_file_safe_name))

  def MeasurePage(self, page, tab, results):
    # In current telemetry tests, all tests wait for DocumentComplete state,
    # but we need to wait for the load event.
    def IsLoaded():
      return bool(tab.EvaluateJavaScript('performance.timing.loadEventStart'))
    util.WaitFor(IsLoaded, 300)

    profile_files = tab.browser.StopProfiling()

    loading.LoadingMetric().AddResults(tab, results)

    profile_file = None
    for profile_file in profile_files:
      if 'renderer' in profile_file:
        break

    for function, period in perf_profiler.PerfProfiler.GetTopSamples(
        profile_file, 10).iteritems():
      results.Add(function.replace('.', '_'), 'period', period)
