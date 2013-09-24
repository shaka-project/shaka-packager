# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import sys

_BASE_PATH = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
_FIND_RUNTIME_SYMBOLS_PATH = os.path.join(_BASE_PATH,
                                          os.pardir,
                                          'find_runtime_symbols')
sys.path.append(_FIND_RUNTIME_SYMBOLS_PATH)

import find_runtime_symbols
import prepare_symbol_info
import proc_maps  # pylint: disable=W0611

LOGGER = logging.getLogger('dmprof')

FUNCTION_SYMBOLS = find_runtime_symbols.FUNCTION_SYMBOLS
SOURCEFILE_SYMBOLS = find_runtime_symbols.SOURCEFILE_SYMBOLS
TYPEINFO_SYMBOLS = find_runtime_symbols.TYPEINFO_SYMBOLS


class SymbolDataSources(object):
  """Manages symbol data sources in a process.

  The symbol data sources consist of maps (/proc/<pid>/maps), nm, readelf and
  so on.  They are collected into a directory '|prefix|.symmap' from the binary
  files by 'prepare()' with tools/find_runtime_symbols/prepare_symbol_info.py.

  Binaries are not mandatory to profile.  The prepared data sources work in
  place of the binary even if the binary has been overwritten with another
  binary.

  Note that loading the symbol data sources takes a long time.  They are often
  very big.  So, the 'dmprof' profiler is designed to use 'SymbolMappingCache'
  which caches actually used symbols.
  """
  def __init__(self, prefix, alternative_dirs=None):
    self._prefix = prefix
    self._prepared_symbol_data_sources_path = None
    self._loaded_symbol_data_sources = None
    self._alternative_dirs = alternative_dirs or {}

  def prepare(self):
    """Prepares symbol data sources by extracting mapping from a binary.

    The prepared symbol data sources are stored in a directory.  The directory
    name is stored in |self._prepared_symbol_data_sources_path|.

    Returns:
        True if succeeded.
    """
    LOGGER.info('Preparing symbol mapping...')
    self._prepared_symbol_data_sources_path, used_tempdir = (
        prepare_symbol_info.prepare_symbol_info(
            self._prefix + '.maps',
            output_dir_path=self._prefix + '.symmap',
            alternative_dirs=self._alternative_dirs,
            use_tempdir=True,
            use_source_file_name=True))
    if self._prepared_symbol_data_sources_path:
      LOGGER.info('  Prepared symbol mapping.')
      if used_tempdir:
        LOGGER.warn('  Using a temporary directory for symbol mapping.')
        LOGGER.warn('  Delete it by yourself.')
        LOGGER.warn('  Or, move the directory by yourself to use it later.')
      return True
    else:
      LOGGER.warn('  Failed to prepare symbol mapping.')
      return False

  def get(self):
    """Returns the prepared symbol data sources.

    Returns:
        The prepared symbol data sources.  None if failed.
    """
    if not self._prepared_symbol_data_sources_path and not self.prepare():
      return None
    if not self._loaded_symbol_data_sources:
      LOGGER.info('Loading symbol mapping...')
      self._loaded_symbol_data_sources = (
          find_runtime_symbols.RuntimeSymbolsInProcess.load(
              self._prepared_symbol_data_sources_path))
    return self._loaded_symbol_data_sources

  def path(self):
    """Returns the path of the prepared symbol data sources if possible."""
    if not self._prepared_symbol_data_sources_path and not self.prepare():
      return None
    return self._prepared_symbol_data_sources_path


class SymbolFinder(object):
  """Finds corresponding symbols from addresses.

  This class does only 'find()' symbols from a specified |address_list|.
  It is introduced to make a finder mockable.
  """
  def __init__(self, symbol_type, symbol_data_sources):
    self._symbol_type = symbol_type
    self._symbol_data_sources = symbol_data_sources

  def find(self, address_list):
    return find_runtime_symbols.find_runtime_symbols(
        self._symbol_type, self._symbol_data_sources.get(), address_list)


class SymbolMappingCache(object):
  """Caches mapping from actually used addresses to symbols.

  'update()' updates the cache from the original symbol data sources via
  'SymbolFinder'.  Symbols can be looked up by the method 'lookup()'.
  """
  def __init__(self):
    self._symbol_mapping_caches = {
        FUNCTION_SYMBOLS: {},
        SOURCEFILE_SYMBOLS: {},
        TYPEINFO_SYMBOLS: {},
        }

  def update(self, symbol_type, bucket_set, symbol_finder, cache_f):
    """Updates symbol mapping cache on memory and in a symbol cache file.

    It reads cached symbol mapping from a symbol cache file |cache_f| if it
    exists.  Unresolved addresses are then resolved and added to the cache
    both on memory and in the symbol cache file with using 'SymbolFinder'.

    A cache file is formatted as follows:
      <Address> <Symbol>
      <Address> <Symbol>
      <Address> <Symbol>
      ...

    Args:
        symbol_type: A type of symbols to update.  It should be one of
            FUNCTION_SYMBOLS, SOURCEFILE_SYMBOLS and TYPEINFO_SYMBOLS.
        bucket_set: A BucketSet object.
        symbol_finder: A SymbolFinder object to find symbols.
        cache_f: A readable and writable IO object of the symbol cache file.
    """
    cache_f.seek(0, os.SEEK_SET)
    self._load(cache_f, symbol_type)

    unresolved_addresses = sorted(
        address for address in bucket_set.iter_addresses(symbol_type)
        if address not in self._symbol_mapping_caches[symbol_type])

    if not unresolved_addresses:
      LOGGER.info('No need to resolve any more addresses.')
      return

    cache_f.seek(0, os.SEEK_END)
    LOGGER.info('Loading %d unresolved addresses.' %
                len(unresolved_addresses))
    symbol_dict = symbol_finder.find(unresolved_addresses)

    for address, symbol in symbol_dict.iteritems():
      stripped_symbol = symbol.strip() or '?'
      self._symbol_mapping_caches[symbol_type][address] = stripped_symbol
      cache_f.write('%x %s\n' % (address, stripped_symbol))

  def lookup(self, symbol_type, address):
    """Looks up a symbol for a given |address|.

    Args:
        symbol_type: A type of symbols to update.  It should be one of
            FUNCTION_SYMBOLS, SOURCEFILE_SYMBOLS and TYPEINFO_SYMBOLS.
        address: An integer that represents an address.

    Returns:
        A string that represents a symbol.
    """
    return self._symbol_mapping_caches[symbol_type].get(address)

  def _load(self, cache_f, symbol_type):
    try:
      for line in cache_f:
        items = line.rstrip().split(None, 1)
        if len(items) == 1:
          items.append('??')
        self._symbol_mapping_caches[symbol_type][int(items[0], 16)] = items[1]
      LOGGER.info('Loaded %d entries from symbol cache.' %
                     len(self._symbol_mapping_caches[symbol_type]))
    except IOError as e:
      LOGGER.info('The symbol cache file is invalid: %s' % e)
