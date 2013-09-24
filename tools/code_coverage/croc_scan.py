# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Crocodile source scanners."""


import re


class Scanner(object):
  """Generic source scanner."""

  def __init__(self):
    """Constructor."""

    self.re_token = re.compile('#')
    self.comment_to_eol = ['#']
    self.comment_start = None
    self.comment_end = None

  def ScanLines(self, lines):
    """Scans the lines for executable statements.

    Args:
      lines: Iterator returning source lines.

    Returns:
      An array of line numbers which are executable.
    """
    exe_lines = []
    lineno = 0

    in_string = None
    in_comment = None
    comment_index = None

    for line in lines:
      lineno += 1
      in_string_at_start = in_string

      for t in self.re_token.finditer(line):
        tokenstr = t.groups()[0]

        if in_comment:
          # Inside a multi-line comment, so look for end token
          if tokenstr == in_comment:
            in_comment = None
            # Replace comment with spaces
            line = (line[:comment_index]
                    + ' ' * (t.end(0) - comment_index)
                    + line[t.end(0):])

        elif in_string:
          # Inside a string, so look for end token
          if tokenstr == in_string:
            in_string = None

        elif tokenstr in self.comment_to_eol:
          # Single-line comment, so truncate line at start of token
          line = line[:t.start(0)]
          break

        elif tokenstr == self.comment_start:
          # Multi-line comment start - end token is comment_end
          in_comment = self.comment_end
          comment_index = t.start(0)

        else:
          # Starting a string - end token is same as start
          in_string = tokenstr

      # If still in comment at end of line, remove comment
      if in_comment:
        line = line[:comment_index]
        # Next line, delete from the beginnine
        comment_index = 0

      # If line-sans-comments is not empty, claim it may be executable
      if line.strip() or in_string_at_start:
        exe_lines.append(lineno)

    # Return executable lines
    return exe_lines

  def Scan(self, filename):
    """Reads the file and scans its lines.

    Args:
      filename: Path to file to scan.

    Returns:
      An array of line numbers which are executable.
    """

    # TODO: All manner of error checking
    f = None
    try:
      f = open(filename, 'rt')
      return self.ScanLines(f)
    finally:
      if f:
        f.close()


class PythonScanner(Scanner):
  """Python source scanner."""

  def __init__(self):
    """Constructor."""
    Scanner.__init__(self)

    # TODO: This breaks for strings ending in more than 2 backslashes.  Need
    # a pattern which counts only an odd number of backslashes, so the last
    # one thus escapes the quote.
    self.re_token = re.compile(r'(#|\'\'\'|"""|(?<!(?<!\\)\\)["\'])')
    self.comment_to_eol = ['#']
    self.comment_start = None
    self.comment_end = None


class CppScanner(Scanner):
  """C / C++ / ObjC / ObjC++ source scanner."""

  def __init__(self):
    """Constructor."""
    Scanner.__init__(self)

    # TODO: This breaks for strings ending in more than 2 backslashes.  Need
    # a pattern which counts only an odd number of backslashes, so the last
    # one thus escapes the quote.
    self.re_token = re.compile(r'(^\s*#|//|/\*|\*/|(?<!(?<!\\)\\)["\'])')

    # TODO: Treat '\' at EOL as a token, and handle it as continuing the
    # previous line.  That is, if in a comment-to-eol, this line is a comment
    # too.

    # Note that we treat # at beginning of line as a comment, so that we ignore
    # preprocessor definitions
    self.comment_to_eol = ['//', '#']

    self.comment_start = '/*'
    self.comment_end = '*/'


def ScanFile(filename, language):
  """Scans a file for executable lines.

  Args:
    filename: Path to file to scan.
    language: Language for file ('C', 'C++', 'python', 'ObjC', 'ObjC++')

  Returns:
    A list of executable lines, or an empty list if the file was not a handled
        language.
  """

  if language == 'python':
    return PythonScanner().Scan(filename)
  elif language in ['C', 'C++', 'ObjC', 'ObjC++']:
    return CppScanner().Scan(filename)

  # Something we don't handle
  return []
