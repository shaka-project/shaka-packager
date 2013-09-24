# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import re
import struct


LOGGER = logging.getLogger('dmprof')


class PageFrame(object):
  """Represents a pageframe and maybe its shared count."""
  def __init__(self, pfn, size, pagecount, start_truncated, end_truncated):
    self._pfn = pfn
    self._size = size
    self._pagecount = pagecount
    self._start_truncated = start_truncated
    self._end_truncated = end_truncated

  def __str__(self):
    result = str()
    if self._start_truncated:
      result += '<'
    result += '%06x#%d' % (self._pfn, self._pagecount)
    if self._end_truncated:
      result += '>'
    return result

  def __repr__(self):
    return str(self)

  @staticmethod
  def parse(encoded_pfn, size):
    start = 0
    end = len(encoded_pfn)
    end_truncated = False
    if encoded_pfn.endswith('>'):
      end = len(encoded_pfn) - 1
      end_truncated = True
    pagecount_found = encoded_pfn.find('#')
    pagecount = None
    if pagecount_found >= 0:
      encoded_pagecount = 'AAA' + encoded_pfn[pagecount_found+1 : end]
      pagecount = struct.unpack(
          '>I', '\x00' + encoded_pagecount.decode('base64'))[0]
      end = pagecount_found
    start_truncated = False
    if encoded_pfn.startswith('<'):
      start = 1
      start_truncated = True

    pfn = struct.unpack(
        '>I', '\x00' + (encoded_pfn[start:end]).decode('base64'))[0]

    return PageFrame(pfn, size, pagecount, start_truncated, end_truncated)

  @property
  def pfn(self):
    return self._pfn

  @property
  def size(self):
    return self._size

  def set_size(self, size):
    self._size = size

  @property
  def pagecount(self):
    return self._pagecount

  @property
  def start_truncated(self):
    return self._start_truncated

  @property
  def end_truncated(self):
    return self._end_truncated


class PFNCounts(object):
  """Represents counts of PFNs in a process."""

  _PATH_PATTERN = re.compile(r'^(.*)\.([0-9]+)\.([0-9]+)\.heap$')

  def __init__(self, path, modified_time):
    matched = self._PATH_PATTERN.match(path)
    if matched:
      self._pid = int(matched.group(2))
    else:
      self._pid = 0
    self._command_line = ''
    self._pagesize = 4096
    self._path = path
    self._pfn_meta = ''
    self._pfnset = {}
    self._reason = ''
    self._time = modified_time

  @staticmethod
  def load(path, log_header='Loading PFNs from a heap profile dump: '):
    pfnset = PFNCounts(path, float(os.stat(path).st_mtime))
    LOGGER.info('%s%s' % (log_header, path))

    with open(path, 'r') as pfnset_f:
      pfnset.load_file(pfnset_f)

    return pfnset

  @property
  def path(self):
    return self._path

  @property
  def pid(self):
    return self._pid

  @property
  def time(self):
    return self._time

  @property
  def reason(self):
    return self._reason

  @property
  def iter_pfn(self):
    for pfn, count in self._pfnset.iteritems():
      yield pfn, count

  def load_file(self, pfnset_f):
    prev_pfn_end_truncated = None
    for line in pfnset_f:
      line = line.strip()
      if line.startswith('GLOBAL_STATS:') or line.startswith('STACKTRACES:'):
        break
      elif line.startswith('PF: '):
        for encoded_pfn in line[3:].split():
          page_frame = PageFrame.parse(encoded_pfn, self._pagesize)
          if page_frame.start_truncated and (
              not prev_pfn_end_truncated or
              prev_pfn_end_truncated != page_frame.pfn):
            LOGGER.error('Broken page frame number: %s.' % encoded_pfn)
          self._pfnset[page_frame.pfn] = self._pfnset.get(page_frame.pfn, 0) + 1
          if page_frame.end_truncated:
            prev_pfn_end_truncated = page_frame.pfn
          else:
            prev_pfn_end_truncated = None
      elif line.startswith('PageSize: '):
        self._pagesize = int(line[10:])
      elif line.startswith('PFN: '):
        self._pfn_meta = line[5:]
      elif line.startswith('PageFrame: '):
        self._pfn_meta = line[11:]
      elif line.startswith('Time: '):
        self._time = float(line[6:])
      elif line.startswith('CommandLine: '):
        self._command_line = line[13:]
      elif line.startswith('Reason: '):
        self._reason = line[8:]
