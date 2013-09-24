# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import sys

from lib.bucket import BUCKET_ID, COMMITTED, ALLOC_COUNT, FREE_COUNT
from lib.policy import PolicySet
from lib.subcommand import SubCommand


LOGGER = logging.getLogger('dmprof')


class PProfCommand(SubCommand):
  def __init__(self):
    super(PProfCommand, self).__init__(
        'Usage: %prog pprof [-c COMPONENT] <dump> <policy>')
    self._parser.add_option('-c', '--component', type='string',
                            dest='component',
                            help='restrict to COMPONENT', metavar='COMPONENT')

  def do(self, sys_argv):
    options, args = self._parse_args(sys_argv, 2)

    dump_path = args[1]
    target_policy = args[2]
    component = options.component

    (bucket_set, dump) = SubCommand.load_basic_files(dump_path, False)
    policy_set = PolicySet.load(SubCommand._parse_policy_list(target_policy))

    with open(SubCommand._find_prefix(dump_path) + '.maps', 'r') as maps_f:
      maps_lines = maps_f.readlines()
    PProfCommand._output(
        dump, policy_set[target_policy], bucket_set, maps_lines, component,
        sys.stdout)

    return 0

  @staticmethod
  def _output(dump, policy, bucket_set, maps_lines, component_name, out):
    """Converts the heap profile dump so it can be processed by pprof.

    Args:
        dump: A Dump object.
        policy: A Policy object.
        bucket_set: A BucketSet object.
        maps_lines: A list of strings containing /proc/.../maps.
        component_name: A name of component for filtering.
        out: An IO object to output.
    """
    out.write('heap profile: ')
    com_committed, com_allocs = PProfCommand._accumulate(
        dump, policy, bucket_set, component_name)

    out.write('%6d: %8s [%6d: %8s] @ heapprofile\n' % (
        com_allocs, com_committed, com_allocs, com_committed))

    PProfCommand._output_stacktrace_lines(
        dump, policy, bucket_set, component_name, out)

    out.write('MAPPED_LIBRARIES:\n')
    for line in maps_lines:
      out.write(line)

  @staticmethod
  def _accumulate(dump, policy, bucket_set, component_name):
    """Accumulates size of committed chunks and the number of allocated chunks.

    Args:
        dump: A Dump object.
        policy: A Policy object.
        bucket_set: A BucketSet object.
        component_name: A name of component for filtering.

    Returns:
        Two integers which are the accumulated size of committed regions and the
        number of allocated chunks, respectively.
    """
    com_committed = 0
    com_allocs = 0

    for _, region in dump.iter_map:
      if region[0] != 'hooked':
        continue
      component_match, bucket = policy.find_mmap(region, bucket_set)

      if (component_name and component_name != component_match) or (
          region[1]['committed'] == 0):
        continue

      com_committed += region[1]['committed']
      com_allocs += 1

    for line in dump.iter_stacktrace:
      words = line.split()
      bucket = bucket_set.get(int(words[BUCKET_ID]))
      if not bucket or bucket.allocator_type == 'malloc':
        component_match = policy.find_malloc(bucket)
      elif bucket.allocator_type == 'mmap':
        continue
      else:
        assert False
      if (not bucket or
          (component_name and component_name != component_match)):
        continue

      com_committed += int(words[COMMITTED])
      com_allocs += int(words[ALLOC_COUNT]) - int(words[FREE_COUNT])

    return com_committed, com_allocs

  @staticmethod
  def _output_stacktrace_lines(dump, policy, bucket_set, component_name, out):
    """Prints information of stacktrace lines for pprof.

    Args:
        dump: A Dump object.
        policy: A Policy object.
        bucket_set: A BucketSet object.
        component_name: A name of component for filtering.
        out: An IO object to output.
    """
    for _, region in dump.iter_map:
      if region[0] != 'hooked':
        continue
      component_match, bucket = policy.find_mmap(region, bucket_set)

      if (component_name and component_name != component_match) or (
          region[1]['committed'] == 0):
        continue

      out.write('     1: %8s [     1: %8s] @' % (
          region[1]['committed'], region[1]['committed']))
      for address in bucket.stacktrace:
        out.write(' 0x%016x' % address)
      out.write('\n')

    for line in dump.iter_stacktrace:
      words = line.split()
      bucket = bucket_set.get(int(words[BUCKET_ID]))
      if not bucket or bucket.allocator_type == 'malloc':
        component_match = policy.find_malloc(bucket)
      elif bucket.allocator_type == 'mmap':
        continue
      else:
        assert False
      if (not bucket or
          (component_name and component_name != component_match)):
        continue

      out.write('%6d: %8s [%6d: %8s] @' % (
          int(words[ALLOC_COUNT]) - int(words[FREE_COUNT]),
          words[COMMITTED],
          int(words[ALLOC_COUNT]) - int(words[FREE_COUNT]),
          words[COMMITTED]))
      for address in bucket.stacktrace:
        out.write(' 0x%016x' % address)
      out.write('\n')
