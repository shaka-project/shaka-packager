# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

class Code(object):
  """A convenience object for constructing code.

  Logically each object should be a block of code. All methods except |Render|
  and |IsEmpty| return self.
  """
  def __init__(self, indent_size=2, comment_length=80):
    self._code = []
    self._indent_level = 0
    self._indent_size = indent_size
    self._comment_length = comment_length

  def Append(self, line='', substitute=True, indent_level=None):
    """Appends a line of code at the current indent level or just a newline if
    line is not specified. Trailing whitespace is stripped.

    substitute: indicated whether this line should be affected by
    code.Substitute().
    """
    if indent_level is None:
      indent_level = self._indent_level
    self._code.append(Line(((' ' * indent_level) + line).rstrip(),
                      substitute=substitute))
    return self

  def IsEmpty(self):
    """Returns True if the Code object is empty.
    """
    return not bool(self._code)

  def Concat(self, obj):
    """Concatenate another Code object onto this one. Trailing whitespace is
    stripped.

    Appends the code at the current indent level. Will fail if there are any
    un-interpolated format specifiers eg %s, %(something)s which helps
    isolate any strings that haven't been substituted.
    """
    if not isinstance(obj, Code):
      raise TypeError(type(obj))
    assert self is not obj
    for line in obj._code:
      try:
        # line % () will fail if any substitution tokens are left in line
        if line.substitute:
          line.value %= ()
      except TypeError:
        raise TypeError('Unsubstituted value when concatting\n' + line.value)
      except ValueError:
        raise ValueError('Stray % character when concatting\n' + line.value)
      self.Append(line.value, line.substitute)

    return self

  def Cblock(self, code):
    """Concatenates another Code object |code| onto this one followed by a
    blank line, if |code| is non-empty."""
    if not code.IsEmpty():
      self.Concat(code).Append()
    return self

  def Sblock(self, line=None):
    """Starts a code block.

    Appends a line of code and then increases the indent level.
    """
    if line is not None:
      self.Append(line)
    self._indent_level += self._indent_size
    return self

  def Eblock(self, line=None):
    """Ends a code block by decreasing and then appending a line (or a blank
    line if not given).
    """
    # TODO(calamity): Decide if type checking is necessary
    #if not isinstance(line, basestring):
    #  raise TypeError
    self._indent_level -= self._indent_size
    if line is not None:
      self.Append(line)
    return self

  def Comment(self, comment, comment_prefix='// '):
    """Adds the given string as a comment.

    Will split the comment if it's too long. Use mainly for variable length
    comments. Otherwise just use code.Append('// ...') for comments.

    Unaffected by code.Substitute().
    """
    max_len = self._comment_length - self._indent_level - len(comment_prefix)
    while len(comment) >= max_len:
      line = comment[0:max_len]
      last_space = line.rfind(' ')
      if last_space != -1:
        line = line[0:last_space]
        comment = comment[last_space + 1:]
      else:
        comment = comment[max_len:]
      self.Append(comment_prefix + line, substitute=False)
    self.Append(comment_prefix + comment, substitute=False)
    return self

  def Substitute(self, d):
    """Goes through each line and interpolates using the given dict.

    Raises type error if passed something that isn't a dict

    Use for long pieces of code using interpolation with the same variables
    repeatedly. This will reduce code and allow for named placeholders which
    are more clear.
    """
    if not isinstance(d, dict):
      raise TypeError('Passed argument is not a dictionary: ' + d)
    for i, line in enumerate(self._code):
      if self._code[i].substitute:
        # Only need to check %s because arg is a dict and python will allow
        # '%s %(named)s' but just about nothing else
        if '%s' in self._code[i].value or '%r' in self._code[i].value:
          raise TypeError('"%s" or "%r" found in substitution. '
                          'Named arguments only. Use "%" to escape')
        self._code[i].value = line.value % d
        self._code[i].substitute = False
    return self

  def Render(self):
    """Renders Code as a string.
    """
    return '\n'.join([l.value for l in self._code])

class Line(object):
  """A line of code.
  """
  def __init__(self, value, substitute=True):
    self.value = value
    self.substitute = substitute
