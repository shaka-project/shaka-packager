# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json

from telemetry.page import page_measurement

class Startup(page_measurement.PageMeasurement):
  """Performs a measurement of Chromium's startup performance.

  This test must be invoked with either --warm or --cold on the command line. A
  cold start means none of the Chromium files are in the disk cache. A warm
  start assumes the OS has already cached much of Chromium's content. For warm
  tests, you should repeat the page set to ensure it's cached.
  """

  HISTOGRAMS_TO_RECORD = {
    'messageloop_start_time' :
        'Startup.BrowserMessageLoopStartTimeFromMainEntry',
    'window_display_time' : 'Startup.BrowserWindowDisplay',
    'open_tabs_time' : 'Startup.BrowserOpenTabs'}

  def __init__(self):
    super(Startup, self).__init__(needs_browser_restart_after_each_run=True)
    self._cold = False

  def AddCommandLineOptions(self, parser):
    parser.add_option('--cold', action='store_true',
                      help='Clear the OS disk cache before performing the test')
    parser.add_option('--warm', action='store_true',
                      help='Start up with everything already cached')

  def CustomizeBrowserOptions(self, options):
    # TODO: Once the bots start running benchmarks, enforce that either --warm
    # or --cold is explicitly specified.
    # assert options.warm != options.cold, \
    #     "You must specify either --warm or --cold"
    self._cold = options.cold

    if self._cold:
      options.clear_sytem_cache_for_browser_and_profile_on_start = True
    else:
      self.discard_first_result = True

    options.AppendExtraBrowserArg('--enable-stats-collection-bindings')

    # Old commandline flags used for reference builds.
    options.AppendExtraBrowserArg('--dom-automation')
    options.AppendExtraBrowserArg(
          '--reduce-security-for-dom-automation-tests')

  def MeasurePage(self, page, tab, results):
    # TODO(jeremy): Remove references to
    # domAutomationController.getBrowserHistogram when we update the reference
    # builds.
    get_histogram_js = ('(window.statsCollectionController ?'
        'statsCollectionController :'
        'domAutomationController).getBrowserHistogram("%s")')

    for display_name, histogram_name in self.HISTOGRAMS_TO_RECORD.iteritems():
      result = tab.EvaluateJavaScript(get_histogram_js % histogram_name)
      result = json.loads(result)
      measured_time = 0

      if 'sum' in result:
        # For all the histograms logged here, there's a single entry so sum
        # is the exact value for that entry.
        measured_time = result['sum']
      elif 'buckets' in result:
        measured_time = \
            (result['buckets'][0]['high'] + result['buckets'][0]['low']) / 2

      results.Add(display_name, 'ms', measured_time)
