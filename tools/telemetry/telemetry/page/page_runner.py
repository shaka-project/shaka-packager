# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import collections
import glob
import logging
import os
import sys
import tempfile
import time
import traceback
import random

from telemetry.core import browser_finder
from telemetry.core import exceptions
from telemetry.core import util
from telemetry.core import wpr_modes
from telemetry.core.platform.profiler import profiler_finder
from telemetry.page import page_filter as page_filter_module
from telemetry.page import page_measurement_results
from telemetry.page import page_runner_repeat
from telemetry.page import page_test


class _RunState(object):
  def __init__(self):
    self.browser = None
    self.tab = None

    self._append_to_existing_wpr = False
    self._last_archive_path = None
    self._first_browser = True
    self.first_page = collections.defaultdict(lambda: True)
    self.profiler_dir = None
    self.repeat_state = None

  def StartBrowser(self, test, page_set, page, possible_browser,
                   credentials_path, archive_path):
    # Create a browser.
    if not self.browser:
      assert not self.tab
      self.browser = possible_browser.Create()
      self.browser.credentials.credentials_path = credentials_path

      test.WillStartBrowser(self.browser)
      self.browser.Start()
      test.DidStartBrowser(self.browser)

      if self._first_browser:
        self._first_browser = False
        self.browser.credentials.WarnIfMissingCredentials(page_set)

      # Set up WPR path on the new browser.
      self.browser.SetReplayArchivePath(archive_path,
                                        self._append_to_existing_wpr,
                                        page_set.make_javascript_deterministic)
      self._last_archive_path = page.archive_path
    else:
      # Set up WPR path if it changed.
      if page.archive_path and self._last_archive_path != page.archive_path:
        self.browser.SetReplayArchivePath(
            page.archive_path,
            self._append_to_existing_wpr,
            page_set.make_javascript_deterministic)
        self._last_archive_path = page.archive_path

    if self.browser.supports_tab_control:
      # Create a tab if there's none.
      if len(self.browser.tabs) == 0:
        self.browser.tabs.New()

      # Ensure only one tab is open.
      while len(self.browser.tabs) > 1:
        self.browser.tabs[-1].Close()

    if not self.tab:
      self.tab = self.browser.tabs[0]

    if self.first_page[page]:
      self.first_page[page] = False

  def StopBrowser(self):
    if self.tab:
      self.tab.Disconnect()
      self.tab = None

    if self.browser:
      self.browser.Close()
      self.browser = None

      # Restarting the state will also restart the wpr server. If we're
      # recording, we need to continue adding into the same wpr archive,
      # not overwrite it.
      self._append_to_existing_wpr = True

  def StartProfiling(self, page, options):
    if not self.profiler_dir:
      self.profiler_dir = tempfile.mkdtemp()
    output_file = os.path.join(self.profiler_dir, page.url_as_file_safe_name)
    if options.repeat_options.IsRepeating():
      output_file = _GetSequentialFileName(output_file)
    self.browser.StartProfiling(options.profiler, output_file)

  def StopProfiling(self):
    self.browser.StopProfiling()


class PageState(object):
  def __init__(self):
    self._did_login = False

  def PreparePage(self, page, tab, test=None):
    if page.is_file:
      serving_dirs = page.serving_dirs_and_file[0]
      if tab.browser.SetHTTPServerDirectories(serving_dirs) and test:
        test.DidStartHTTPServer(tab)

    if page.credentials:
      if not tab.browser.credentials.LoginNeeded(tab, page.credentials):
        raise page_test.Failure('Login as ' + page.credentials + ' failed')
      self._did_login = True

    if test:
      if test.clear_cache_before_each_run:
        tab.ClearCache()

  def ImplicitPageNavigation(self, page, tab, test=None):
    """Executes the implicit navigation that occurs for every page iteration.

    This function will be called once per page before any actions are executed.
    """
    if page.is_file:
      filename = page.serving_dirs_and_file[1]
      target_side_url = tab.browser.http_server.UrlOf(filename)
    else:
      target_side_url = page.url

    if test:
      test.WillNavigateToPage(page, tab)
    tab.Navigate(target_side_url, page.script_to_evaluate_on_commit)
    if test:
      test.DidNavigateToPage(page, tab)

    page.WaitToLoad(tab, 60)
    tab.WaitForDocumentReadyStateToBeInteractiveOrBetter()

  def CleanUpPage(self, page, tab):
    if page.credentials and self._did_login:
      tab.browser.credentials.LoginNoLongerNeeded(tab, page.credentials)


def AddCommandLineOptions(parser):
  page_filter_module.PageFilter.AddCommandLineOptions(parser)


def _LogStackTrace(title, browser):
  if browser:
    stack_trace = browser.GetStackTrace()
  else:
    stack_trace = 'Browser object is empty, no stack trace.'
  stack_trace = (('\nStack Trace:\n') +
            ('*' * 80) +
            '\n\t' + stack_trace.replace('\n', '\n\t') + '\n' +
            ('*' * 80))
  logging.warning('%s%s', title, stack_trace)


