# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import logging
import os
import sys
import unittest

from telemetry.core import browser_options
from telemetry.core import discover
from telemetry.unittest import gtest_testrunner
from telemetry.unittest import options_for_unittests

def Discover(start_dir, top_level_dir=None, pattern='test*.py'):
  loader = unittest.defaultTestLoader
  loader.suiteClass = gtest_testrunner.GTestTestSuite
  subsuites = []

  modules = discover.DiscoverModules(start_dir, top_level_dir, pattern)
  for module in modules:
    if hasattr(module, 'suite'):
      new_suite = module.suite()
    else:
      new_suite = loader.loadTestsFromModule(module)
    if new_suite.countTestCases():
      subsuites.append(new_suite)
  return gtest_testrunner.GTestTestSuite(subsuites)


def FilterSuite(suite, predicate):
  new_suite = suite.__class__()
  for x in suite:
    if isinstance(x, unittest.TestSuite):
      subsuite = FilterSuite(x, predicate)
      if subsuite.countTestCases() == 0:
        continue

      new_suite.addTest(subsuite)
      continue

    assert isinstance(x, unittest.TestCase)
    if predicate(x):
      new_suite.addTest(x)

  return new_suite


def DiscoverAndRunTests(
    dir_name, args, top_level_dir,
    runner=None, run_disabled_tests=False):
  if not runner:
    runner = gtest_testrunner.GTestTestRunner(inner=True)

  suite = Discover(dir_name, top_level_dir, '*_unittest.py')

  def IsTestSelected(test):
    if len(args) != 0:
      found = False
      for name in args:
        if name in test.id():
          found = True
      if not found:
        return False

    if hasattr(test, '_testMethodName'):
      method = getattr(test, test._testMethodName) # pylint: disable=W0212
      if hasattr(method, '_requires_browser_types'):
        types = method._requires_browser_types # pylint: disable=W0212
        if options_for_unittests.GetBrowserType() not in types:
          logging.debug('Skipping test %s because it requires %s' %
                        (test.id(), types))
          return False
      if hasattr(method, '_disabled_test'):
        if not run_disabled_tests:
          return False

    return True

  filtered_suite = FilterSuite(suite, IsTestSelected)
  test_result = runner.run(filtered_suite)
  return test_result


def Main(args, start_dir, top_level_dir, runner=None):
  """Unit test suite that collects all test cases for telemetry."""
  # Add unittest_data to the path so we can import packages from it.
  unittest_data_dir = os.path.abspath(os.path.join(
        os.path.dirname(__file__), '..', '..', 'unittest_data'))
  sys.path.append(unittest_data_dir)

  default_options = browser_options.BrowserOptions()
  default_options.browser_type = 'any'

  parser = default_options.CreateParser('run_tests [options] [test names]')
  parser.add_option('--repeat-count', dest='run_test_repeat_count',
                    type='int', default=1,
                    help='Repeats each a provided number of times.')
  parser.add_option('-d', '--also-run-disabled-tests',
                    dest='run_disabled_tests',
                    action='store_true', default=False,
                    help='Also run tests decorated with @DisabledTest.')

  _, args = parser.parse_args(args)

  logging_level = logging.getLogger().getEffectiveLevel()
  if default_options.verbosity == 0:
    logging.getLogger().setLevel(logging.WARN)

  from telemetry.core import browser_finder
  try:
    browser_to_create = browser_finder.FindBrowser(default_options)
  except browser_finder.BrowserFinderException, ex:
    logging.error(str(ex))
    return 1

  if browser_to_create == None:
    logging.error('No browser found of type %s. Cannot run tests.',
                  default_options.browser_type)
    logging.error('Re-run with --browser=list to see available browser types.')
    return 1

  options_for_unittests.Set(default_options,
                            browser_to_create.browser_type)
  olddir = os.getcwd()
  try:
    os.chdir(top_level_dir)
    success = True
    for _ in range(
        default_options.run_test_repeat_count): # pylint: disable=E1101
      success = success and DiscoverAndRunTests(
        start_dir, args, top_level_dir,
        runner, default_options.run_disabled_tests)
    if success:
      return 0
  finally:
    os.chdir(olddir)
    options_for_unittests.Set(None, None)
    if default_options.verbosity == 0:
      # Restore logging level.
      logging.getLogger().setLevel(logging_level)

  return 1
