# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import cStringIO
import json
import logging
import os
import re


LOGGER = logging.getLogger('dmprof')

BASE_PATH = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

DEFAULT_SORTERS = [
    os.path.join(BASE_PATH, 'sorter.malloc-component.json'),
    os.path.join(BASE_PATH, 'sorter.malloc-type.json'),
    os.path.join(BASE_PATH, 'sorter.vm-map.json'),
    os.path.join(BASE_PATH, 'sorter.vm-sharing.json'),
    ]

DEFAULT_TEMPLATES = os.path.join(BASE_PATH, 'templates.json')


class Unit(object):
  """Represents a minimum unit of memory usage categorization.

  It is supposed to be inherited for some different spaces like the entire
  virtual memory and malloc arena. Such different spaces are called "worlds"
  in dmprof. (For example, the "vm" world and the "malloc" world.)
  """
  def __init__(self, unit_id, size):
    self._unit_id = unit_id
    self._size = size

  @property
  def unit_id(self):
    return self._unit_id

  @property
  def size(self):
    return self._size


class VMUnit(Unit):
  """Represents a Unit for a memory region on virtual memory."""
  def __init__(self, unit_id, committed, reserved, mmap, region,
               pageframe=None, group_pfn_counts=None):
    super(VMUnit, self).__init__(unit_id, committed)
    self._reserved = reserved
    self._mmap = mmap
    self._region = region
    self._pageframe = pageframe
    self._group_pfn_counts = group_pfn_counts

  @property
  def committed(self):
    return self._size

  @property
  def reserved(self):
    return self._reserved

  @property
  def mmap(self):
    return self._mmap

  @property
  def region(self):
    return self._region

  @property
  def pageframe(self):
    return self._pageframe

  @property
  def group_pfn_counts(self):
    return self._group_pfn_counts


class MMapUnit(VMUnit):
  """Represents a Unit for a mmap'ed region."""
  def __init__(self, unit_id, committed, reserved, region, bucket_set,
               pageframe=None, group_pfn_counts=None):
    super(MMapUnit, self).__init__(unit_id, committed, reserved, True,
                                   region, pageframe, group_pfn_counts)
    self._bucket_set = bucket_set

  def __repr__(self):
    return str(self.region)

  @property
  def bucket_set(self):
    return self._bucket_set


class UnhookedUnit(VMUnit):
  """Represents a Unit for a non-mmap'ed memory region on virtual memory."""
  def __init__(self, unit_id, committed, reserved, region,
               pageframe=None, group_pfn_counts=None):
    super(UnhookedUnit, self).__init__(unit_id, committed, reserved, False,
                                       region, pageframe, group_pfn_counts)

  def __repr__(self):
    return str(self.region)


class MallocUnit(Unit):
  """Represents a Unit for a malloc'ed memory block."""
  def __init__(self, unit_id, size, alloc_count, free_count, bucket):
    super(MallocUnit, self).__init__(unit_id, size)
    self._bucket = bucket
    self._alloc_count = alloc_count
    self._free_count = free_count

  def __repr__(self):
    return str(self.bucket)

  @property
  def bucket(self):
    return self._bucket

  @property
  def alloc_count(self):
    return self._alloc_count

  @property
  def free_count(self):
    return self._free_count


class UnitSet(object):
  """Represents an iterable set of Units."""
  def __init__(self, world):
    self._units = {}
    self._world = world

  def __repr__(self):
    return str(self._units)

  def __iter__(self):
    for unit_id in sorted(self._units):
      yield self._units[unit_id]

  def append(self, unit, overwrite=False):
    if not overwrite and unit.unit_id in self._units:
      LOGGER.error('The unit id=%s already exists.' % str(unit.unit_id))
    self._units[unit.unit_id] = unit


class AbstractRule(object):
  """An abstract class for rules to be matched with units."""
  def __init__(self, dct):
    self._name = dct['name']
    self._hidden = dct.get('hidden', False)
    self._subs = dct.get('subs', [])

  def match(self, unit):
    raise NotImplementedError()

  @property
  def name(self):
    return self._name

  @property
  def hidden(self):
    return self._hidden

  def iter_subs(self):
    for sub in self._subs:
      yield sub