def _PrepareAndRunPage(test, page_set, expectations, options, page,
                       credentials_path, possible_browser, results, state):
  if options.wpr_mode != wpr_modes.WPR_RECORD:
    if page.archive_path and os.path.isfile(page.archive_path):
      possible_browser.options.wpr_mode = wpr_modes.WPR_REPLAY
    else:
      possible_browser.options.wpr_mode = wpr_modes.WPR_OFF
  results_for_current_run = results
  if state.first_page[page] and test.discard_first_result:
    # If discarding results, substitute a dummy object.
    results_for_current_run = page_measurement_results.PageMeasurementResults()
  results_for_current_run.StartTest(page)
  tries = 3
  while tries:
    try:
      state.StartBrowser(test, page_set, page, possible_browser,
                         credentials_path, page.archive_path)

      _WaitForThermalThrottlingIfNeeded(state.browser.platform)

      if options.profiler:
        state.StartProfiling(page, options)

      expectation = expectations.GetExpectationForPage(
          state.browser.platform, page)

      try:
        _RunPage(test, page, state, expectation,
                 results_for_current_run, options)
        _CheckThermalThrottling(state.browser.platform)
      except exceptions.TabCrashException:
        _LogStackTrace('Tab crashed: %s' % page.url, state.browser)
        state.StopBrowser()

      if options.profiler:
        state.StopProfiling()

      if test.NeedsBrowserRestartAfterEachRun(state.tab):
        state.StopBrowser()

      break
    except exceptions.BrowserGoneException:
      _LogStackTrace('Browser crashed', state.browser)
      logging.warning('Lost connection to browser. Retrying.')
      state.StopBrowser()
      tries -= 1
      if not tries:
        logging.error('Lost connection to browser 3 times. Failing.')
        raise
  results_for_current_run.StopTest(page)


def Run(test, page_set, expectations, options):
  """Runs a given test against a given page_set with the given options."""
  results = test.PrepareResults(options)

  # Create a possible_browser with the given options.
  test.CustomizeBrowserOptions(options)
  if options.profiler:
    profiler_class = profiler_finder.FindProfiler(options.profiler)
    profiler_class.CustomizeBrowserOptions(options)
  try:
    possible_browser = browser_finder.FindBrowser(options)
  except browser_finder.BrowserTypeRequiredException, e:
    sys.stderr.write(str(e) + '\n')
    sys.exit(1)
  if not possible_browser:
    sys.stderr.write(
        'No browser found. Available browsers:\n' +
        '\n'.join(browser_finder.GetAllAvailableBrowserTypes(options)) + '\n')
    sys.exit(1)

  # Reorder page set based on options.
  pages = _ShuffleAndFilterPageSet(page_set, options)

  if (not options.allow_live_sites and
      options.wpr_mode != wpr_modes.WPR_RECORD):
    pages = _CheckArchives(page_set, pages, results)

  # Verify credentials path.
  credentials_path = None
  if page_set.credentials_path:
    credentials_path = os.path.join(os.path.dirname(page_set.file_path),
                                    page_set.credentials_path)
    if not os.path.exists(credentials_path):
      credentials_path = None

  # Set up user agent.
  if page_set.user_agent_type:
    options.browser_user_agent_type = page_set.user_agent_type

  for page in pages:
    test.CustomizeBrowserOptionsForPage(page, possible_browser.options)

  for page in list(pages):
    if not test.CanRunForPage(page):
      logging.warning('Skipping test: it cannot run for %s', page.url)
      results.AddSkip(page, 'Test cannot run')
      pages.remove(page)

  if not pages:
    return results

  state = _RunState()
  # TODO(dtu): Move results creation and results_for_current_run into RunState.

  try:
    test.WillRunTest(state.tab)
    state.repeat_state = page_runner_repeat.PageRunnerRepeatState(
                             options.repeat_options)

    state.repeat_state.WillRunPageSet()
    while state.repeat_state.ShouldRepeatPageSet():
      for page in pages:
        state.repeat_state.WillRunPage()
        while state.repeat_state.ShouldRepeatPage():
          # execute test on page
          _PrepareAndRunPage(test, page_set, expectations, options, page,
                             credentials_path, possible_browser, results, state)
          state.repeat_state.DidRunPage()
      state.repeat_state.DidRunPageSet()

    test.DidRunTest(state.tab, results)
  finally:
    state.StopBrowser()

  return results


