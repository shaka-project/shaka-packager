# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import copy
import datetime
import logging
import os
import re
import time

from lib.bucket import BUCKET_ID
from lib.exceptions import EmptyDumpException, InvalidDumpException
from lib.exceptions import ObsoleteDumpVersionException, ParsingException
from lib.pageframe import PageFrame
from lib.range_dict import ExclusiveRangeDict
from lib.symbol import proc_maps


LOGGER = logging.getLogger('dmprof')


# Heap Profile Dump versions

# DUMP_DEEP_[1-4] are obsolete.
# DUMP_DEEP_2+ distinct mmap regions and malloc chunks.
# DUMP_DEEP_3+ don't include allocation functions in their stack dumps.
# DUMP_DEEP_4+ support comments with '#' and global stats "nonprofiled-*".
# DUMP_DEEP_[1-2] should be processed by POLICY_DEEP_1.
# DUMP_DEEP_[3-4] should be processed by POLICY_DEEP_2 or POLICY_DEEP_3.
DUMP_DEEP_1 = 'DUMP_DEEP_1'
DUMP_DEEP_2 = 'DUMP_DEEP_2'
DUMP_DEEP_3 = 'DUMP_DEEP_3'
DUMP_DEEP_4 = 'DUMP_DEEP_4'

DUMP_DEEP_OBSOLETE = (DUMP_DEEP_1, DUMP_DEEP_2, DUMP_DEEP_3, DUMP_DEEP_4)

# DUMP_DEEP_5 doesn't separate sections for malloc and mmap.
# malloc and mmap are identified in bucket files.
# DUMP_DEEP_5 should be processed by POLICY_DEEP_4.
DUMP_DEEP_5 = 'DUMP_DEEP_5'

# DUMP_DEEP_6 adds a mmap list to DUMP_DEEP_5.
DUMP_DEEP_6 = 'DUMP_DEEP_6'


