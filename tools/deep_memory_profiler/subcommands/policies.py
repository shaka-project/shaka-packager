# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import datetime
import json
import logging
import sys

from lib.bucket import BUCKET_ID, COMMITTED
from lib.pageframe import PFNCounts
from lib.policy import PolicySet
from lib.subcommand import SubCommand


LOGGER = logging.getLogger('dmprof')


class PolicyCommands(SubCommand):
  def __init__(self, command):
    super(PolicyCommands, self).__init__(
        'Usage: %%prog %s [-p POLICY] <first-dump> [shared-first-dumps...]' %
        command)
    self._parser.add_option('-p', '--policy', type='string', dest='policy',
                            help='profile with POLICY', metavar='POLICY')
    self._parser.add_option('--alternative-dirs', dest='alternative_dirs',
                            metavar='/path/on/target@/path/on/host[:...]',
                            help='Read files in /path/on/host/ instead of '
                                 'files in /path/on/target/.')

  def _set_up(self, sys_argv):
    options, args = self._parse_args(sys_argv, 1)
    dump_path = args[1]
    shared_first_dump_paths = args[2:]
    alternative_dirs_dict = {}
    if options.alternative_dirs:
      for alternative_dir_pair in options.alternative_dirs.split(':'):
        target_path, host_path = alternative_dir_pair.split('@', 1)
        alternative_dirs_dict[target_path] = host_path
    (bucket_set, dumps) = SubCommand.load_basic_files(
        dump_path, True, alternative_dirs=alternative_dirs_dict)

    pfn_counts_dict = {}
    for shared_first_dump_path in shared_first_dump_paths:
      shared_dumps = SubCommand._find_all_dumps(shared_first_dump_path)
      for shared_dump in shared_dumps:
        pfn_counts = PFNCounts.load(shared_dump)
        if pfn_counts.pid not in pfn_counts_dict:
          pfn_counts_dict[pfn_counts.pid] = []
        pfn_counts_dict[pfn_counts.pid].append(pfn_counts)

    policy_set = PolicySet.load(SubCommand._parse_policy_list(options.policy))
    return policy_set, dumps, pfn_counts_dict, bucket_set

  @staticmethod
  def _apply_policy(dump, pfn_counts_dict, policy, bucket_set, first_dump_time):
    """Aggregates the total memory size of each component.

    Iterate through all stacktraces and attribute them to one of the components
    based on the policy.  It is important to apply policy in right order.

    Args:
        dump: A Dump object.
        pfn_counts_dict: A dict mapping a pid to a list of PFNCounts.
        policy: A Policy object.
        bucket_set: A BucketSet object.
        first_dump_time: An integer representing time when the first dump is
            dumped.

    Returns:
        A dict mapping components and their corresponding sizes.
    """
    LOGGER.info('  %s' % dump.path)
    all_pfn_dict = {}
    if pfn_counts_dict:
      LOGGER.info('    shared with...')
      for pid, pfnset_list in pfn_counts_dict.iteritems():
        closest_pfnset_index = None
        closest_pfnset_difference = 1024.0
        for index, pfnset in enumerate(pfnset_list):
          time_difference = pfnset.time - dump.time
          if time_difference >= 3.0:
            break
          elif ((time_difference < 0.0 and pfnset.reason != 'Exiting') or
                (0.0 <= time_difference and time_difference < 3.0)):
            closest_pfnset_index = index
            closest_pfnset_difference = time_difference
          elif time_difference < 0.0 and pfnset.reason == 'Exiting':
            closest_pfnset_index = None
            break
        if closest_pfnset_index:
          for pfn, count in pfnset_list[closest_pfnset_index].iter_pfn:
            all_pfn_dict[pfn] = all_pfn_dict.get(pfn, 0) + count
          LOGGER.info('      %s (time difference = %f)' %
                      (pfnset_list[closest_pfnset_index].path,
                       closest_pfnset_difference))
        else:
          LOGGER.info('      (no match with pid:%d)' % pid)

    sizes = dict((c, 0) for c in policy.components)

    PolicyCommands._accumulate_malloc(dump, policy, bucket_set, sizes)
    verify_global_stats = PolicyCommands._accumulate_maps(
        dump, all_pfn_dict, policy, bucket_set, sizes)

    # TODO(dmikurube): Remove the verifying code when GLOBAL_STATS is removed.
    # http://crbug.com/245603.
    for verify_key, verify_value in verify_global_stats.iteritems():
      dump_value = dump.global_stat('%s_committed' % verify_key)
      if dump_value != verify_value:
        LOGGER.warn('%25s: %12d != %d (%d)' % (
            verify_key, dump_value, verify_value, dump_value - verify_value))

    sizes['mmap-no-log'] = (
        dump.global_stat('profiled-mmap_committed') -
        sizes['mmap-total-log'])
    sizes['mmap-total-record'] = dump.global_stat('profiled-mmap_committed')
    sizes['mmap-total-record-vm'] = dump.global_stat('profiled-mmap_virtual')

    sizes['tc-no-log'] = (
        dump.global_stat('profiled-malloc_committed') -
        sizes['tc-total-log'])
    sizes['tc-total-record'] = dump.global_stat('profiled-malloc_committed')
    sizes['tc-unused'] = (
        sizes['mmap-tcmalloc'] -
        dump.global_stat('profiled-malloc_committed'))
    if sizes['tc-unused'] < 0:
      LOGGER.warn('    Assuming tc-unused=0 as it is negative: %d (bytes)' %
                  sizes['tc-unused'])
      sizes['tc-unused'] = 0
    sizes['tc-total'] = sizes['mmap-tcmalloc']

    # TODO(dmikurube): global_stat will be deprecated.
    # See http://crbug.com/245603.
    for key, value in {
        'total': 'total_committed',
        'filemapped': 'file_committed',
        'absent': 'absent_committed',
        'file-exec': 'file-exec_committed',
        'file-nonexec': 'file-nonexec_committed',
        'anonymous': 'anonymous_committed',
        'stack': 'stack_committed',
        'other': 'other_committed',
        'unhooked-absent': 'nonprofiled-absent_committed',
        'total-vm': 'total_virtual',
        'filemapped-vm': 'file_virtual',
        'anonymous-vm': 'anonymous_virtual',
        'other-vm': 'other_virtual' }.iteritems():
      if key in sizes:
        sizes[key] = dump.global_stat(value)

    if 'mustbezero' in sizes:
      removed_list = (
          'profiled-mmap_committed',
          'nonprofiled-absent_committed',
          'nonprofiled-anonymous_committed',
          'nonprofiled-file-exec_committed',
          'nonprofiled-file-nonexec_committed',
          'nonprofiled-stack_committed',
          'nonprofiled-other_committed')
      sizes['mustbezero'] = (
          dump.global_stat('total_committed') -
          sum(dump.global_stat(removed) for removed in removed_list))
    if 'total-exclude-profiler' in sizes:
      sizes['total-exclude-profiler'] = (
          dump.global_stat('total_committed') -
          (sizes['mmap-profiler'] + sizes['mmap-type-profiler']))
    if 'hour' in sizes:
      sizes['hour'] = (dump.time - first_dump_time) / 60.0 / 60.0
    if 'minute' in sizes:
      sizes['minute'] = (dump.time - first_dump_time) / 60.0
    if 'second' in sizes:
      sizes['second'] = dump.time - first_dump_time

    return sizes

  @staticmethod
  def _accumulate_malloc(dump, policy, bucket_set, sizes):
    for line in dump.iter_stacktrace:
      words = line.split()
      bucket = bucket_set.get(int(words[BUCKET_ID]))
      if not bucket or bucket.allocator_type == 'malloc':
        component_match = policy.find_malloc(bucket)
      elif bucket.allocator_type == 'mmap':
        continue
      else:
        assert False
      sizes[component_match] += int(words[COMMITTED])

      assert not component_match.startswith('mmap-')
      if component_match.startswith('tc-'):
        sizes['tc-total-log'] += int(words[COMMITTED])
      else:
        sizes['other-total-log'] += int(words[COMMITTED])

  @staticmethod
  def _accumulate_maps(dump, pfn_dict, policy, bucket_set, sizes):
    # TODO(dmikurube): Remove the dict when GLOBAL_STATS is removed.
    # http://crbug.com/245603.
    global_stats = {
        'total': 0,
        'file-exec': 0,
        'file-nonexec': 0,
        'anonymous': 0,
        'stack': 0,
        'other': 0,
        'nonprofiled-file-exec': 0,
        'nonprofiled-file-nonexec': 0,
        'nonprofiled-anonymous': 0,
        'nonprofiled-stack': 0,
        'nonprofiled-other': 0,
        'profiled-mmap': 0,
        }

    for key, value in dump.iter_map:
      # TODO(dmikurube): Remove the subtotal code when GLOBAL_STATS is removed.
      # It's temporary verification code for transition described in
      # http://crbug.com/245603.
      committed = 0
      if 'committed' in value[1]:
        committed = value[1]['committed']
      global_stats['total'] += committed
      key = 'other'
      name = value[1]['vma']['name']
      if name.startswith('/'):
        if value[1]['vma']['executable'] == 'x':
          key = 'file-exec'
        else:
          key = 'file-nonexec'
      elif name == '[stack]':
        key = 'stack'
      elif name == '':
        key = 'anonymous'
      global_stats[key] += committed
      if value[0] == 'unhooked':
        global_stats['nonprofiled-' + key] += committed
      if value[0] == 'hooked':
        global_stats['profiled-mmap'] += committed

      if value[0] == 'unhooked':
        if pfn_dict and dump.pageframe_length:
          for pageframe in value[1]['pageframe']:
            component_match = policy.find_unhooked(value, pageframe, pfn_dict)
            sizes[component_match] += pageframe.size
        else:
          component_match = policy.find_unhooked(value)
          sizes[component_match] += int(value[1]['committed'])
      elif value[0] == 'hooked':
        if pfn_dict and dump.pageframe_length:
          for pageframe in value[1]['pageframe']:
            component_match, _ = policy.find_mmap(
                value, bucket_set, pageframe, pfn_dict)
            sizes[component_match] += pageframe.size
            assert not component_match.startswith('tc-')
            if component_match.startswith('mmap-'):
              sizes['mmap-total-log'] += pageframe.size
            else:
              sizes['other-total-log'] += pageframe.size
        else:
          component_match, _ = policy.find_mmap(value, bucket_set)
          sizes[component_match] += int(value[1]['committed'])
          if component_match.startswith('mmap-'):
            sizes['mmap-total-log'] += int(value[1]['committed'])
          else:
            sizes['other-total-log'] += int(value[1]['committed'])
      else:
        LOGGER.error('Unrecognized mapping status: %s' % value[0])

    return global_stats


