# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import logging
import sys

from lib.bucket import BUCKET_ID, COMMITTED, ALLOC_COUNT, FREE_COUNT
from lib.ordered_dict import OrderedDict
from lib.subcommand import SubCommand
from lib.sorter import MallocUnit, MMapUnit, SorterSet, UnhookedUnit, UnitSet


LOGGER = logging.getLogger('dmprof')


class CatCommand(SubCommand):
  def __init__(self):
    super(CatCommand, self).__init__('Usage: %prog cat <first-dump>')
    self._parser.add_option('--alternative-dirs', dest='alternative_dirs',
                            metavar='/path/on/target@/path/on/host[:...]',
                            help='Read files in /path/on/host/ instead of '
                                 'files in /path/on/target/.')
    self._parser.add_option('--indent', dest='indent', action='store_true',
                            help='Indent the output.')

  def do(self, sys_argv):
    options, args = self._parse_args(sys_argv, 1)
    dump_path = args[1]
    # TODO(dmikurube): Support shared memory.
    alternative_dirs_dict = {}
    if options.alternative_dirs:
      for alternative_dir_pair in options.alternative_dirs.split(':'):
        target_path, host_path = alternative_dir_pair.split('@', 1)
        alternative_dirs_dict[target_path] = host_path
    (bucket_set, dumps) = SubCommand.load_basic_files(
        dump_path, True, alternative_dirs=alternative_dirs_dict)

    # Load all sorters.
    sorters = SorterSet()

    json_root = OrderedDict()
    json_root['version'] = 1
    json_root['run_id'] = None
    for dump in dumps:
      if json_root['run_id'] and json_root['run_id'] != dump.run_id:
        LOGGER.error('Inconsistent heap profile dumps.')
        json_root['run_id'] = ''
        break
      json_root['run_id'] = dump.run_id
    json_root['roots'] = []
    for sorter in sorters:
      if sorter.root:
        json_root['roots'].append([sorter.world, sorter.name])
    json_root['default_template'] = 'l2'
    json_root['templates'] = sorters.templates.as_dict()

    json_root['snapshots'] = []

    for dump in dumps:
      json_root['snapshots'].append(
          self._fill_snapshot(dump, bucket_set, sorters))

    if options.indent:
      json.dump(json_root, sys.stdout, indent=2)
    else:
      json.dump(json_root, sys.stdout)
    print ''

  @staticmethod
  def _fill_snapshot(dump, bucket_set, sorters):
    root = OrderedDict()
    root['time'] = dump.time
    root['worlds'] = OrderedDict()
    root['worlds']['vm'] = CatCommand._fill_world(
        dump, bucket_set, sorters, 'vm')
    root['worlds']['malloc'] = CatCommand._fill_world(
        dump, bucket_set, sorters, 'malloc')
    return root

  @staticmethod
  def _fill_world(dump, bucket_set, sorters, world):
    root = OrderedDict()

    root['name'] = world
    if world == 'vm':
      root['unit_fields'] = ['size', 'reserved']
    elif world == 'malloc':
      root['unit_fields'] = ['size', 'alloc_count', 'free_count']

    # Make { vm | malloc } units with their sizes.
    root['units'] = OrderedDict()
    unit_set = UnitSet(world)
    if world == 'vm':
      for unit in CatCommand._iterate_vm_unit(dump, None, bucket_set):
        unit_set.append(unit)
      for unit in unit_set:
        root['units'][unit.unit_id] = [unit.committed, unit.reserved]
    elif world == 'malloc':
      for unit in CatCommand._iterate_malloc_unit(dump, bucket_set):
        unit_set.append(unit)
      for unit in unit_set:
        root['units'][unit.unit_id] = [
            unit.size, unit.alloc_count, unit.free_count]

    # Iterate for { vm | malloc } sorters.
    root['breakdown'] = OrderedDict()
    for sorter in sorters.iter_world(world):
      breakdown = OrderedDict()
      for unit in unit_set:
        found = sorter.find(unit)
        if found.name not in breakdown:
          category = OrderedDict()
          category['name'] = found.name
          category['color'] = 'random'
          subs = []
          for sub_world, sub_breakdown in found.iter_subs():
            subs.append([sub_world, sub_breakdown])
          if subs:
            category['subs'] = subs
          if found.hidden:
            category['hidden'] = True
          category['units'] = []
          breakdown[found.name] = category
        breakdown[found.name]['units'].append(unit.unit_id)
      root['breakdown'][sorter.name] = breakdown

    return root

  @staticmethod
  def _iterate_vm_unit(dump, pfn_dict, bucket_set):
    unit_id = 0
    for _, region in dump.iter_map:
      unit_id += 1
      if region[0] == 'unhooked':
        if pfn_dict and dump.pageframe_length:
          for pageframe in region[1]['pageframe']:
            yield UnhookedUnit(unit_id, pageframe.size, pageframe.size,
                               region, pageframe, pfn_dict)
        else:
          yield UnhookedUnit(unit_id,
                             int(region[1]['committed']),
                             int(region[1]['reserved']),
                             region)
      elif region[0] == 'hooked':
        if pfn_dict and dump.pageframe_length:
          for pageframe in region[1]['pageframe']:
            yield MMapUnit(unit_id,
                           pageframe.size,
                           pageframe.size,
                           region, bucket_set, pageframe, pfn_dict)
        else:
          yield MMapUnit(unit_id,
                         int(region[1]['committed']),
                         int(region[1]['reserved']),
                         region,
                         bucket_set)
      else:
        LOGGER.error('Unrecognized mapping status: %s' % region[0])

  @staticmethod
  def _iterate_malloc_unit(dump, bucket_set):
    for line in dump.iter_stacktrace:
      words = line.split()
      bucket = bucket_set.get(int(words[BUCKET_ID]))
      if bucket and bucket.allocator_type == 'malloc':
        yield MallocUnit(int(words[BUCKET_ID]),
                         int(words[COMMITTED]),
                         int(words[ALLOC_COUNT]),
                         int(words[FREE_COUNT]),
                         bucket)
      elif not bucket:
        # 'Not-found' buckets are all assumed as malloc buckets.
        yield MallocUnit(int(words[BUCKET_ID]),
                         int(words[COMMITTED]),
                         int(words[ALLOC_COUNT]),
                         int(words[FREE_COUNT]),
                         None)