class Dump(object):
  """Represents a heap profile dump."""

  _PATH_PATTERN = re.compile(r'^(.*)\.([0-9]+)\.([0-9]+)\.heap$')

  _HOOK_PATTERN = re.compile(
      r'^ ([ \(])([a-f0-9]+)([ \)])-([ \(])([a-f0-9]+)([ \)])\s+'
      r'(hooked|unhooked)\s+(.+)$', re.IGNORECASE)

  _HOOKED_PATTERN = re.compile(r'(?P<TYPE>.+ )?(?P<COMMITTED>[0-9]+) / '
                               '(?P<RESERVED>[0-9]+) @ (?P<BUCKETID>[0-9]+)')
  _UNHOOKED_PATTERN = re.compile(r'(?P<TYPE>.+ )?(?P<COMMITTED>[0-9]+) / '
                                 '(?P<RESERVED>[0-9]+)')

  _OLD_HOOKED_PATTERN = re.compile(r'(?P<TYPE>.+) @ (?P<BUCKETID>[0-9]+)')
  _OLD_UNHOOKED_PATTERN = re.compile(r'(?P<TYPE>.+) (?P<COMMITTED>[0-9]+)')

  _TIME_PATTERN_FORMAT = re.compile(
      r'^Time: ([0-9]+/[0-9]+/[0-9]+ [0-9]+:[0-9]+:[0-9]+)(\.[0-9]+)?')
  _TIME_PATTERN_SECONDS = re.compile(r'^Time: ([0-9]+)$')

  def __init__(self, path, modified_time):
    self._path = path
    matched = self._PATH_PATTERN.match(path)
    self._pid = int(matched.group(2))
    self._count = int(matched.group(3))
    self._time = modified_time
    self._map = {}
    self._procmaps = ExclusiveRangeDict(ProcMapsEntryAttribute)
    self._stacktrace_lines = []
    self._global_stats = {} # used only in apply_policy

    self._run_id = ''
    self._pagesize = 4096
    self._pageframe_length = 0
    self._pageframe_encoding = ''
    self._has_pagecount = False

    self._version = ''
    self._lines = []

  @property
  def path(self):
    return self._path

  @property
  def count(self):
    return self._count

  @property
  def time(self):
    return self._time

  @property
  def iter_map(self):
    for region in sorted(self._map.iteritems()):
      yield region[0], region[1]

  def iter_procmaps(self):
    for begin, end, attr in self._map.iter_range():
      yield begin, end, attr

  @property
  def iter_stacktrace(self):
    for line in self._stacktrace_lines:
      yield line

  def global_stat(self, name):
    return self._global_stats[name]

  @property
  def run_id(self):
    return self._run_id

  @property
  def pagesize(self):
    return self._pagesize

  @property
  def pageframe_length(self):
    return self._pageframe_length

  @property
  def pageframe_encoding(self):
    return self._pageframe_encoding

  @property
  def has_pagecount(self):
    return self._has_pagecount

  @staticmethod
  def load(path, log_header='Loading a heap profile dump: '):
    """Loads a heap profile dump.

    Args:
        path: A file path string to load.
        log_header: A preceding string for log messages.

    Returns:
        A loaded Dump object.

    Raises:
        ParsingException for invalid heap profile dumps.
    """
    dump = Dump(path, os.stat(path).st_mtime)
    with open(path, 'r') as f:
      dump.load_file(f, log_header)
    return dump

  def load_file(self, f, log_header):
    self._lines = [line for line in f
                   if line and not line.startswith('#')]

    try:
      self._version, ln = self._parse_version()
      self._parse_meta_information()
      if self._version == DUMP_DEEP_6:
        self._parse_mmap_list()
      self._parse_global_stats()
      self._extract_stacktrace_lines(ln)
    except EmptyDumpException:
      LOGGER.info('%s%s ...ignored an empty dump.' % (log_header, self._path))
    except ParsingException, e:
      LOGGER.error('%s%s ...error %s' % (log_header, self._path, e))
      raise
    else:
      LOGGER.info('%s%s (version:%s)' % (log_header, self._path, self._version))

  def _parse_version(self):
    """Parses a version string in self._lines.

    Returns:
        A pair of (a string representing a version of the stacktrace dump,
        and an integer indicating a line number next to the version string).

    Raises:
        ParsingException for invalid dump versions.
    """
    version = ''

    # Skip until an identifiable line.
    headers = ('STACKTRACES:\n', 'MMAP_STACKTRACES:\n', 'heap profile: ')
    if not self._lines:
      raise EmptyDumpException('Empty heap dump file.')
    (ln, found) = skip_while(
        0, len(self._lines),
        lambda n: not self._lines[n].startswith(headers))
    if not found:
      raise InvalidDumpException('No version header.')

    # Identify a version.
    if self._lines[ln].startswith('heap profile: '):
      version = self._lines[ln][13:].strip()
      if version in (DUMP_DEEP_5, DUMP_DEEP_6):
        (ln, _) = skip_while(
            ln, len(self._lines),
            lambda n: self._lines[n] != 'STACKTRACES:\n')
      elif version in DUMP_DEEP_OBSOLETE:
        raise ObsoleteDumpVersionException(version)
      else:
        raise InvalidDumpException('Invalid version: %s' % version)
    elif self._lines[ln] == 'STACKTRACES:\n':
      raise ObsoleteDumpVersionException(DUMP_DEEP_1)
    elif self._lines[ln] == 'MMAP_STACKTRACES:\n':
      raise ObsoleteDumpVersionException(DUMP_DEEP_2)

    return (version, ln)

  def _parse_global_stats(self):
    """Parses lines in self._lines as global stats."""
    (ln, _) = skip_while(
        0, len(self._lines),
        lambda n: self._lines[n] != 'GLOBAL_STATS:\n')

    global_stat_names = [
        'total', 'absent', 'file-exec', 'file-nonexec', 'anonymous', 'stack',
        'other', 'nonprofiled-absent', 'nonprofiled-anonymous',
        'nonprofiled-file-exec', 'nonprofiled-file-nonexec',
        'nonprofiled-stack', 'nonprofiled-other',
        'profiled-mmap', 'profiled-malloc']

    for prefix in global_stat_names:
      (ln, _) = skip_while(
          ln, len(self._lines),
          lambda n: self._lines[n].split()[0] != prefix)
      words = self._lines[ln].split()
      self._global_stats[prefix + '_virtual'] = int(words[-2])
      self._global_stats[prefix + '_committed'] = int(words[-1])

  def _parse_meta_information(self):
    """Parses lines in self._lines for meta information."""
    (ln, found) = skip_while(
        0, len(self._lines),
        lambda n: self._lines[n] != 'META:\n')
    if not found:
      return
    ln += 1

    while True:
      if self._lines[ln].startswith('Time:'):
        matched_seconds = self._TIME_PATTERN_SECONDS.match(self._lines[ln])
        matched_format = self._TIME_PATTERN_FORMAT.match(self._lines[ln])
        if matched_format:
          self._time = time.mktime(datetime.datetime.strptime(
              matched_format.group(1), '%Y/%m/%d %H:%M:%S').timetuple())
          if matched_format.group(2):
            self._time += float(matched_format.group(2)[1:]) / 1000.0
        elif matched_seconds:
          self._time = float(matched_seconds.group(1))
      elif self._lines[ln].startswith('Reason:'):
        pass  # Nothing to do for 'Reason:'
      elif self._lines[ln].startswith('PageSize: '):
        self._pagesize = int(self._lines[ln][10:])
      elif self._lines[ln].startswith('CommandLine:'):
        pass
      elif (self._lines[ln].startswith('PageFrame: ') or
            self._lines[ln].startswith('PFN: ')):
        if self._lines[ln].startswith('PageFrame: '):
          words = self._lines[ln][11:].split(',')
        else:
          words = self._lines[ln][5:].split(',')
        for word in words:
          if word == '24':
            self._pageframe_length = 24
          elif word == 'Base64':
            self._pageframe_encoding = 'base64'
          elif word == 'PageCount':
            self._has_pagecount = True
      elif self._lines[ln].startswith('RunID: '):
        self._run_id = self._lines[ln][7:].strip()
      elif (self._lines[ln].startswith('MMAP_LIST:') or
            self._lines[ln].startswith('GLOBAL_STATS:')):
        # Skip until "MMAP_LIST:" or "GLOBAL_STATS" is found.
        break
      else:
        pass
      ln += 1

  def _parse_mmap_list(self):
    """Parses lines in self._lines as a mmap list."""
    (ln, found) = skip_while(
        0, len(self._lines),
        lambda n: self._lines[n] != 'MMAP_LIST:\n')
    if not found:
      return {}

    ln += 1
    self._map = {}
    current_vma = {}
    pageframe_list = []
    while True:
      entry = proc_maps.ProcMaps.parse_line(self._lines[ln])
      if entry:
        current_vma = {}
        for _, _, attr in self._procmaps.iter_range(entry.begin, entry.end):
          for key, value in entry.as_dict().iteritems():
            attr[key] = value
            current_vma[key] = value
        ln += 1
        continue

      if self._lines[ln].startswith('  PF: '):
        for pageframe in self._lines[ln][5:].split():
          pageframe_list.append(PageFrame.parse(pageframe, self._pagesize))
        ln += 1
        continue

      matched = self._HOOK_PATTERN.match(self._lines[ln])
      if not matched:
        break
      # 2: starting address
      # 5: end address
      # 7: hooked or unhooked
      # 8: additional information
      if matched.group(7) == 'hooked':
        submatched = self._HOOKED_PATTERN.match(matched.group(8))
        if not submatched:
          submatched = self._OLD_HOOKED_PATTERN.match(matched.group(8))
      elif matched.group(7) == 'unhooked':
        submatched = self._UNHOOKED_PATTERN.match(matched.group(8))
        if not submatched:
          submatched = self._OLD_UNHOOKED_PATTERN.match(matched.group(8))
      else:
        assert matched.group(7) in ['hooked', 'unhooked']

      submatched_dict = submatched.groupdict()
      region_info = { 'vma': current_vma }
      if submatched_dict.get('TYPE'):
        region_info['type'] = submatched_dict['TYPE'].strip()
      if submatched_dict.get('COMMITTED'):
        region_info['committed'] = int(submatched_dict['COMMITTED'])
      if submatched_dict.get('RESERVED'):
        region_info['reserved'] = int(submatched_dict['RESERVED'])
      if submatched_dict.get('BUCKETID'):
        region_info['bucket_id'] = int(submatched_dict['BUCKETID'])

      if matched.group(1) == '(':
        start = current_vma['begin']
      else:
        start = int(matched.group(2), 16)
      if matched.group(4) == '(':
        end = current_vma['end']
      else:
        end = int(matched.group(5), 16)

      if pageframe_list and pageframe_list[0].start_truncated:
        pageframe_list[0].set_size(
            pageframe_list[0].size - start % self._pagesize)
      if pageframe_list and pageframe_list[-1].end_truncated:
        pageframe_list[-1].set_size(
            pageframe_list[-1].size - (self._pagesize - end % self._pagesize))
      region_info['pageframe'] = pageframe_list
      pageframe_list = []

      self._map[(start, end)] = (matched.group(7), region_info)
      ln += 1

  def _extract_stacktrace_lines(self, line_number):
    """Extracts the position of stacktrace lines.

    Valid stacktrace lines are stored into self._stacktrace_lines.

    Args:
        line_number: A line number to start parsing in lines.

    Raises:
        ParsingException for invalid dump versions.
    """
    if self._version in (DUMP_DEEP_5, DUMP_DEEP_6):
      (line_number, _) = skip_while(
          line_number, len(self._lines),
          lambda n: not self._lines[n].split()[0].isdigit())
      stacktrace_start = line_number
      (line_number, _) = skip_while(
          line_number, len(self._lines),
          lambda n: self._check_stacktrace_line(self._lines[n]))
      self._stacktrace_lines = self._lines[stacktrace_start:line_number]

    elif self._version in DUMP_DEEP_OBSOLETE:
      raise ObsoleteDumpVersionException(self._version)

    else:
      raise InvalidDumpException('Invalid version: %s' % self._version)

  @staticmethod
  def _check_stacktrace_line(stacktrace_line):
    """Checks if a given stacktrace_line is valid as stacktrace.

    Args:
        stacktrace_line: A string to be checked.

    Returns:
        True if the given stacktrace_line is valid.
    """
    words = stacktrace_line.split()
    if len(words) < BUCKET_ID + 1:
      return False
    if words[BUCKET_ID - 1] != '@':
      return False
    return True


