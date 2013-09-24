#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import cStringIO
import logging
import os
import sys
import textwrap
import unittest

BASE_PATH = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.append(BASE_PATH)

from lib.bucket import Bucket
from lib.ordered_dict import OrderedDict
from lib.policy import Policy
from lib.symbol import SymbolMappingCache
from lib.symbol import FUNCTION_SYMBOLS, SOURCEFILE_SYMBOLS, TYPEINFO_SYMBOLS

import subcommands


class SymbolMappingCacheTest(unittest.TestCase):
  class MockBucketSet(object):
    def __init__(self, addresses):
      self._addresses = addresses

    def iter_addresses(self, symbol_type):  # pylint: disable=W0613
      for address in self._addresses:
        yield address

  class MockSymbolFinder(object):
    def __init__(self, mapping):
      self._mapping = mapping

    def find(self, address_list):
      result = OrderedDict()
      for address in address_list:
        result[address] = self._mapping[address]
      return result

  _TEST_FUNCTION_CACHE = textwrap.dedent("""\
      1 0x0000000000000001
      7fc33eebcaa4 __gnu_cxx::new_allocator::allocate
      7fc33ef69242 void DispatchToMethod
      """)

  _EXPECTED_TEST_FUNCTION_CACHE = textwrap.dedent("""\
      1 0x0000000000000001
      7fc33eebcaa4 __gnu_cxx::new_allocator::allocate
      7fc33ef69242 void DispatchToMethod
      2 0x0000000000000002
      7fc33ef7bc3e std::map::operator[]
      7fc34411f9d5 WTF::RefCounted::operator new
      """)

  _TEST_FUNCTION_ADDRESS_LIST1 = [
      0x1, 0x7fc33eebcaa4, 0x7fc33ef69242]

  _TEST_FUNCTION_ADDRESS_LIST2 = [
      0x1, 0x2, 0x7fc33eebcaa4, 0x7fc33ef69242, 0x7fc33ef7bc3e, 0x7fc34411f9d5]

  _TEST_FUNCTION_DICT = {
      0x1: '0x0000000000000001',
      0x2: '0x0000000000000002',
      0x7fc33eebcaa4: '__gnu_cxx::new_allocator::allocate',
      0x7fc33ef69242: 'void DispatchToMethod',
      0x7fc33ef7bc3e: 'std::map::operator[]',
      0x7fc34411f9d5: 'WTF::RefCounted::operator new',
  }

  def test_update(self):
    symbol_mapping_cache = SymbolMappingCache()
    cache_f = cStringIO.StringIO()
    cache_f.write(self._TEST_FUNCTION_CACHE)

    # No update from self._TEST_FUNCTION_CACHE
    symbol_mapping_cache.update(
        FUNCTION_SYMBOLS,
        self.MockBucketSet(self._TEST_FUNCTION_ADDRESS_LIST1),
        self.MockSymbolFinder(self._TEST_FUNCTION_DICT), cache_f)
    for address in self._TEST_FUNCTION_ADDRESS_LIST1:
      self.assertEqual(self._TEST_FUNCTION_DICT[address],
                       symbol_mapping_cache.lookup(FUNCTION_SYMBOLS, address))
    self.assertEqual(self._TEST_FUNCTION_CACHE, cache_f.getvalue())

    # Update to self._TEST_FUNCTION_ADDRESS_LIST2
    symbol_mapping_cache.update(
        FUNCTION_SYMBOLS,
        self.MockBucketSet(self._TEST_FUNCTION_ADDRESS_LIST2),
        self.MockSymbolFinder(self._TEST_FUNCTION_DICT), cache_f)
    for address in self._TEST_FUNCTION_ADDRESS_LIST2:
      self.assertEqual(self._TEST_FUNCTION_DICT[address],
                       symbol_mapping_cache.lookup(FUNCTION_SYMBOLS, address))
    self.assertEqual(self._EXPECTED_TEST_FUNCTION_CACHE, cache_f.getvalue())


