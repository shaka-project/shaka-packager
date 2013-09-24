# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import logging
import os
import re


LOGGER = logging.getLogger('dmprof')

BASE_PATH = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
POLICIES_JSON_PATH = os.path.join(BASE_PATH, 'policies.json')

# Heap Profile Policy versions

# POLICY_DEEP_1 DOES NOT include allocation_type columns.
# mmap regions are distincted w/ mmap frames in the pattern column.
POLICY_DEEP_1 = 'POLICY_DEEP_1'

# POLICY_DEEP_2 DOES include allocation_type columns.
# mmap regions are distincted w/ the allocation_type column.
POLICY_DEEP_2 = 'POLICY_DEEP_2'

# POLICY_DEEP_3 is in JSON format.
POLICY_DEEP_3 = 'POLICY_DEEP_3'

# POLICY_DEEP_3 contains typeinfo.
POLICY_DEEP_4 = 'POLICY_DEEP_4'


class Rule(object):
  """Represents one matching rule in a policy file."""

  def __init__(self,
               name,
               allocator_type,
               stackfunction_pattern=None,
               stacksourcefile_pattern=None,
               typeinfo_pattern=None,
               mappedpathname_pattern=None,
               mappedpermission_pattern=None,
               sharedwith=None):
    self._name = name
    self._allocator_type = allocator_type

    self._stackfunction_pattern = None
    if stackfunction_pattern:
      self._stackfunction_pattern = re.compile(
          stackfunction_pattern + r'\Z')

    self._stacksourcefile_pattern = None
    if stacksourcefile_pattern:
      self._stacksourcefile_pattern = re.compile(
          stacksourcefile_pattern + r'\Z')

    self._typeinfo_pattern = None
    if typeinfo_pattern:
      self._typeinfo_pattern = re.compile(typeinfo_pattern + r'\Z')

    self._mappedpathname_pattern = None
    if mappedpathname_pattern:
      self._mappedpathname_pattern = re.compile(mappedpathname_pattern + r'\Z')

    self._mappedpermission_pattern = None
    if mappedpermission_pattern:
      self._mappedpermission_pattern = re.compile(
          mappedpermission_pattern + r'\Z')

    self._sharedwith = []
    if sharedwith:
      self._sharedwith = sharedwith

  @property
  def name(self):
    return self._name

  @property
  def allocator_type(self):
    return self._allocator_type

  @property
  def stackfunction_pattern(self):
    return self._stackfunction_pattern

  @property
  def stacksourcefile_pattern(self):
    return self._stacksourcefile_pattern

  @property
  def typeinfo_pattern(self):
    return self._typeinfo_pattern

  @property
  def mappedpathname_pattern(self):
    return self._mappedpathname_pattern

  @property
  def mappedpermission_pattern(self):
    return self._mappedpermission_pattern

  @property
  def sharedwith(self):
    return self._sharedwith


