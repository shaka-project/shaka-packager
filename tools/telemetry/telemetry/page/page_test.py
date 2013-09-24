# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import logging

from telemetry.core import util
from telemetry.page import gtest_test_results
from telemetry.page import test_expectations
from telemetry.page import page_test_results
from telemetry.page.actions import all_page_actions
from telemetry.page.actions import page_action

def _GetActionFromData(action_data):
  action_name = action_data['action']
  action = all_page_actions.FindClassWithName(action_name)
  if not action:
    logging.critical('Could not find an action named %s.', action_name)
    logging.critical('Check the page set for a typo and check the error '
                     'log for possible Python loading/compilation errors.')
    raise Exception('Action "%s" not found.' % action_name)
  return action(action_data)

def GetCompoundActionFromPage(page, action_name):
  if not action_name:
    return []

  action_data_list = getattr(page, action_name)
  if not isinstance(action_data_list, list):
    action_data_list = [action_data_list]

  action_list = []
  for subaction_data in action_data_list:
    subaction_name = subaction_data['action']
    if hasattr(page, subaction_name):
      subaction = GetCompoundActionFromPage(page, subaction_name)
    else:
      subaction = [_GetActionFromData(subaction_data)]
    action_list += subaction * subaction_data.get('repeat', 1)
  return action_list

class Failure(Exception):
  """Exception that can be thrown from PageMeasurement to indicate an
  undesired but designed-for problem."""
  pass

class PageTest(object):
  """A class styled on unittest.TestCase for creating page-specific tests."""

  def __init__(self,
               test_method_name,
               action_name_to_run='',
               needs_browser_restart_after_each_run=False,
               discard_first_result=False,
               clear_cache_before_each_run=False):
    self.options = None
    try:
      self._test_method = getattr(self, test_method_name)
    except AttributeError:
      raise ValueError, 'No such method %s.%s' % (
        self.__class_, test_method_name) # pylint: disable=E1101
    self._action_name_to_run = action_name_to_run
    self._needs_browser_restart_after_each_run = (
        needs_browser_restart_after_each_run)
    self._discard_first_result = discard_first_result
    self._clear_cache_before_each_run = clear_cache_before_each_run

  @property
  def discard_first_result(self):
    """When set to True, the first run of the test is discarded.  This is
    useful for cases where it's desirable to have some test resource cached so
    the first run of the test can warm things up. """
    return self._discard_first_result

  @discard_first_result.setter
  def discard_first_result(self, discard):
    self._discard_first_result = discard

  @property
  def clear_cache_before_each_run(self):
    """When set to True, the browser's disk and memory cache will be cleared
    before each run."""
    return self._clear_cache_before_each_run

  def NeedsBrowserRestartAfterEachRun(self, tab): # pylint: disable=W0613
    """Override to specify browser restart after each run."""
    return self._needs_browser_restart_after_each_run

  def AddCommandLineOptions(self, parser):
    """Override to expose command-line options for this test.

    The provided parser is an optparse.OptionParser instance and accepts all
    normal results. The parsed options are available in Run as
    self.options."""
    pass

  def CustomizeBrowserOptions(self, options):
    """Override to add test-specific options to the BrowserOptions object"""
    pass

  def CustomizeBrowserOptionsForPage(self, page, options):
    """Add options specific to the test and the given page."""
    if not self.CanRunForPage(page):
      return
    for action in GetCompoundActionFromPage(page, self._action_name_to_run):
      action.CustomizeBrowserOptions(options)

  def WillStartBrowser(self, browser):
    """Override to manipulate the browser environment before it launches."""
    pass

  def DidStartBrowser(self, browser):
    """Override to customize the browser right after it has launched."""
    pass

  def CanRunForPage(self, page): #pylint: disable=W0613
    """Override to customize if the test can be ran for the given page."""
    return True

  def WillRunTest(self, tab):
    """Override to do operations before the page set(s) are navigated."""
    pass

  def DidRunTest(self, tab, results):
    """Override to do operations after all page set(s) are completed.

    This will occur before the browser is torn down.
    """
    pass

  def DidStartHTTPServer(self, tab):
    """Override to do operations after the HTTP server is started."""
    pass

  def WillNavigateToPage(self, page, tab):
    """Override to do operations before the page is navigated."""
    pass

  def DidNavigateToPage(self, page, tab):
    """Override to do operations right after the page is navigated, but before
    any waiting for completion has occurred."""
    pass

  def WillRunAction(self, page, tab, action):
    """Override to do operations before running the action on the page."""
    pass

  def DidRunAction(self, page, tab, action):
    """Override to do operations after running the action on the page."""
    pass

  def CreatePageSet(self, args, options):  # pylint: disable=W0613
    """Override to make this test generate its own page set instead of
    allowing arbitrary page sets entered from the command-line."""
    return None

  def CreateExpectations(self, page_set):  # pylint: disable=W0613
    """Override to make this test generate its own expectations instead of
    any that may have been defined in the page set."""
    return test_expectations.TestExpectations()

  def AddOutputOptions(self, parser):
    parser.add_option('--output-format',
                      default=self.output_format_choices[0],
                      choices=self.output_format_choices,
                      help='Output format. Defaults to "%%default". '
                      'Can be %s.' % ', '.join(self.output_format_choices))

  @property
  def output_format_choices(self):
    """Allowed output formats. The default is the first item in the list."""
    return ['gtest', 'none']

  def PrepareResults(self, options):
    if not hasattr(options, 'output_format'):
      options.output_format = self.output_format_choices[0]

    if options.output_format == 'gtest':
      return gtest_test_results.GTestTestResults()
    elif options.output_format == 'none':
      return page_test_results.PageTestResults()
    else:
      # Should never be reached. The parser enforces the choices.
      raise Exception('Invalid --output-format "%s". Valid choices are: %s'
                      % (options.output_format,
                         ', '.join(self.output_format_choices)))

  def Run(self, options, page, tab, results):
    self.options = options
    compound_action = GetCompoundActionFromPage(page, self._action_name_to_run)
    self._RunCompoundAction(page, tab, compound_action)
    try:
      self._test_method(page, tab, results)
    finally:
      self.options = None

  def _RunCompoundAction(self, page, tab, actions):
    for i, action in enumerate(actions):
      prev_action = actions[i - 1] if i > 0 else None
      next_action = actions[i + 1] if i < len(actions) - 1 else None

      if (action.RunsPreviousAction() and
          next_action and next_action.RunsPreviousAction()):
        raise page_action.PageActionFailed('Consecutive actions cannot both '
                                           'have RunsPreviousAction() == True.')

      if not (next_action and next_action.RunsPreviousAction()):
        action.WillRunAction(page, tab)
        self.WillRunAction(page, tab, action)
        try:
          action.RunAction(page, tab, prev_action)
        finally:
          self.DidRunAction(page, tab, action)

      # Closing the connections periodically is needed; otherwise we won't be
      # able to open enough sockets, and the pages will time out.
      util.CloseConnections(tab)

  @property
  def action_name_to_run(self):
    return self._action_name_to_run