def _ShuffleAndFilterPageSet(page_set, options):
  if options.pageset_shuffle_order_file and not options.pageset_shuffle:
    raise Exception('--pageset-shuffle-order-file requires --pageset-shuffle.')

  if options.pageset_shuffle_order_file:
    return page_set.ReorderPageSet(options.pageset_shuffle_order_file)

  page_filter = page_filter_module.PageFilter(options)
  pages = [page for page in page_set.pages[:]
           if not page.disabled and page_filter.IsSelected(page)]

  if options.pageset_shuffle:
    random.Random().shuffle(pages)

  return pages


def _CheckArchives(page_set, pages, results):
  """Returns a subset of pages that are local or have WPR archives.

  Logs warnings if any are missing."""
  page_set_has_live_sites = False
  for page in pages:
    if not page.is_local:
      page_set_has_live_sites = True
      break

  # Potential problems with the entire page set.
  if page_set_has_live_sites:
    if not page_set.archive_data_file:
      logging.warning('The page set is missing an "archive_data_file" '
                      'property. Skipping any live sites. To include them, '
                      'pass the flag --allow-live-sites.')
    if not page_set.wpr_archive_info:
      logging.warning('The archive info file is missing. '
                      'To fix this, either add svn-internal to your '
                      '.gclient using http://goto/read-src-internal, '
                      'or create a new archive using record_wpr.')

  # Potential problems with individual pages.
  pages_missing_archive_path = []
  pages_missing_archive_data = []

  for page in pages:
    if page.is_local:
      continue

    if not page.archive_path:
      pages_missing_archive_path.append(page)
    elif not os.path.isfile(page.archive_path):
      pages_missing_archive_data.append(page)

  if pages_missing_archive_path:
    logging.warning('The page set archives for some pages do not exist. '
                    'Skipping those pages. To fix this, record those pages '
                    'using record_wpr. To ignore this warning and run '
                    'against live sites, pass the flag --allow-live-sites.')
  if pages_missing_archive_data:
    logging.warning('The page set archives for some pages are missing. '
                    'Someone forgot to check them in, or they were deleted. '
                    'Skipping those pages. To fix this, record those pages '
                    'using record_wpr. To ignore this warning and run '
                    'against live sites, pass the flag --allow-live-sites.')

  for page in pages_missing_archive_path + pages_missing_archive_data:
    results.StartTest(page)
    results.AddErrorMessage(page, 'Page set archive doesn\'t exist.')
    results.StopTest(page)

  return [page for page in pages if page not in
          pages_missing_archive_path + pages_missing_archive_data]


def _RunPage(test, page, state, expectation, results, options):
  logging.info('Running %s' % page.url)

  page_state = PageState()
  tab = state.tab

  def ProcessError():
    logging.error('%s:\n%s', page.url, traceback.format_exc())
    if expectation == 'fail':
      logging.info('Error was expected\n')
      results.AddSuccess(page)
    else:
      results.AddError(page, sys.exc_info())

  try:
    page_state.PreparePage(page, tab, test)
    if state.repeat_state.ShouldNavigate(options.skip_navigate_on_repeat):
      page_state.ImplicitPageNavigation(page, tab, test)
    test.Run(options, page, tab, results)
    util.CloseConnections(tab)
  except page_test.Failure:
    logging.warning('%s:\n%s', page.url, traceback.format_exc())
    if expectation == 'fail':
      logging.info('Failure was expected\n')
      results.AddSuccess(page)
    else:
      results.AddFailure(page, sys.exc_info())
  except (util.TimeoutException, exceptions.LoginException,
          exceptions.ProfilingException):
    ProcessError()
  except (exceptions.TabCrashException, exceptions.BrowserGoneException):
    ProcessError()
    # Run() catches these exceptions to relaunch the tab/browser, so re-raise.
    raise
  except Exception:
    raise
  else:
    if expectation == 'fail':
      logging.warning('%s was expected to fail, but passed.\n', page.url)
    results.AddSuccess(page)
  finally:
    page_state.CleanUpPage(page, tab)


def _GetSequentialFileName(base_name):
  """Returns the next sequential file name based on |base_name| and the
  existing files."""
  index = 0
  while True:
    output_name = '%s_%03d' % (base_name, index)
    if not glob.glob(output_name + '.*'):
      break
    index = index + 1
  return output_name


def _WaitForThermalThrottlingIfNeeded(platform):
  if not platform.CanMonitorThermalThrottling():
    return
  thermal_throttling_retry = 0
  while (platform.IsThermallyThrottled() and
         thermal_throttling_retry < 3):
    logging.warning('Thermally throttled, waiting (%d)...',
                    thermal_throttling_retry)
    thermal_throttling_retry += 1
    time.sleep(thermal_throttling_retry * 2)

  if thermal_throttling_retry and platform.IsThermallyThrottled():
    logging.error('Device is thermally throttled before running '
                  'performance tests, results will vary.')


def _CheckThermalThrottling(platform):
  if not platform.CanMonitorThermalThrottling():
    return
  if platform.HasBeenThermallyThrottled():
    logging.error('Device has been thermally throttled during '
                  'performance tests, results will vary.')