class VMRule(AbstractRule):
  """Represents a Rule to match with virtual memory regions."""
  def __init__(self, dct):
    super(VMRule, self).__init__(dct)
    self._backtrace_function = dct.get('backtrace_function', None)
    if self._backtrace_function:
      self._backtrace_function = re.compile(self._backtrace_function)
    self._backtrace_sourcefile = dct.get('backtrace_sourcefile', None)
    if self._backtrace_sourcefile:
      self._backtrace_sourcefile = re.compile(self._backtrace_sourcefile)
    self._mmap = dct.get('mmap', None)
    self._sharedwith = dct.get('sharedwith', [])
    self._mapped_pathname = dct.get('mapped_pathname', None)
    if self._mapped_pathname:
      self._mapped_pathname = re.compile(self._mapped_pathname)
    self._mapped_permission = dct.get('mapped_permission', None)
    if self._mapped_permission:
      self._mapped_permission = re.compile(self._mapped_permission)

  def __repr__(self):
    result = cStringIO.StringIO()
    result.write('{"%s"=>' % self._name)
    attributes = []
    attributes.append('mmap: %s' % self._mmap)
    if self._backtrace_function:
      attributes.append('backtrace_function: "%s"' %
                        self._backtrace_function.pattern)
    if self._sharedwith:
      attributes.append('sharedwith: "%s"' % self._sharedwith)
    if self._mapped_pathname:
      attributes.append('mapped_pathname: "%s"' % self._mapped_pathname.pattern)
    if self._mapped_permission:
      attributes.append('mapped_permission: "%s"' %
                        self._mapped_permission.pattern)
    result.write('%s}' % ', '.join(attributes))
    return result.getvalue()

  def match(self, unit):
    if unit.mmap:
      assert unit.region[0] == 'hooked'
      bucket = unit.bucket_set.get(unit.region[1]['bucket_id'])
      assert bucket
      assert bucket.allocator_type == 'mmap'

      stackfunction = bucket.symbolized_joined_stackfunction
      stacksourcefile = bucket.symbolized_joined_stacksourcefile

      # TODO(dmikurube): Support shared memory.
      sharedwith = None

      if self._mmap == False: # (self._mmap == None) should go through.
        return False
      if (self._backtrace_function and
          not self._backtrace_function.match(stackfunction)):
        return False
      if (self._backtrace_sourcefile and
          not self._backtrace_sourcefile.match(stacksourcefile)):
        return False
      if (self._mapped_pathname and
          not self._mapped_pathname.match(unit.region[1]['vma']['name'])):
        return False
      if (self._mapped_permission and
          not self._mapped_permission.match(
              unit.region[1]['vma']['readable'] +
              unit.region[1]['vma']['writable'] +
              unit.region[1]['vma']['executable'] +
              unit.region[1]['vma']['private'])):
        return False
      if (self._sharedwith and
          unit.pageframe and sharedwith not in self._sharedwith):
        return False

      return True

    else:
      assert unit.region[0] == 'unhooked'

      # TODO(dmikurube): Support shared memory.
      sharedwith = None

      if self._mmap == True: # (self._mmap == None) should go through.
        return False
      if (self._mapped_pathname and
          not self._mapped_pathname.match(unit.region[1]['vma']['name'])):
        return False
      if (self._mapped_permission and
          not self._mapped_permission.match(
              unit.region[1]['vma']['readable'] +
              unit.region[1]['vma']['writable'] +
              unit.region[1]['vma']['executable'] +
              unit.region[1]['vma']['private'])):
        return False
      if (self._sharedwith and
          unit.pageframe and sharedwith not in self._sharedwith):
        return False

      return True


class MallocRule(AbstractRule):
  """Represents a Rule to match with malloc'ed blocks."""
  def __init__(self, dct):
    super(MallocRule, self).__init__(dct)
    self._backtrace_function = dct.get('backtrace_function', None)
    if self._backtrace_function:
      self._backtrace_function = re.compile(self._backtrace_function)
    self._backtrace_sourcefile = dct.get('backtrace_sourcefile', None)
    if self._backtrace_sourcefile:
      self._backtrace_sourcefile = re.compile(self._backtrace_sourcefile)
    self._typeinfo = dct.get('typeinfo', None)
    if self._typeinfo:
      self._typeinfo = re.compile(self._typeinfo)

  def __repr__(self):
    result = cStringIO.StringIO()
    result.write('{"%s"=>' % self._name)
    attributes = []
    if self._backtrace_function:
      attributes.append('backtrace_function: "%s"' % self._backtrace_function)
    if self._typeinfo:
      attributes.append('typeinfo: "%s"' % self._typeinfo)
    result.write('%s}' % ', '.join(attributes))
    return result.getvalue()

  def match(self, unit):
    assert unit.bucket.allocator_type == 'malloc'

    stackfunction = unit.bucket.symbolized_joined_stackfunction
    stacksourcefile = unit.bucket.symbolized_joined_stacksourcefile
    typeinfo = unit.bucket.symbolized_typeinfo
    if typeinfo.startswith('0x'):
      typeinfo = unit.bucket.typeinfo_name

    return ((not self._backtrace_function or
             self._backtrace_function.match(stackfunction)) and
            (not self._backtrace_sourcefile or
             self._backtrace_sourcefile.match(stacksourcefile)) and
            (not self._typeinfo or self._typeinfo.match(typeinfo)))


