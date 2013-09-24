#!/usr/bin/env python
# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import sys
import unittest

BASE_PATH = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.append(BASE_PATH)

from lib.range_dict import ExclusiveRangeDict


class ExclusiveRangeDictTest(unittest.TestCase):
  class TestAttribute(ExclusiveRangeDict.RangeAttribute):
    def __init__(self):
      super(ExclusiveRangeDictTest.TestAttribute, self).__init__()
      self._value = 0

    def __str__(self):
      return str(self._value)

    def __repr__(self):
      return '<TestAttribute:%d>' % self._value

    def get(self):
      return self._value

    def set(self, new_value):
      self._value = new_value

    def copy(self):  # pylint: disable=R0201
      new_attr = ExclusiveRangeDictTest.TestAttribute()
      new_attr.set(self._value)
      return new_attr

  def test_init(self):
    ranges = ExclusiveRangeDict(self.TestAttribute)

    result = []
    for begin, end, attr in ranges.iter_range(20, 40):
      result.append({'begin': begin, 'end':end, 'attr':attr.get()})
    expected = [
        {'begin': 20, 'end': 40, 'attr': 0},
        ]
    self.assertEqual(expected, result)

  def test_norange(self):
    ranges = ExclusiveRangeDict(self.TestAttribute)

    result = []
    for begin, end, attr in ranges.iter_range(20, 20):
      result.append({'begin': begin, 'end':end, 'attr':attr.get()})
    expected = []
    self.assertEqual(expected, result)

  def test_set(self):
    ranges = ExclusiveRangeDict(self.TestAttribute)
    for begin, end, attr in ranges.iter_range(20, 30):
      attr.set(12)
    for begin, end, attr in ranges.iter_range(30, 40):
      attr.set(52)

    result = []
    for begin, end, attr in ranges.iter_range(20, 40):
      result.append({'begin': begin, 'end':end, 'attr':attr.get()})
    expected = [
        {'begin': 20, 'end': 30, 'attr': 12},
        {'begin': 30, 'end': 40, 'attr': 52},
        ]
    self.assertEqual(expected, result)

  def test_split(self):
    ranges = ExclusiveRangeDict(self.TestAttribute)
    for begin, end, attr in ranges.iter_range(20, 30):
      attr.set(1000)
    for begin, end, attr in ranges.iter_range(30, 40):
      attr.set(2345)
    for begin, end, attr in ranges.iter_range(40, 50):
      attr.set(3579)

    result1 = []
    for begin, end, attr in ranges.iter_range(25, 45):
      result1.append({'begin': begin, 'end':end, 'attr':attr.get()})
    expected1 = [
        {'begin': 25, 'end': 30, 'attr': 1000},
        {'begin': 30, 'end': 40, 'attr': 2345},
        {'begin': 40, 'end': 45, 'attr': 3579},
        ]
    self.assertEqual(expected1, result1)

    result2 = []
    for begin, end, attr in ranges.iter_range(20, 50):
      result2.append({'begin': begin, 'end':end, 'attr':attr.get()})
    expected2 = [
        {'begin': 20, 'end': 25, 'attr': 1000},
        {'begin': 25, 'end': 30, 'attr': 1000},
        {'begin': 30, 'end': 40, 'attr': 2345},
        {'begin': 40, 'end': 45, 'attr': 3579},
        {'begin': 45, 'end': 50, 'attr': 3579},
        ]
    self.assertEqual(expected2, result2)

  def test_fill(self):
    ranges = ExclusiveRangeDict(self.TestAttribute)
    for begin, end, attr in ranges.iter_range(30, 35):
      attr.set(12345)
    for begin, end, attr in ranges.iter_range(40, 45):
      attr.set(97531)

    result = []
    for begin, end, attr in ranges.iter_range(25, 50):
      result.append({'begin': begin, 'end':end, 'attr':attr.get()})
    expected = [
        {'begin': 25, 'end': 30, 'attr': 0},
        {'begin': 30, 'end': 35, 'attr': 12345},
        {'begin': 35, 'end': 40, 'attr': 0},
        {'begin': 40, 'end': 45, 'attr': 97531},
        {'begin': 45, 'end': 50, 'attr': 0},
        ]
    self.assertEqual(expected, result)


if __name__ == '__main__':
  logging.basicConfig(
      level=logging.DEBUG if '-v' in sys.argv else logging.ERROR,
      format='%(levelname)5s %(filename)15s(%(lineno)3d): %(message)s')
  unittest.main()