class CSVCommand(PolicyCommands):
  def __init__(self):
    super(CSVCommand, self).__init__('csv')

  def do(self, sys_argv):
    policy_set, dumps, pfn_counts_dict, bucket_set = self._set_up(sys_argv)
    return CSVCommand._output(
        policy_set, dumps, pfn_counts_dict, bucket_set, sys.stdout)

  @staticmethod
  def _output(policy_set, dumps, pfn_counts_dict, bucket_set, out):
    max_components = 0
    for label in policy_set:
      max_components = max(max_components, len(policy_set[label].components))

    for label in sorted(policy_set):
      components = policy_set[label].components
      if len(policy_set) > 1:
        out.write('%s%s\n' % (label, ',' * (max_components - 1)))
      out.write('%s%s\n' % (
          ','.join(components), ',' * (max_components - len(components))))

      LOGGER.info('Applying a policy %s to...' % label)
      for dump in dumps:
        component_sizes = PolicyCommands._apply_policy(
            dump, pfn_counts_dict, policy_set[label], bucket_set, dumps[0].time)
        s = []
        for c in components:
          if c in ('hour', 'minute', 'second'):
            s.append('%05.5f' % (component_sizes[c]))
          else:
            s.append('%05.5f' % (component_sizes[c] / 1024.0 / 1024.0))
        out.write('%s%s\n' % (
              ','.join(s), ',' * (max_components - len(components))))

      bucket_set.clear_component_cache()

    return 0


