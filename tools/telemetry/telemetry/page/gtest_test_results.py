# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys
import time
import unittest

from telemetry.page import page_test_results

class GTestTestResults(page_test_results.PageTestResults):
  def __init__(self):
    super(GTestTestResults, self).__init__()
    self.timestamp = None
    self.num_successes = 0

  def _GetMs(self):
    return (time.time() - self.timestamp) * 1000

  @property
  def num_errors(self):
    return len(self.errors) + len(self.failures)

  @staticmethod
  def _formatTestname(test):
    if isinstance(test, unittest.TestCase):
      chunks = test.id().split('.')[-2:]
      return '.'.join(chunks)
    else:
      return str(test)

  def _emitFailure(self, test, err):
    print self._exc_info_to_string(err, test)
    test_name = GTestTestResults._formatTestname(test)
    print '[  FAILED  ]', test_name, '(%0.f ms)' % self._GetMs()
    sys.stdout.flush()

  def addError(self, test, err):
    self._emitFailure(test, err)
    super(GTestTestResults, self).addError(test, err)

  def addFailure(self, test, err):
    self._emitFailure(test, err)
    super(GTestTestResults, self).addFailure(test, err)

  def startTest(self, test):
    print '[ RUN      ]', GTestTestResults._formatTestname(test)
    sys.stdout.flush()
    self.timestamp = time.time()
    super(GTestTestResults, self).startTest(test)

  def addSuccess(self, test):
    self.num_successes = self.num_successes + 1
    test_name = GTestTestResults._formatTestname(test)
    print '[       OK ]', test_name, '(%0.f ms)' % self._GetMs()
    sys.stdout.flush()
    super(GTestTestResults, self).addSuccess(test)

  def PrintSummary(self):
    unit = 'test' if self.num_successes == 1 else 'tests'
    print '[  PASSED  ] %d %s.' % (self.num_successes, unit)
    if self.errors or self.failures:
      all_errors = self.errors[:]
      all_errors.extend(self.failures)
      unit = 'test' if len(all_errors) == 1 else 'tests'
      print '[  FAILED  ] %d %s, listed below:' % (len(all_errors), unit)
      for test, _ in all_errors:
        print '[  FAILED  ] ', GTestTestResults._formatTestname(test)
    if not self.wasSuccessful():
      print
      count = len(self.errors) + len(self.failures)
      unit = 'TEST' if count == 1 else 'TESTS'
      print '%d FAILED %s' % (count, unit)
    print
    sys.stdout.flush()
