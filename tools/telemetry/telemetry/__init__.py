# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
A library for cross-platform browser tests.
"""
import inspect
import os
import sys

from telemetry.core.browser import Browser
from telemetry.core.browser_options import BrowserOptions
from telemetry.core.tab import Tab

from telemetry.page.page_measurement import PageMeasurement
from telemetry.page.page_runner import Run as RunPage

__all__ = []

# Find all local vars that are classes or functions and make sure they're in the
# __all__ array so they're included in docs.
for x in dir():
  if x.startswith('_'):
    continue
  if x in (inspect, sys):
    continue
  m = sys.modules[__name__]
  if (inspect.isclass(getattr(m, x)) or
      inspect.isfunction(getattr(m, x))):
    __all__.append(x)


def _RemoveAllStalePycFiles():
  for dirname, _, filenames in os.walk(os.path.dirname(__file__)):
    if '.svn' in dirname or '.git' in dirname:
      continue
    for filename in filenames:
      root, ext = os.path.splitext(filename)
      if ext != '.pyc':
        continue

      pyc_path = os.path.join(dirname, filename)
      py_path = os.path.join(dirname, root + '.py')
      if not os.path.exists(py_path):
        os.remove(pyc_path)

    if not os.listdir(dirname):
      os.removedirs(dirname)


_RemoveAllStalePycFiles()