class NoBucketMallocRule(MallocRule):
  """Represents a Rule that small ignorable units match with."""
  def __init__(self):
    super(NoBucketMallocRule, self).__init__({'name': 'tc-no-bucket'})
    self._no_bucket = True

  @property
  def no_bucket(self):
    return self._no_bucket


class AbstractSorter(object):
  """An abstract class for classifying Units with a set of Rules."""
  def __init__(self, dct):
    self._type = 'sorter'
    self._version = dct['version']
    self._world = dct['world']
    self._name = dct['name']
    self._root = dct.get('root', False)
    self._order = dct['order']

    self._rules = []
    for rule in dct['rules']:
      if dct['world'] == 'vm':
        self._rules.append(VMRule(rule))
      elif dct['world'] == 'malloc':
        self._rules.append(MallocRule(rule))
      else:
        LOGGER.error('Unknown sorter world type')

  def __repr__(self):
    result = cStringIO.StringIO()
    result.write('world=%s' % self._world)
    result.write('order=%s' % self._order)
    result.write('rules:')
    for rule in self._rules:
      result.write('  %s' % rule)
    return result.getvalue()

  @staticmethod
  def load(filename):
    with open(filename) as sorter_f:
      sorter_dict = json.load(sorter_f)
    if sorter_dict['world'] == 'vm':
      return VMSorter(sorter_dict)
    elif sorter_dict['world'] == 'malloc':
      return MallocSorter(sorter_dict)
    else:
      LOGGER.error('Unknown sorter world type')
      return None

  @property
  def world(self):
    return self._world

  @property
  def name(self):
    return self._name

  @property
  def root(self):
    return self._root

  def find(self, unit):
    raise NotImplementedError()

  def find_rule(self, name):
    """Finds a rule whose name is |name|. """
    for rule in self._rules:
      if rule.name == name:
        return rule
    return None


class VMSorter(AbstractSorter):
  """Represents a Sorter for memory regions on virtual memory."""
  def __init__(self, dct):
    assert dct['world'] == 'vm'
    super(VMSorter, self).__init__(dct)

  def find(self, unit):
    for rule in self._rules:
      if rule.match(unit):
        return rule
    assert False


class MallocSorter(AbstractSorter):
  """Represents a Sorter for malloc'ed blocks."""
  def __init__(self, dct):
    assert dct['world'] == 'malloc'
    super(MallocSorter, self).__init__(dct)
    self._no_bucket_rule = NoBucketMallocRule()

  def find(self, unit):
    if not unit.bucket:
      return self._no_bucket_rule
    assert unit.bucket.allocator_type == 'malloc'

    if unit.bucket.component_cache:
      return unit.bucket.component_cache

    for rule in self._rules:
      if rule.match(unit):
        unit.bucket.component_cache = rule
        return rule
    assert False


class SorterTemplates(object):
  """Represents a template for sorters."""
  def __init__(self, dct):
    self._dict = dct

  def as_dict(self):
    return self._dict

  @staticmethod
  def load(filename):
    with open(filename) as templates_f:
      templates_dict = json.load(templates_f)
    return SorterTemplates(templates_dict)


class SorterSet(object):
  """Represents an iterable set of Sorters."""
  def __init__(self, additional=None, default=None):
    if not additional:
      additional = []
    if not default:
      default = DEFAULT_SORTERS
    self._sorters = {}
    for filename in default + additional:
      sorter = AbstractSorter.load(filename)
      if sorter.world not in self._sorters:
        self._sorters[sorter.world] = []
      self._sorters[sorter.world].append(sorter)
    self._templates = SorterTemplates.load(DEFAULT_TEMPLATES)

  def __repr__(self):
    result = cStringIO.StringIO()
    result.write(self._sorters)
    return result.getvalue()

  def __iter__(self):
    for sorters in self._sorters.itervalues():
      for sorter in sorters:
        yield sorter

  def iter_world(self, world):
    for sorter in self._sorters.get(world, []):
      yield sorter

  @property
  def templates(self):
    return self._templates
