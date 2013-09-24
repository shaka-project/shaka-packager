# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import optparse
import os
import re

from lib.bucket import BucketSet
from lib.dump import Dump, DumpList
from lib.symbol import SymbolDataSources, SymbolMappingCache, SymbolFinder
from lib.symbol import proc_maps
from lib.symbol import FUNCTION_SYMBOLS, SOURCEFILE_SYMBOLS, TYPEINFO_SYMBOLS


LOGGER = logging.getLogger('dmprof')

BASE_PATH = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CHROME_SRC_PATH = os.path.join(BASE_PATH, os.pardir, os.pardir)


class SubCommand(object):
  """Subclasses are a subcommand for this executable.

  See COMMANDS in main() in dmprof.py.
  """
  _DEVICE_BINDIRS = ['/data/data/', '/data/app-lib/', '/data/local/tmp']

  def __init__(self, usage):
    self._parser = optparse.OptionParser(usage)

  @staticmethod
  def load_basic_files(
      dump_path, multiple, no_dump=False, alternative_dirs=None):
    prefix = SubCommand._find_prefix(dump_path)
    # If the target process is estimated to be working on Android, converts
    # a path in the Android device to a path estimated to be corresponding in
    # the host.  Use --alternative-dirs to specify the conversion manually.
    if not alternative_dirs:
      alternative_dirs = SubCommand._estimate_alternative_dirs(prefix)
    if alternative_dirs:
      for device, host in alternative_dirs.iteritems():
        LOGGER.info('Assuming %s on device as %s on host' % (device, host))
    symbol_data_sources = SymbolDataSources(prefix, alternative_dirs)
    symbol_data_sources.prepare()
    bucket_set = BucketSet()
    bucket_set.load(prefix)
    if not no_dump:
      if multiple:
        dump_list = DumpList.load(SubCommand._find_all_dumps(dump_path))
      else:
        dump = Dump.load(dump_path)
    symbol_mapping_cache = SymbolMappingCache()
    with open(prefix + '.cache.function', 'a+') as cache_f:
      symbol_mapping_cache.update(
          FUNCTION_SYMBOLS, bucket_set,
          SymbolFinder(FUNCTION_SYMBOLS, symbol_data_sources), cache_f)
    with open(prefix + '.cache.typeinfo', 'a+') as cache_f:
      symbol_mapping_cache.update(
          TYPEINFO_SYMBOLS, bucket_set,
          SymbolFinder(TYPEINFO_SYMBOLS, symbol_data_sources), cache_f)
    with open(prefix + '.cache.sourcefile', 'a+') as cache_f:
      symbol_mapping_cache.update(
          SOURCEFILE_SYMBOLS, bucket_set,
          SymbolFinder(SOURCEFILE_SYMBOLS, symbol_data_sources), cache_f)
    bucket_set.symbolize(symbol_mapping_cache)
    if no_dump:
      return bucket_set
    elif multiple:
      return (bucket_set, dump_list)
    else:
      return (bucket_set, dump)

  @staticmethod
  def _find_prefix(path):
    return re.sub('\.[0-9][0-9][0-9][0-9]\.heap', '', path)

  @staticmethod
  def _estimate_alternative_dirs(prefix):
    """Estimates a path in host from a corresponding path in target device.

    For Android, dmprof.py should find symbol information from binaries in
    the host instead of the Android device because dmprof.py doesn't run on
    the Android device.  This method estimates a path in the host
    corresponding to a path in the Android device.

    Returns:
        A dict that maps a path in the Android device to a path in the host.
        If a file in SubCommand._DEVICE_BINDIRS is found in /proc/maps, it
        assumes the process was running on Android and maps the path to
        "out/Debug/lib" in the Chromium directory.  An empty dict is returned
        unless Android.
    """
    device_lib_path_candidates = set()

    with open(prefix + '.maps') as maps_f:
      maps = proc_maps.ProcMaps.load(maps_f)
      for entry in maps:
        name = entry.as_dict()['name']
        if any([base_dir in name for base_dir in SubCommand._DEVICE_BINDIRS]):
          device_lib_path_candidates.add(os.path.dirname(name))

    if len(device_lib_path_candidates) == 1:
      return {device_lib_path_candidates.pop(): os.path.join(
                  CHROME_SRC_PATH, 'out', 'Debug', 'lib')}
    else:
      return {}

  @staticmethod
  def _find_all_dumps(dump_path):
    prefix = SubCommand._find_prefix(dump_path)
    dump_path_list = [dump_path]

    n = int(dump_path[len(dump_path) - 9 : len(dump_path) - 5])
    n += 1
    skipped = 0
    while True:
      p = '%s.%04d.heap' % (prefix, n)
      if os.path.exists(p) and os.stat(p).st_size:
        dump_path_list.append(p)
      else:
        if skipped > 10:
          break
        skipped += 1
      n += 1

    return dump_path_list

  @staticmethod
  def _find_all_buckets(dump_path):
    prefix = SubCommand._find_prefix(dump_path)
    bucket_path_list = []

    n = 0
    while True:
      path = '%s.%04d.buckets' % (prefix, n)
      if not os.path.exists(path):
        if n > 10:
          break
        n += 1
        continue
      bucket_path_list.append(path)
      n += 1

    return bucket_path_list

  def _parse_args(self, sys_argv, required):
    options, args = self._parser.parse_args(sys_argv)
    if len(args) < required + 1:
      self._parser.error('needs %d argument(s).\n' % required)
      return None
    return (options, args)

  @staticmethod
  def _parse_policy_list(options_policy):
    if options_policy:
      return options_policy.split(',')
    else:
      return None