class Policy(object):
  """Represents a policy, a content of a policy file."""

  def __init__(self, rules, version, components):
    self._rules = rules
    self._version = version
    self._components = components

  @property
  def rules(self):
    return self._rules

  @property
  def version(self):
    return self._version

  @property
  def components(self):
    return self._components

  def find_rule(self, component_name):
    """Finds a rule whose name is |component_name|. """
    for rule in self._rules:
      if rule.name == component_name:
        return rule
    return None

  def find_malloc(self, bucket):
    """Finds a matching component name which a given |bucket| belongs to.

    Args:
        bucket: A Bucket object to be searched for.

    Returns:
        A string representing a component name.
    """
    assert not bucket or bucket.allocator_type == 'malloc'

    if not bucket:
      return 'no-bucket'
    if bucket.component_cache:
      return bucket.component_cache

    stackfunction = bucket.symbolized_joined_stackfunction
    stacksourcefile = bucket.symbolized_joined_stacksourcefile
    typeinfo = bucket.symbolized_typeinfo
    if typeinfo.startswith('0x'):
      typeinfo = bucket.typeinfo_name

    for rule in self._rules:
      if (rule.allocator_type == 'malloc' and
          (not rule.stackfunction_pattern or
           rule.stackfunction_pattern.match(stackfunction)) and
          (not rule.stacksourcefile_pattern or
           rule.stacksourcefile_pattern.match(stacksourcefile)) and
          (not rule.typeinfo_pattern or rule.typeinfo_pattern.match(typeinfo))):
        bucket.component_cache = rule.name
        return rule.name

    assert False

  def find_mmap(self, region, bucket_set,
                pageframe=None, group_pfn_counts=None):
    """Finds a matching component which a given mmap |region| belongs to.

    It uses |bucket_set| to match with backtraces.  If |pageframe| is given,
    it considers memory sharing among processes.

    NOTE: Don't use Bucket's |component_cache| for mmap regions because they're
    classified not only with bucket information (mappedpathname for example).

    Args:
        region: A tuple representing a memory region.
        bucket_set: A BucketSet object to look up backtraces.
        pageframe: A PageFrame object representing a pageframe maybe including
            a pagecount.
        group_pfn_counts: A dict mapping a PFN to the number of times the
            the pageframe is mapped by the known "group (Chrome)" processes.

    Returns:
        A string representing a component name.
    """
    assert region[0] == 'hooked'
    bucket = bucket_set.get(region[1]['bucket_id'])
    assert not bucket or bucket.allocator_type == 'mmap'

    if not bucket:
      return 'no-bucket', None

    stackfunction = bucket.symbolized_joined_stackfunction
    stacksourcefile = bucket.symbolized_joined_stacksourcefile
    sharedwith = self._categorize_pageframe(pageframe, group_pfn_counts)

    for rule in self._rules:
      if (rule.allocator_type == 'mmap' and
          (not rule.stackfunction_pattern or
           rule.stackfunction_pattern.match(stackfunction)) and
          (not rule.stacksourcefile_pattern or
           rule.stacksourcefile_pattern.match(stacksourcefile)) and
          (not rule.mappedpathname_pattern or
           rule.mappedpathname_pattern.match(region[1]['vma']['name'])) and
          (not rule.mappedpermission_pattern or
           rule.mappedpermission_pattern.match(
               region[1]['vma']['readable'] +
               region[1]['vma']['writable'] +
               region[1]['vma']['executable'] +
               region[1]['vma']['private'])) and
          (not rule.sharedwith or
           not pageframe or sharedwith in rule.sharedwith)):
        return rule.name, bucket

    assert False

  def find_unhooked(self, region, pageframe=None, group_pfn_counts=None):
    """Finds a matching component which a given unhooked |region| belongs to.

    If |pageframe| is given, it considers memory sharing among processes.

    Args:
        region: A tuple representing a memory region.
        pageframe: A PageFrame object representing a pageframe maybe including
            a pagecount.
        group_pfn_counts: A dict mapping a PFN to the number of times the
            the pageframe is mapped by the known "group (Chrome)" processes.

    Returns:
        A string representing a component name.
    """
    assert region[0] == 'unhooked'
    sharedwith = self._categorize_pageframe(pageframe, group_pfn_counts)

    for rule in self._rules:
      if (rule.allocator_type == 'unhooked' and
          (not rule.mappedpathname_pattern or
           rule.mappedpathname_pattern.match(region[1]['vma']['name'])) and
          (not rule.mappedpermission_pattern or
           rule.mappedpermission_pattern.match(
               region[1]['vma']['readable'] +
               region[1]['vma']['writable'] +
               region[1]['vma']['executable'] +
               region[1]['vma']['private'])) and
          (not rule.sharedwith or
           not pageframe or sharedwith in rule.sharedwith)):
        return rule.name

    assert False

  @staticmethod
  def load(filename, filetype):
    """Loads a policy file of |filename| in a |format|.

    Args:
        filename: A filename to be loaded.
        filetype: A string to specify a type of the file.  Only 'json' is
            supported for now.

    Returns:
        A loaded Policy object.
    """
    with open(os.path.join(BASE_PATH, filename)) as policy_f:
      return Policy.parse(policy_f, filetype)

  @staticmethod
  def parse(policy_f, filetype):
    """Parses a policy file content in a |format|.

    Args:
        policy_f: An IO object to be loaded.
        filetype: A string to specify a type of the file.  Only 'json' is
            supported for now.

    Returns:
        A loaded Policy object.
    """
    if filetype == 'json':
      return Policy._parse_json(policy_f)
    else:
      return None

  @staticmethod
  def _parse_json(policy_f):
    """Parses policy file in json format.

    A policy file contains component's names and their stacktrace pattern
    written in regular expression.  Those patterns are matched against each
    symbols of each stacktraces in the order written in the policy file

    Args:
         policy_f: A File/IO object to read.

    Returns:
         A loaded policy object.
    """
    policy = json.load(policy_f)

    rules = []
    for rule in policy['rules']:
      stackfunction = rule.get('stackfunction') or rule.get('stacktrace')
      stacksourcefile = rule.get('stacksourcefile')
      rules.append(Rule(
          rule['name'],
          rule['allocator'],  # allocator_type
          stackfunction,
          stacksourcefile,
          rule['typeinfo'] if 'typeinfo' in rule else None,
          rule.get('mappedpathname'),
          rule.get('mappedpermission'),
          rule.get('sharedwith')))

    return Policy(rules, policy['version'], policy['components'])

  @staticmethod
  def _categorize_pageframe(pageframe, group_pfn_counts):
    """Categorizes a pageframe based on its sharing status.

    Returns:
        'private' if |pageframe| is not shared with other processes.  'group'
        if |pageframe| is shared only with group (Chrome-related) processes.
        'others' if |pageframe| is shared with non-group processes.
    """
    if not pageframe:
      return 'private'

    if pageframe.pagecount:
      if pageframe.pagecount == 1:
        return 'private'
      elif pageframe.pagecount <= group_pfn_counts.get(pageframe.pfn, 0) + 1:
        return 'group'
      else:
        return 'others'
    else:
      if pageframe.pfn in group_pfn_counts:
        return 'group'
      else:
        return 'private'


