# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

from telemetry.core import discover
from telemetry.core import profile_creator

BASE_PROFILE_TYPES = ['clean', 'default']

PROFILE_CREATORS = {}

PROFILE_TYPE_MAPPING = {
  'typical_user': 'chrome/test/data/extensions/profiles/content_scripts1',
  'power_user': 'chrome/test/data/extensions/profiles/extension_webrequest',
}

def _DiscoverCreateableProfiles(profile_creators_dir, base_dir):
  """Returns a dictionary of all the profile creators we can use to create
  a Chrome profile for testing located in |profile_creators_dir|.
  The returned value consists of 'class_name' -> 'test class' dictionary where
  class_name is the name of the class with the _creator suffix removed e.g.
  'small_profile_creator will be 'small_profile'.
  """
  profile_creators_unfiltered = discover.DiscoverClasses(
      profile_creators_dir, base_dir, profile_creator.ProfileCreator)

  # Remove '_creator' suffix from keys.
  profile_creators = {}
  for test_name, test_class in profile_creators_unfiltered.iteritems():
    assert test_name.endswith('_creator')
    test_name = test_name[:-len('_creator')]
    profile_creators[test_name] = test_class

  return profile_creators

def ClearProfieCreatorsForTests():
  """Clears the discovered profile creator objects.  Used for unit tests."""
  PROFILE_CREATORS.clear()

def FindProfileCreators(profile_creators_dir, base_dir):
  """Discover all the ProfileCreator objects in |profile_creators_dir|."""
  assert not PROFILE_CREATORS  # It's illegal to call this function twice.
  PROFILE_CREATORS.update(_DiscoverCreateableProfiles(
      profile_creators_dir, base_dir))

def GetProfileTypes():
  """Returns a list of all command line options that can be specified for
  profile type."""
  return (BASE_PROFILE_TYPES + PROFILE_TYPE_MAPPING.keys() +
      PROFILE_CREATORS.keys())

def GetProfileDir(profile_type):
  """Given a |profile_type| (as returned by GetProfileTypes()), return the
  directory to use for that profile or None if the profile needs to be generated
  or doesn't need a profile directory (e.g. using the browser default profile).
  """
  if (profile_type in BASE_PROFILE_TYPES or
      profile_type in PROFILE_CREATORS):
    return None

  path = os.path.abspath(os.path.join(os.path.dirname(__file__),
      '..', '..', '..', '..', *PROFILE_TYPE_MAPPING[profile_type].split('/')))
  assert os.path.exists(path)
  return path

def GetProfileCreator(profile_type):
  """Returns the profile creator object corresponding to the |profile_type|
  string."""
  return PROFILE_CREATORS.get(profile_type)
