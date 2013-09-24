#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Valgrind-style suppressions for heapchecker reports.

Suppressions are defined as follows:

# optional one-line comments anywhere in the suppressions file.
{
  Toolname:Errortype
  Short description of the error.
  fun:function_name
  fun:wildcarded_fun*_name
  # an ellipsis wildcards zero or more functions in a stack.
  ...
  fun:some_other_function_name
}

Note that only a 'fun:' prefix is allowed, i.e. we can't suppress objects and
source files.

If ran from the command line, suppressions.py does a self-test of the
Suppression class.
"""

import re

ELLIPSIS = '...'


class Suppression(object):
  """This class represents a single stack trace suppression.

  Attributes:
    type: A string representing the error type, e.g. Heapcheck:Leak.
    description: A string representing the error description.
  """

  def __init__(self, kind, description, stack):
    """Inits Suppression.

    stack is a list of function names and/or wildcards.

    Args:
      kind:
      description: Same as class attributes.
      stack: A list of strings.
    """
    self.type = kind
    self.description = description
    self._stack = stack
    re_line = ''
    re_bucket = ''
    for line in stack:
      if line == ELLIPSIS:
        re_line += re.escape(re_bucket)
        re_bucket = ''
        re_line += '(.*\n)*'
      else:
        for char in line:
          if char == '*':
            re_line += re.escape(re_bucket)
            re_bucket = ''
            re_line += '.*'
          else:  # there can't be any '\*'s in a stack trace
            re_bucket += char
        re_line += re.escape(re_bucket)
        re_bucket = ''
        re_line += '\n'
    self._re = re.compile(re_line, re.MULTILINE)

  def Match(self, report):
    """Returns bool indicating whether the suppression matches the given report.

    Args:
      report: list of strings (function names).
    Returns:
      True if the suppression is not empty and matches the report.
    """
    if not self._stack:
      return False
    if self._re.match('\n'.join(report) + '\n'):
      return True
    else:
      return False


class SuppressionError(Exception):
  def __init__(self, filename, line, report=''):
    Exception.__init__(self, filename, line, report)
    self._file = filename
    self._line = line
    self._report = report

  def __str__(self):
    return 'Error reading suppressions from "%s" (line %d): %s.' % (
        self._file, self._line, self._report)


def ReadSuppressionsFromFile(filename):
  """Given a file, returns a list of suppressions."""
  input_file = file(filename, 'r')
  result = []
  cur_descr = ''
  cur_type = ''
  cur_stack = []
  nline = 0
  try:
    for line in input_file:
      nline += 1
      line = line.strip()
      if line.startswith('#'):
        continue
      elif line.startswith('{'):
        pass
      elif line.startswith('}'):
        result.append(Suppression(cur_type, cur_descr, cur_stack))
        cur_descr = ''
        cur_type = ''
        cur_stack = []
      elif not cur_descr:
        cur_descr = line
        continue
      elif not cur_type:
        cur_type = line
        continue
      elif line.startswith('fun:'):
        line = line[4:]
        cur_stack.append(line.strip())
      elif line.startswith(ELLIPSIS):
        cur_stack.append(ELLIPSIS)
      else:
        raise SuppressionError(filename, nline,
                               '"fun:function_name" or "..." expected')
  except SuppressionError:
    input_file.close()
    raise
  return result


def MatchTest():
  """Tests the Suppression.Match() capabilities."""

  def GenSupp(*lines):
    return Suppression('', '', list(lines))
  empty = GenSupp()
  assert not empty.Match([])
  assert not empty.Match(['foo', 'bar'])
  asterisk = GenSupp('*bar')
  assert asterisk.Match(['foobar', 'foobaz'])
  assert not asterisk.Match(['foobaz', 'foobar'])
  ellipsis = GenSupp('...', 'foo')
  assert ellipsis.Match(['foo', 'bar'])
  assert ellipsis.Match(['bar', 'baz', 'foo'])
  assert not ellipsis.Match(['bar', 'baz', 'bah'])
  mixed = GenSupp('...', 'foo*', 'function')
  assert mixed.Match(['foobar', 'foobaz', 'function'])
  assert not mixed.Match(['foobar', 'blah', 'function'])
  at_and_dollar = GenSupp('foo@GLIBC', 'bar@NOCANCEL')
  assert at_and_dollar.Match(['foo@GLIBC', 'bar@NOCANCEL'])
  re_chars = GenSupp('.*')
  assert re_chars.Match(['.foobar'])
  assert not re_chars.Match(['foobar'])
  print 'PASS'


if __name__ == '__main__':
  MatchTest()