class JSONCommand(PolicyCommands):
  def __init__(self):
    super(JSONCommand, self).__init__('json')

  def do(self, sys_argv):
    policy_set, dumps, pfn_counts_dict, bucket_set = self._set_up(sys_argv)
    return JSONCommand._output(
        policy_set, dumps, pfn_counts_dict, bucket_set, sys.stdout)

  @staticmethod
  def _output(policy_set, dumps, pfn_counts_dict, bucket_set, out):
    json_base = {
      'version': 'JSON_DEEP_2',
      'policies': {},
    }

    for label in sorted(policy_set):
      json_base['policies'][label] = {
        'legends': policy_set[label].components,
        'snapshots': [],
      }

      LOGGER.info('Applying a policy %s to...' % label)
      for dump in dumps:
        component_sizes = PolicyCommands._apply_policy(
            dump, pfn_counts_dict, policy_set[label], bucket_set, dumps[0].time)
        component_sizes['dump_path'] = dump.path
        component_sizes['dump_time'] = datetime.datetime.fromtimestamp(
            dump.time).strftime('%Y-%m-%d %H:%M:%S')
        json_base['policies'][label]['snapshots'].append(component_sizes)

      bucket_set.clear_component_cache()

    json.dump(json_base, out, indent=2, sort_keys=True)

    return 0


class ListCommand(PolicyCommands):
  def __init__(self):
    super(ListCommand, self).__init__('list')

  def do(self, sys_argv):
    policy_set, dumps, pfn_counts_dict, bucket_set = self._set_up(sys_argv)
    return ListCommand._output(
        policy_set, dumps, pfn_counts_dict, bucket_set, sys.stdout)

  @staticmethod
  def _output(policy_set, dumps, pfn_counts_dict, bucket_set, out):
    for label in sorted(policy_set):
      LOGGER.info('Applying a policy %s to...' % label)
      for dump in dumps:
        component_sizes = PolicyCommands._apply_policy(
            dump, pfn_counts_dict, policy_set[label], bucket_set, dump.time)
        out.write('%s for %s:\n' % (label, dump.path))
        for c in policy_set[label].components:
          if c in ['hour', 'minute', 'second']:
            out.write('%40s %12.3f\n' % (c, component_sizes[c]))
          else:
            out.write('%40s %12d\n' % (c, component_sizes[c]))

      bucket_set.clear_component_cache()

    return 0