class PolicyTest(unittest.TestCase):
  class MockSymbolMappingCache(object):
    def __init__(self):
      self._symbol_caches = {
          FUNCTION_SYMBOLS: {},
          SOURCEFILE_SYMBOLS: {},
          TYPEINFO_SYMBOLS: {},
          }

    def add(self, symbol_type, address, symbol):
      self._symbol_caches[symbol_type][address] = symbol

    def lookup(self, symbol_type, address):
      symbol = self._symbol_caches[symbol_type].get(address)
      return symbol if symbol else '0x%016x' % address

  _TEST_POLICY = textwrap.dedent("""\
      {
        "components": [
          "second",
          "mmap-v8",
          "malloc-v8",
          "malloc-WebKit",
          "mmap-catch-all",
          "malloc-catch-all"
        ],
        "rules": [
          {
            "name": "second",
            "stacktrace": "optional",
            "allocator": "optional"
          },
          {
            "name": "mmap-v8",
            "stacktrace": ".*v8::.*",
            "allocator": "mmap"
          },
          {
            "name": "malloc-v8",
            "stacktrace": ".*v8::.*",
            "allocator": "malloc"
          },
          {
            "name": "malloc-WebKit",
            "stacktrace": ".*WebKit::.*",
            "allocator": "malloc"
          },
          {
            "name": "mmap-catch-all",
            "stacktrace": ".*",
            "allocator": "mmap"
          },
          {
            "name": "malloc-catch-all",
            "stacktrace": ".*",
            "allocator": "malloc"
          }
        ],
        "version": "POLICY_DEEP_3"
      }
      """)

  def test_load(self):
    policy = Policy.parse(cStringIO.StringIO(self._TEST_POLICY), 'json')
    self.assertTrue(policy)
    self.assertEqual('POLICY_DEEP_3', policy.version)

  def test_find(self):
    policy = Policy.parse(cStringIO.StringIO(self._TEST_POLICY), 'json')
    self.assertTrue(policy)

    symbol_mapping_cache = self.MockSymbolMappingCache()
    symbol_mapping_cache.add(FUNCTION_SYMBOLS, 0x1212, 'v8::create')
    symbol_mapping_cache.add(FUNCTION_SYMBOLS, 0x1381, 'WebKit::create')

    bucket1 = Bucket([0x1212, 0x013], 'malloc', 0x29492, '_Z')
    bucket1.symbolize(symbol_mapping_cache)
    bucket2 = Bucket([0x18242, 0x1381], 'malloc', 0x9492, '_Z')
    bucket2.symbolize(symbol_mapping_cache)
    bucket3 = Bucket([0x18242, 0x181], 'malloc', 0x949, '_Z')
    bucket3.symbolize(symbol_mapping_cache)

    self.assertEqual('malloc-v8', policy.find_malloc(bucket1))
    self.assertEqual('malloc-WebKit', policy.find_malloc(bucket2))
    self.assertEqual('malloc-catch-all', policy.find_malloc(bucket3))


class BucketsCommandTest(unittest.TestCase):
  def test(self):
    BUCKETS_PATH = os.path.join(BASE_PATH, 'tests', 'output', 'buckets')
    with open(BUCKETS_PATH) as output_f:
      expected = output_f.read()

    out = cStringIO.StringIO()

    HEAP_PATH = os.path.join(BASE_PATH, 'tests', 'data', 'heap.01234.0001.heap')
    subcommand = subcommands.BucketsCommand()
    returncode = subcommand.do(['buckets', HEAP_PATH], out)
    self.assertEqual(0, returncode)
    self.assertEqual(expected, out.getvalue())


class UploadCommandTest(unittest.TestCase):
  def test(self):
    MOCK_GSUTIL_PATH = os.path.join(BASE_PATH, 'tests', 'mock_gsutil.py')
    HEAP_PATH = os.path.join(BASE_PATH, 'tests', 'data', 'heap.01234.0001.heap')
    subcommand = subcommands.UploadCommand()
    returncode = subcommand.do([
        'upload',
         '--gsutil',
        MOCK_GSUTIL_PATH,
        HEAP_PATH,
        'gs://test-storage/'])
    self.assertEqual(0, returncode)


if __name__ == '__main__':
  logging.basicConfig(
      level=logging.DEBUG if '-v' in sys.argv else logging.ERROR,
      format='%(levelname)5s %(filename)15s(%(lineno)3d): %(message)s')
  unittest.main()
