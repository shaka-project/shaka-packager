#!/usr/bin/env python
# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for coverage_posix.py.

Run a single test with a command such as:
  ./coverage_posix_unittest.py CoveragePosixTest.testFindTestsAsArgs

Waring that running a single test like that may interfere with the arg
parsing tests, since coverage_posix.py uses optparse.OptionParser()
which references globals.
"""

import coverage_posix as coverage
import os
import sys
import tempfile
import unittest

class CoveragePosixTest(unittest.TestCase):


  def setUp(self):
    self.parseArgs()
    self.sample_test_names = ['zippy_tests', '../base/base.gyp:base_unittests']

  def confirmSampleTestsArePresent(self, tests):
    """Confirm the tests in self.sample_test_names are in some form in 'tests'.

    The Coverage object can munge them (e.g. add .exe to the end as needed.
    Helper function for arg parsing, bundle file tests.

    Args:
      tests: the parsed tests from a Coverage object.
    """
    for simple_test_name in ('zippy_tests', 'base_unittests'):
      found = False
      for item in tests:
        if simple_test_name in item:
          found = True
          break
      self.assertTrue(found)
    for not_test_name in ('kablammo', 'not_a_unittest'):
      found = False
      for item in tests:
        if not_test_name in item:
          found = True
          break
      self.assertFalse(found)

  def parseArgs(self):
    """Setup and process arg parsing."""
    self.parser = coverage.CoverageOptionParser()
    (self.options, self.args) = self.parser.parse_args()
    self.options.directory = '.'

  def testSanity(self):
    """Sanity check we're able to actually run the tests.

    Simply creating a Coverage instance checks a few things (e.g. on
    Windows that the coverage tools can be found)."""
    c = coverage.Coverage(self.options, self.args)

  def testRunBasicProcess(self):
    """Test a simple run of a subprocess."""
    c = coverage.Coverage(self.options, self.args)
    for code in range(2):
      retcode = c.Run([sys.executable, '-u', '-c',
                       'import sys; sys.exit(%d)' % code],
                      ignore_error=True)
      self.assertEqual(code, retcode)

  def testRunSlowProcess(self):
    """Test program which prints slowly but doesn't hit our timeout.

    Overall runtime is longer than the timeout but output lines
    trickle in keeping things alive.
    """
    self.options.timeout = 2.5
    c = coverage.Coverage(self.options, self.args)
    slowscript = ('import sys, time\n'
                  'for x in range(10):\n'
                  '  time.sleep(0.5)\n'
                  '  print "hi mom"\n'
                  'sys.exit(0)\n')
    retcode = c.Run([sys.executable, '-u', '-c', slowscript])
    self.assertEqual(0, retcode)

  def testRunExcessivelySlowProcess(self):
    """Test program which DOES hit our timeout.

    Initial lines should print but quickly it takes too long and
    should be killed.
    """
    self.options.timeout = 2.5
    c = coverage.Coverage(self.options, self.args)
    slowscript = ('import time\n'
                  'for x in range(1,10):\n'
                  '  print "sleeping for %d" % x\n'
                  '  time.sleep(x)\n')
    self.assertRaises(Exception,
                      c.Run,
                      [sys.executable, '-u', '-c', slowscript])

  def testFindTestsAsArgs(self):
    """Test finding of tests passed as args."""
    self.args += '--'
    self.args += self.sample_test_names
    c = coverage.Coverage(self.options, self.args)
    c.FindTests()
    self.confirmSampleTestsArePresent(c.tests)

  def testFindTestsFromBundleFile(self):
    """Test finding of tests from a bundlefile."""
    (fd, filename) = tempfile.mkstemp()
    f = os.fdopen(fd, 'w')
    f.write(str(self.sample_test_names))
    f.close()
    self.options.bundles = filename
    c = coverage.Coverage(self.options, self.args)
    c.FindTests()
    self.confirmSampleTestsArePresent(c.tests)
    os.unlink(filename)

  def testExclusionList(self):
    """Test the gtest_filter exclusion list."""
    c = coverage.Coverage(self.options, self.args)
    self.assertFalse(c.GtestFilter('doesnotexist_test'))
    fake_exclusions = { sys.platform: { 'foobar':
                                        ('a','b'),
                                        'doesnotexist_test':
                                        ('Evil.Crash','Naughty.Test') } }
    self.assertFalse(c.GtestFilter('barfoo'))
    filter = c.GtestFilter('doesnotexist_test', fake_exclusions)
    self.assertEquals('--gtest_filter=-Evil.Crash:-Naughty.Test', filter)



if __name__ == '__main__':
  unittest.main()
