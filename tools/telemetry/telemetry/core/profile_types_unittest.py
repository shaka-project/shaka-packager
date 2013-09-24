# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import os
import unittest

from telemetry.core import profile_types
from telemetry.core import util

class ProfileTypesTest(unittest.TestCase):
  def testGetProfileTypes(self):
    types = profile_types.GetProfileTypes()

    self.assertTrue('clean' in types)
    self.assertTrue(len(types) > 0)

  def testGetProfileDir(self):
    self.assertFalse(profile_types.GetProfileDir('typical_user') is None)

  def testGetProfileCreatorTypes(self):
    profile_creators_dir = os.path.join(
        util.GetUnittestDataDir(), 'discoverable_classes')
    base_dir = util.GetUnittestDataDir()

    profile_types.FindProfileCreators(profile_creators_dir, base_dir)
    types = profile_types.GetProfileTypes()
    self.assertTrue(len(types) > 0)
    self.assertTrue('dummy_profile' in types)

    dummy_profile_creator = profile_types.GetProfileCreator('dummy_profile')
    self.assertTrue(dummy_profile_creator.__name__ == 'DummyProfileCreator')
    profile_types.ClearProfieCreatorsForTests()