class DumpList(object):
  """Represents a sequence of heap profile dumps."""

  def __init__(self, dump_list):
    self._dump_list = dump_list

  @staticmethod
  def load(path_list):
    LOGGER.info('Loading heap dump profiles.')
    dump_list = []
    for path in path_list:
      dump_list.append(Dump.load(path, '  '))
    return DumpList(dump_list)

  def __len__(self):
    return len(self._dump_list)

  def __iter__(self):
    for dump in self._dump_list:
      yield dump

  def __getitem__(self, index):
    return self._dump_list[index]


class ProcMapsEntryAttribute(ExclusiveRangeDict.RangeAttribute):
  """Represents an entry of /proc/maps in range_dict.ExclusiveRangeDict."""
  _DUMMY_ENTRY = proc_maps.ProcMapsEntry(
      0,     # begin
      0,     # end
      '-',   # readable
      '-',   # writable
      '-',   # executable
      '-',   # private
      0,     # offset
      '00',  # major
      '00',  # minor
      0,     # inode
      ''     # name
      )

  def __init__(self):
    super(ProcMapsEntryAttribute, self).__init__()
    self._entry = self._DUMMY_ENTRY.as_dict()

  def __str__(self):
    return str(self._entry)

  def __repr__(self):
    return 'ProcMapsEntryAttribute' + str(self._entry)

  def __getitem__(self, key):
    return self._entry[key]

  def __setitem__(self, key, value):
    if key not in self._entry:
      raise KeyError(key)
    self._entry[key] = value

  def copy(self):
    new_entry = ProcMapsEntryAttribute()
    for key, value in self._entry.iteritems():
      new_entry[key] = copy.deepcopy(value)
    return new_entry


def skip_while(index, max_index, skipping_condition):
  """Increments |index| until |skipping_condition|(|index|) is False.

  Returns:
      A pair of an integer indicating a line number after skipped, and a
      boolean value which is True if found a line which skipping_condition
      is False for.
  """
  while skipping_condition(index):
    index += 1
    if index >= max_index:
      return index, False
  return index, True
