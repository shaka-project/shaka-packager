# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import sys
import traceback
import unittest

class PageTestResults(unittest.TestResult):
  def __init__(self):
    super(PageTestResults, self).__init__()
    self.successes = []
    self.skipped = []

  def _exc_info_to_string(self, err, test):
    if isinstance(test, unittest.TestCase):
      return super(PageTestResults, self)._exc_info_to_string(err, test)
    else:
      return ''.join(traceback.format_exception(*err))

  def addSuccess(self, test):
    self.successes.append(test)

  def addSkip(self, test, reason):  # Python 2.7 has this in unittest.TestResult
    self.skipped.append((test, reason))

  def StartTest(self, page):
    self.startTest(page.url)

  def StopTest(self, page):
    self.stopTest(page.url)

  def AddError(self, page, err):
    self.addError(page.url, err)

  def AddFailure(self, page, err):
    self.addFailure(page.url, err)

  def AddSuccess(self, page):
    self.addSuccess(page.url)

  def AddSkip(self, page, reason):
    self.addSkip(page.url, reason)

  def AddFailureMessage(self, page, message):
    try:
      raise Exception(message)
    except Exception:
      self.AddFailure(page, sys.exc_info())

  def AddErrorMessage(self, page, message):
    try:
      raise Exception(message)
    except Exception:
      self.AddError(page, sys.exc_info())

  def PrintSummary(self):
    if self.failures:
      logging.warning('Failed pages:\n%s', '\n'.join(zip(*self.failures)[0]))

    if self.errors:
      logging.warning('Errored pages:\n%s', '\n'.join(zip(*self.errors)[0]))

    if self.skipped:
      logging.warning('Skipped pages:\n%s', '\n'.join(zip(*self.skipped)[0]))