class PolicySet(object):
  """Represents a set of policies."""

  def __init__(self, policy_directory):
    self._policy_directory = policy_directory

  @staticmethod
  def load(labels=None):
    """Loads a set of policies via the "default policy directory".

    The "default policy directory" contains pairs of policies and their labels.
    For example, a policy "policy.l0.json" is labeled "l0" in the default
    policy directory "policies.json".

    All policies in the directory are loaded by default.  Policies can be
    limited by |labels|.

    Args:
        labels: An array that contains policy labels to be loaded.

    Returns:
        A PolicySet object.
    """
    default_policy_directory = PolicySet._load_default_policy_directory()
    if labels:
      specified_policy_directory = {}
      for label in labels:
        if label in default_policy_directory:
          specified_policy_directory[label] = default_policy_directory[label]
        # TODO(dmikurube): Load an un-labeled policy file.
      return PolicySet._load_policies(specified_policy_directory)
    else:
      return PolicySet._load_policies(default_policy_directory)

  def __len__(self):
    return len(self._policy_directory)

  def __iter__(self):
    for label in self._policy_directory:
      yield label

  def __getitem__(self, label):
    return self._policy_directory[label]

  @staticmethod
  def _load_default_policy_directory():
    with open(POLICIES_JSON_PATH, mode='r') as policies_f:
      default_policy_directory = json.load(policies_f)
    return default_policy_directory

  @staticmethod
  def _load_policies(directory):
    LOGGER.info('Loading policy files.')
    policies = {}
    for label in directory:
      LOGGER.info('  %s: %s' % (label, directory[label]['file']))
      loaded = Policy.load(directory[label]['file'], directory[label]['format'])
      if loaded:
        policies[label] = loaded
    return PolicySet(policies)
