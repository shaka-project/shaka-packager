# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import copy
import logging
import sys

from lib.range_dict import ExclusiveRangeDict
from lib.policy import PolicySet
from lib.subcommand import SubCommand


LOGGER = logging.getLogger('dmprof')


class MapCommand(SubCommand):
  def __init__(self):
    super(MapCommand, self).__init__('Usage: %prog map <first-dump> <policy>')

  def do(self, sys_argv, out=sys.stdout):
    _, args = self._parse_args(sys_argv, 2)
    dump_path = args[1]
    target_policy = args[2]
    (bucket_set, dumps) = SubCommand.load_basic_files(dump_path, True)
    policy_set = PolicySet.load(SubCommand._parse_policy_list(target_policy))

    MapCommand._output(dumps, bucket_set, policy_set[target_policy], out)
    return 0

  @staticmethod
  def _output(dumps, bucket_set, policy, out):
    """Prints all stacktraces in a given component of given depth.

    Args:
        dumps: A list of Dump objects.
        bucket_set: A BucketSet object.
        policy: A Policy object.
        out: An IO object to output.
    """
    max_dump_count = 0
    range_dict = ExclusiveRangeDict(ListAttribute)
    for dump in dumps:
      max_dump_count = max(max_dump_count, dump.count)
      for key, value in dump.iter_map:
        for begin, end, attr in range_dict.iter_range(key[0], key[1]):
          attr[dump.count] = value

    max_dump_count_digit = len(str(max_dump_count))
    for begin, end, attr in range_dict.iter_range():
      out.write('%x-%x\n' % (begin, end))
      if len(attr) < max_dump_count:
        attr[max_dump_count] = None
      for index, value in enumerate(attr[1:]):
        out.write('  #%0*d: ' % (max_dump_count_digit, index + 1))
        if not value:
          out.write('None\n')
        elif value[0] == 'hooked':
          component_match, _ = policy.find_mmap(value, bucket_set)
          out.write('%s @ %d\n' % (component_match, value[1]['bucket_id']))
        else:
          component_match = policy.find_unhooked(value)
          region_info = value[1]
          size = region_info['committed']
          out.write('%s [%d bytes] %s%s%s%s %s\n' % (
              component_match, size, value[1]['vma']['readable'],
              value[1]['vma']['writable'], value[1]['vma']['executable'],
              value[1]['vma']['private'], value[1]['vma']['name']))


class ListAttribute(ExclusiveRangeDict.RangeAttribute):
  """Represents a list for an attribute in range_dict.ExclusiveRangeDict."""
  def __init__(self):
    super(ListAttribute, self).__init__()
    self._list = []

  def __str__(self):
    return str(self._list)

  def __repr__(self):
    return 'ListAttribute' + str(self._list)

  def __len__(self):
    return len(self._list)

  def __iter__(self):
    for x in self._list:
      yield x

  def __getitem__(self, index):
    return self._list[index]

  def __setitem__(self, index, value):
    if index >= len(self._list):
      self._list.extend([None] * (index + 1 - len(self._list)))
    self._list[index] = value

  def copy(self):
    new_list = ListAttribute()
    for index, item in enumerate(self._list):
      new_list[index] = copy.deepcopy(item)
    return new_list
