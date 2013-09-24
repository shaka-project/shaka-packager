# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Class representing uiautomator test package."""

import os

from pylib import constants
from pylib.instrumentation import test_jar


class TestPackage(test_jar.TestJar):
  def __init__(self, jar_path, jar_info_path):
    test_jar.TestJar.__init__(self, jar_info_path)

    if not os.path.exists(jar_path):
      raise Exception('%s not found, please build it' % jar_path)
    self._jar_path = jar_path

  def GetPackageName(self):
    """Returns the JAR named that is installed on the device."""
    return os.path.basename(self._jar_path)

  # Override.
  def Install(self, adb):
    adb.PushIfNeeded(self._jar_path, constants.TEST_EXECUTABLE_DIR)
