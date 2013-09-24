#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
import unittest
import glob

from dart_generator import DartGenerator
from compiler import GenerateSchema

# If --rebase is passed to this test, this is set to True, indicating the test
# output should be re-generated for each test (rather than running the tests
# themselves).
REBASE_MODE = False

# The directory containing the input and expected output files corresponding
# to each test name.
TESTS_DIR = 'dart_test'

class DartTest(unittest.TestCase):

  def _RunTest(self, test_filename):
    '''Given the name of a test, runs compiler.py on the file:
      TESTS_DIR/test_filename.idl
    and compares it to the output in the file:
      TESTS_DIR/test_filename.dart
    '''
    file_rel = os.path.join(TESTS_DIR, test_filename)

    output_dir = None
    if REBASE_MODE:
      output_dir = TESTS_DIR
    output_code = GenerateSchema('dart', ['%s.idl' % file_rel], TESTS_DIR,
                                 output_dir, None, None)

    if not REBASE_MODE:
      with open('%s.dart' % file_rel) as f:
        expected_output = f.read()
      # Remove the first line of the output code (as it contains the filename).
      # Also remove all blank lines, ignoring them from the comparison.
      # Compare with lists instead of strings for clearer diffs (especially with
      # whitespace) when a test fails.
      self.assertEqual([l for l in expected_output.split('\n') if l],
                       [l for l in output_code.split('\n')[1:] if l])

  def setUp(self):
    # Increase the maximum diff amount to see the full diff on a failed test.
    self.maxDiff = 2000

  def testComments(self):
    self._RunTest('comments')

  def testDictionaries(self):
    self._RunTest('dictionaries')

  def testEmptyNamespace(self):
    self._RunTest('empty_namespace')

  def testEmptyType(self):
    self._RunTest('empty_type')

  def testEvents(self):
    self._RunTest('enums')

  def testEvents(self):
    self._RunTest('events')

  def testBasicFunction(self):
    self._RunTest('functions')

  def testOpratableType(self):
    self._RunTest('operatable_type')

  def testTags(self):
    self._RunTest('tags')

if __name__ == '__main__':
  if '--rebase' in sys.argv:
    print "Running in rebase mode."
    REBASE_MODE = True
    sys.argv.remove('--rebase')
  unittest.main()
