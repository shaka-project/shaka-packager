#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import logging
import os
import sys
import tempfile
import time

from telemetry import test
from telemetry.core import browser_options
from telemetry.core import discover
from telemetry.core import wpr_modes
from telemetry.page import page_measurement
from telemetry.page import page_runner
from telemetry.page import page_set
from telemetry.page import page_test
from telemetry.page import test_expectations


class RecordPage(page_test.PageTest):
  def __init__(self, measurements):
    # This class overwrites PageTest.Run, so that the test method name is not
    # really used (except for throwing an exception if it doesn't exist).
    super(RecordPage, self).__init__('Run')
    self._action_names = set(
        [measurement().action_name_to_run
         for measurement in measurements.values()
         if measurement().action_name_to_run])

  def CanRunForPage(self, page):
    return page.url.startswith('http')

  def CustomizeBrowserOptionsForPage(self, page, options):
    for compound_action in self._CompoundActionsForPage(page):
      for action in compound_action:
        action.CustomizeBrowserOptions(options)

  def Run(self, options, page, tab, results):
    # When recording, sleep to catch any resources that load post-onload.
    tab.WaitForDocumentReadyStateToBeComplete()
    time.sleep(3)

    # Run the actions for all measurements. Reload the page between
    # actions.
    should_reload = False
    for compound_action in self._CompoundActionsForPage(page):
      if should_reload:
        tab.Navigate(page.url)
        tab.WaitForDocumentReadyStateToBeComplete()
      self._RunCompoundAction(page, tab, compound_action)
      should_reload = True

  def _CompoundActionsForPage(self, page):
    actions = []
    for action_name in self._action_names:
      if not hasattr(page, action_name):
        continue
      actions.append(page_test.GetCompoundActionFromPage(page, action_name))
    return actions


def Main(base_dir):
  measurements = discover.DiscoverClasses(base_dir, base_dir,
                                          page_measurement.PageMeasurement)
  tests = discover.DiscoverClasses(base_dir, base_dir, test.Test,
                                   index_by_class_name=True)
  options = browser_options.BrowserOptions()
  parser = options.CreateParser('%prog <PageSet|Measurement|Test>')
  page_runner.AddCommandLineOptions(parser)

  recorder = RecordPage(measurements)
  recorder.AddCommandLineOptions(parser)
  recorder.AddOutputOptions(parser)

  _, args = parser.parse_args()

  if len(args) != 1:
    parser.print_usage()
    sys.exit(1)

  if args[0].endswith('.json'):
    ps = page_set.PageSet.FromFile(args[0])
  elif args[0] in tests:
    ps = tests[args[0]]().CreatePageSet(options)
  elif args[0] in measurements:
    ps = measurements[args[0]]().CreatePageSet(args, options)
  else:
    parser.print_usage()
    sys.exit(1)

  expectations = test_expectations.TestExpectations()

  # Set the archive path to something temporary.
  temp_target_wpr_file_path = tempfile.mkstemp()[1]
  ps.wpr_archive_info.AddNewTemporaryRecording(temp_target_wpr_file_path)

  # Do the actual recording.
  options.wpr_mode = wpr_modes.WPR_RECORD
  options.no_proxy_server = True
  recorder.CustomizeBrowserOptions(options)
  results = page_runner.Run(recorder, ps, expectations, options)

  if results.errors or results.failures:
    logging.warning('Some pages failed. The recording has not been updated for '
                    'these pages.')
    logging.warning('Failed pages:\n%s',
                    '\n'.join(zip(*results.errors + results.failures)[0]))

  if results.skipped:
    logging.warning('Some pages were skipped. The recording has not been '
                    'updated for these pages.')
    logging.warning('Skipped pages:\n%s', '\n'.join(zip(*results.skipped)[0]))

  if results.successes:
    # Update the metadata for the pages which were recorded.
    ps.wpr_archive_info.AddRecordedPages(results.successes)
  else:
    os.remove(temp_target_wpr_file_path)

  return min(255, len(results.failures))
