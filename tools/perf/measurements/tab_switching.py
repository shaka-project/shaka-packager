# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""The tab switching measurement.

This measurement opens pages in different tabs. After all the tabs have opened,
it cycles through each tab in sequence, and records a histogram of the time
between when a tab was first requested to be shown, and when it was painted.
"""

from metrics import histogram_util
from telemetry.core import util
from telemetry.page import page_measurement
from telemetry.page import page_runner

# TODO: Revisit this test once multitab support is finalized.

class TabSwitching(page_measurement.PageMeasurement):
  def CustomizeBrowserOptions(self, options):
    options.AppendExtraBrowserArg('--enable-stats-collection-bindings')
    options.AppendExtraBrowserArg('--dom-automation')

  def CanRunForPage(self, page):
    return not page.page_set.pages.index(page)

  def DidNavigateToPage(self, page, tab):
    for i in xrange(1, len(page.page_set.pages)):
      t = tab.browser.tabs.New()

      page_state = page_runner.PageState()
      page_state.PreparePage(page.page_set.pages[i], t)

  def MeasurePage(self, _, tab, results):
    """Although this is called MeasurePage, we're actually using this function
    to cycle through each tab that was opened via DidNavigateToPage and
    thenrecord a single histogram for the tab switching metric.
    """
    histogram_name = 'MPArch.RWH_TabSwitchPaintDuration'
    histogram_type = 'getBrowserHistogram'
    first_histogram = histogram_util.GetHistogramFromDomAutomation(
        histogram_type, histogram_name, tab)
    prev_histogram = first_histogram

    for i in xrange(len(tab.browser.tabs)):
      t = tab.browser.tabs[i]
      t.Activate()
      def _IsDone():
        cur_histogram = histogram_util.GetHistogramFromDomAutomation(
            histogram_type, histogram_name, tab)
        diff_histogram = histogram_util.SubtractHistogram(
            cur_histogram, prev_histogram)
        return diff_histogram
      util.WaitFor(_IsDone, 30)
      prev_histogram = histogram_util.GetHistogramFromDomAutomation(
          histogram_type, histogram_name, tab)

    last_histogram = histogram_util.GetHistogramFromDomAutomation(
        histogram_type, histogram_name, tab)
    diff_histogram = histogram_util.SubtractHistogram(last_histogram,
        first_histogram)

    results.AddSummary(histogram_name, '', diff_histogram,
        data_type='unimportant-histogram')
