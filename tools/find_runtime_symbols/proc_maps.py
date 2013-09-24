# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import re


_MAPS_PATTERN = re.compile(
    r'^([a-f0-9]+)-([a-f0-9]+)\s+(.)(.)(.)(.)\s+([a-f0-9]+)\s+(\S+):(\S+)\s+'
    r'(\d+)\s*(.*)$', re.IGNORECASE)


class ProcMapsEntry(object):
  """A class representing one line in /proc/.../maps."""

  def __init__(
      self, begin, end, readable, writable, executable, private, offset,
      major, minor, inode, name):
    self.begin = begin
    self.end = end
    self.readable = readable
    self.writable = writable
    self.executable = executable
    self.private = private
    self.offset = offset
    self.major = major
    self.minor = minor
    self.inode = inode
    self.name = name

  def as_dict(self):
    return {
        'begin': self.begin,
        'end': self.end,
        'readable': self.readable,
        'writable': self.writable,
        'executable': self.executable,
        'private': self.private,
        'offset': self.offset,
        'major': self.major,
        'minor': self.minor,
        'inode': self.inode,
        'name': self.name,
    }


class ProcMaps(object):
  """A class representing contents in /proc/.../maps."""

  def __init__(self):
    self._sorted_indexes = []
    self._dictionary = {}
    self._sorted = True

  def iter(self, condition):
    if not self._sorted:
      self._sorted_indexes.sort()
      self._sorted = True
    for index in self._sorted_indexes:
      if not condition or condition(self._dictionary[index]):
        yield self._dictionary[index]

  def __iter__(self):
    if not self._sorted:
      self._sorted_indexes.sort()
      self._sorted = True
    for index in self._sorted_indexes:
      yield self._dictionary[index]

  @staticmethod
  def load(f):
    table = ProcMaps()
    for line in f:
      table.append_line(line)
    return table

  def append_line(self, line):
    entry = self.parse_line(line)
    if entry:
      self._append_entry(entry)

  @staticmethod
  def parse_line(line):
    matched = _MAPS_PATTERN.match(line)
    if matched:
      return ProcMapsEntry(  # pylint: disable=W0212
          int(matched.group(1), 16),  # begin
          int(matched.group(2), 16),  # end
          matched.group(3),           # readable
          matched.group(4),           # writable
          matched.group(5),           # executable
          matched.group(6),           # private
          int(matched.group(7), 16),  # offset
          matched.group(8),           # major
          matched.group(9),           # minor
          int(matched.group(10), 10), # inode
          matched.group(11)           # name
          )
    else:
      return None

  @staticmethod
  def constants(entry):
    return (entry.writable == '-' and entry.executable == '-' and re.match(
        '\S+(\.(so|dll|dylib|bundle)|chrome)((\.\d+)+\w*(\.\d+){0,3})?',
        entry.name))

  @staticmethod
  def executable(entry):
    return (entry.executable == 'x' and re.match(
        '\S+(\.(so|dll|dylib|bundle)|chrome)((\.\d+)+\w*(\.\d+){0,3})?',
        entry.name))

  @staticmethod
  def executable_and_constants(entry):
    return (((entry.writable == '-' and entry.executable == '-') or
             entry.executable == 'x') and re.match(
        '\S+(\.(so|dll|dylib|bundle)|chrome)((\.\d+)+\w*(\.\d+){0,3})?',
        entry.name))

  def _append_entry(self, entry):
    if self._sorted_indexes and self._sorted_indexes[-1] > entry.begin:
      self._sorted = False
    self._sorted_indexes.append(entry.begin)
    self._dictionary[entry.begin] = entry
