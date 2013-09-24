# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import fnmatch

OS_MODIFIERS = ['win', 'xp', 'vista', 'win7',
                'mac', 'leopard', 'snowleopard', 'lion', 'mountainlion',
                'linux', 'chromeos', 'android']
GPU_MODIFIERS = ['nvidia', 'amd', 'intel']
CONFIG_MODIFIERS = ['debug', 'release']

class Expectation(object):
  def __init__(self, expectation, url_pattern, conditions=None, bug=None):
    self.expectation = expectation.lower()
    self.url_pattern = url_pattern
    self.bug = bug

    self.os_conditions = []
    self.gpu_conditions = []
    self.config_conditions = []

    # Make sure that non-absolute paths are searchable
    if not '://' in self.url_pattern:
      self.url_pattern = '*/' + self.url_pattern

    if conditions:
      for c in conditions:
        condition = c.lower()
        if condition in OS_MODIFIERS:
          self.os_conditions.append(condition)
        elif condition in GPU_MODIFIERS:
          self.gpu_conditions.append(condition)
        elif condition in CONFIG_MODIFIERS:
          self.config_conditions.append(condition)
        else:
          raise ValueError('Unknown expectation condition: "%s"' % condition)

class TestExpectations(object):
  """A class which defines the expectations for a page set test execution"""

  def __init__(self):
    self.expectations = []
    self.SetExpectations()

  def SetExpectations(self):
    """Called on creation. Override to set up custom expectations."""
    pass

  def Fail(self, url_pattern, conditions=None, bug=None):
    self._Expect('fail', url_pattern, conditions, bug)

  def _Expect(self, expectation, url_pattern, conditions=None, bug=None):
    self.expectations.append(Expectation(expectation, url_pattern, conditions,
      bug))

  def GetExpectationForPage(self, platform, page):
    for e in self.expectations:
      if fnmatch.fnmatch(page.url, e.url_pattern):
        if self._ModifiersApply(platform, e):
          return e.expectation
    return 'pass'

  def _ModifiersApply(self, platform, expectation):
    """Determines if the conditions for an expectation apply to this system."""
    os_matches = (not expectation.os_conditions or
          platform.GetOSName() in expectation.os_conditions or
          platform.GetOSVersionName() in expectation.os_conditions)

    # TODO: Add checks against other modifiers (GPU, configuration, etc.)

    return os_matches