# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os

from lib.symbol import FUNCTION_SYMBOLS, SOURCEFILE_SYMBOLS, TYPEINFO_SYMBOLS


LOGGER = logging.getLogger('dmprof')

# Indexes in dumped heap profile dumps.
VIRTUAL, COMMITTED, ALLOC_COUNT, FREE_COUNT, _, BUCKET_ID = range(6)


class Bucket(object):
  """Represents a bucket, which is a unit of memory block classification."""

  def __init__(self, stacktrace, allocator_type, typeinfo, typeinfo_name):
    self._stacktrace = stacktrace
    self._allocator_type = allocator_type
    self._typeinfo = typeinfo
    self._typeinfo_name = typeinfo_name

    self._symbolized_stackfunction = stacktrace
    self._symbolized_joined_stackfunction = ''
    self._symbolized_stacksourcefile = stacktrace
    self._symbolized_joined_stacksourcefile = ''
    self._symbolized_typeinfo = typeinfo_name

    self.component_cache = ''

  def __str__(self):
    result = []
    result.append(self._allocator_type)
    if self._symbolized_typeinfo == 'no typeinfo':
      result.append('tno_typeinfo')
    else:
      result.append('t' + self._symbolized_typeinfo)
    result.append('n' + self._typeinfo_name)
    result.extend(['%s(@%s)' % (function, sourcefile)
                   for function, sourcefile
                   in zip(self._symbolized_stackfunction,
                          self._symbolized_stacksourcefile)])
    return ' '.join(result)

  def symbolize(self, symbol_mapping_cache):
    """Makes a symbolized stacktrace and typeinfo with |symbol_mapping_cache|.

    Args:
        symbol_mapping_cache: A SymbolMappingCache object.
    """
    # TODO(dmikurube): Fill explicitly with numbers if symbol not found.
    self._symbolized_stackfunction = [
        symbol_mapping_cache.lookup(FUNCTION_SYMBOLS, address)
        for address in self._stacktrace]
    self._symbolized_joined_stackfunction = ' '.join(
        self._symbolized_stackfunction)
    self._symbolized_stacksourcefile = [
        symbol_mapping_cache.lookup(SOURCEFILE_SYMBOLS, address)
        for address in self._stacktrace]
    self._symbolized_joined_stacksourcefile = ' '.join(
        self._symbolized_stacksourcefile)
    if not self._typeinfo:
      self._symbolized_typeinfo = 'no typeinfo'
    else:
      self._symbolized_typeinfo = symbol_mapping_cache.lookup(
          TYPEINFO_SYMBOLS, self._typeinfo)
      if not self._symbolized_typeinfo:
        self._symbolized_typeinfo = 'no typeinfo'

  def clear_component_cache(self):
    self.component_cache = ''

  @property
  def stacktrace(self):
    return self._stacktrace

  @property
  def allocator_type(self):
    return self._allocator_type

  @property
  def typeinfo(self):
    return self._typeinfo

  @property
  def typeinfo_name(self):
    return self._typeinfo_name

  @property
  def symbolized_stackfunction(self):
    return self._symbolized_stackfunction

  @property
  def symbolized_joined_stackfunction(self):
    return self._symbolized_joined_stackfunction

  @property
  def symbolized_stacksourcefile(self):
    return self._symbolized_stacksourcefile

  @property
  def symbolized_joined_stacksourcefile(self):
    return self._symbolized_joined_stacksourcefile

  @property
  def symbolized_typeinfo(self):
    return self._symbolized_typeinfo


class BucketSet(object):
  """Represents a set of bucket."""
  def __init__(self):
    self._buckets = {}
    self._code_addresses = set()
    self._typeinfo_addresses = set()

  def load(self, prefix):
    """Loads all related bucket files.

    Args:
        prefix: A prefix string for bucket file names.
    """
    LOGGER.info('Loading bucket files.')

    n = 0
    skipped = 0
    while True:
      path = '%s.%04d.buckets' % (prefix, n)
      if not os.path.exists(path) or not os.stat(path).st_size:
        if skipped > 10:
          break
        n += 1
        skipped += 1
        continue
      LOGGER.info('  %s' % path)
      with open(path, 'r') as f:
        self._load_file(f)
      n += 1
      skipped = 0

  def _load_file(self, bucket_f):
    for line in bucket_f:
      words = line.split()
      typeinfo = None
      typeinfo_name = ''
      stacktrace_begin = 2
      for index, word in enumerate(words):
        if index < 2:
          continue
        if word[0] == 't':
          typeinfo = int(word[1:], 16)
          self._typeinfo_addresses.add(typeinfo)
        elif word[0] == 'n':
          typeinfo_name = word[1:]
        else:
          stacktrace_begin = index
          break
      stacktrace = [int(address, 16) for address in words[stacktrace_begin:]]
      for frame in stacktrace:
        self._code_addresses.add(frame)
      self._buckets[int(words[0])] = Bucket(
          stacktrace, words[1], typeinfo, typeinfo_name)

  def __iter__(self):
    for bucket_id, bucket_content in self._buckets.iteritems():
      yield bucket_id, bucket_content

  def __getitem__(self, bucket_id):
    return self._buckets[bucket_id]

  def get(self, bucket_id):
    return self._buckets.get(bucket_id)

  def symbolize(self, symbol_mapping_cache):
    for bucket_content in self._buckets.itervalues():
      bucket_content.symbolize(symbol_mapping_cache)

  def clear_component_cache(self):
    for bucket_content in self._buckets.itervalues():
      bucket_content.clear_component_cache()

  def iter_addresses(self, symbol_type):
    if symbol_type in [FUNCTION_SYMBOLS, SOURCEFILE_SYMBOLS]:
      for function in self._code_addresses:
        yield function
    else:
      for function in self._typeinfo_addresses:
        yield function
