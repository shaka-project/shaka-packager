#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for croc_scan.py."""

import re
import unittest
import croc_scan


class TestScanner(unittest.TestCase):
  """Tests for croc_scan.Scanner."""

  def testInit(self):
    """Test __init()__."""
    s = croc_scan.Scanner()

    self.assertEqual(s.re_token.pattern, '#')
    self.assertEqual(s.comment_to_eol, ['#'])
    self.assertEqual(s.comment_start, None)
    self.assertEqual(s.comment_end, None)

  def testScanLines(self):
    """Test ScanLines()."""
    s = croc_scan.Scanner()
    # Set up imaginary language:
    #   ':' = comment to EOL
    #   '"' = string start/end
    #   '(' = comment start
    #   ')' = comment end
    s.re_token = re.compile(r'([\:\"\(\)])')
    s.comment_to_eol = [':']
    s.comment_start = '('
    s.comment_end = ')'

    # No input file = no output lines
    self.assertEqual(s.ScanLines([]), [])

    # Empty lines and lines with only whitespace are ignored
    self.assertEqual(s.ScanLines([
        '',             # 1
        'line',         # 2 exe
        ' \t  ',        # 3
    ]), [2])

    # Comments to EOL are stripped, but not inside strings
    self.assertEqual(s.ScanLines([
        'test',                                                 # 1 exe
        '   : A comment',                                       # 2
        '"a : in a string"',                                    # 3 exe
        'test2 : with comment to EOL',                          # 4 exe
        'foo = "a multiline string with an empty line',         # 5 exe
        '',                                                     # 6 exe
        ': and a comment-to-EOL character"',                    # 7 exe
        ': done',                                               # 8
    ]), [1, 3, 4, 5, 6, 7])

    # Test Comment start/stop detection
    self.assertEqual(s.ScanLines([
        '( a comment on one line)',                     # 1
        'text (with a comment)',                        # 2 exe
        '( a comment with a : in the middle)',          # 3
        '( a multi-line',                               # 4
        '  comment)',                                   # 5
        'a string "with a ( in it"',                    # 6 exe
        'not in a multi-line comment',                  # 7 exe
        '(a comment with a " in it)',                   # 8
        ': not in a string, so this gets stripped',     # 9
        'more text "with an uninteresting string"',     # 10 exe
    ]), [2, 6, 7, 10])

  # TODO: Test Scan().  Low priority, since it just wraps ScanLines().


class TestPythonScanner(unittest.TestCase):
  """Tests for croc_scan.PythonScanner."""

  def testScanLines(self):
    """Test ScanLines()."""
    s = croc_scan.PythonScanner()

    # No input file = no output lines
    self.assertEqual(s.ScanLines([]), [])

    self.assertEqual(s.ScanLines([
        '# a comment',                                  # 1
        '',                                             # 2
        '"""multi-line string',                         # 3 exe
        '# not a comment',                              # 4 exe
        'end of multi-line string"""',                  # 5 exe
        '  ',                                           # 6
        '"single string with #comment"',                # 7 exe
        '',                                             # 8
        '\'\'\'multi-line string, single-quote',        # 9 exe
        '# not a comment',                              # 10 exe
        'end of multi-line string\'\'\'',               # 11 exe
        '',                                             # 12
        '"string with embedded \\" is handled"',        # 13 exe
        '# quoted "',                                   # 14
        '"\\""',                                        # 15 exe
        '# quoted backslash',                           # 16
        '"\\\\"',                                       # 17 exe
        'main()',                                       # 18 exe
        '# end',                                        # 19
    ]), [3, 4, 5, 7, 9, 10, 11, 13, 15, 17, 18])


class TestCppScanner(unittest.TestCase):
  """Tests for croc_scan.CppScanner."""

  def testScanLines(self):
    """Test ScanLines()."""
    s = croc_scan.CppScanner()

    # No input file = no output lines
    self.assertEqual(s.ScanLines([]), [])

    self.assertEqual(s.ScanLines([
        '// a comment',                                 # 1
        '# a preprocessor define',                      # 2
        '',                                             # 3
        '\'#\', \'"\'',                                 # 4 exe
        '',                                             # 5
        '/* a multi-line comment',                      # 6
        'with a " in it',                               # 7
        '*/',                                           # 8
        '',                                             # 9
        '"a string with /* and \' in it"',              # 10 exe
        '',                                             # 11
        '"a multi-line string\\',                       # 12 exe
        '// not a comment\\',                           # 13 exe
        'ending here"',                                 # 14 exe
        '',                                             # 15
        '"string with embedded \\" is handled"',        # 16 exe
        '',                                             # 17
        'main()',                                       # 18 exe
        '// end',                                       # 19
    ]), [4, 10, 12, 13, 14, 16, 18])


class TestScanFile(unittest.TestCase):
  """Tests for croc_scan.ScanFile()."""

  class MockScanner(object):
    """Mock scanner."""

    def __init__(self, language):
      """Constructor."""
      self.language = language

    def Scan(self, filename):
      """Mock Scan() method."""
      return 'scan %s %s' % (self.language, filename)

  def MockPythonScanner(self):
    return self.MockScanner('py')

  def MockCppScanner(self):
    return self.MockScanner('cpp')

  def setUp(self):
    """Per-test setup."""
    # Hook scanners
    self.old_python_scanner = croc_scan.PythonScanner
    self.old_cpp_scanner = croc_scan.CppScanner
    croc_scan.PythonScanner = self.MockPythonScanner
    croc_scan.CppScanner = self.MockCppScanner

  def tearDown(self):
    """Per-test cleanup."""
    croc_scan.PythonScanner = self.old_python_scanner
    croc_scan.CppScanner = self.old_cpp_scanner

  def testScanFile(self):
    """Test ScanFile()."""
    self.assertEqual(croc_scan.ScanFile('foo', 'python'), 'scan py foo')
    self.assertEqual(croc_scan.ScanFile('bar1', 'C'), 'scan cpp bar1')
    self.assertEqual(croc_scan.ScanFile('bar2', 'C++'), 'scan cpp bar2')
    self.assertEqual(croc_scan.ScanFile('bar3', 'ObjC'), 'scan cpp bar3')
    self.assertEqual(croc_scan.ScanFile('bar4', 'ObjC++'), 'scan cpp bar4')
    self.assertEqual(croc_scan.ScanFile('bar', 'fortran'), [])


if __name__ == '__main__':
  unittest.main()
