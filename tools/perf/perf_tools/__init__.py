# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""A library for Chrome GPU test code."""
import os
import sys


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


def Init():
  telemetry_path = os.path.join(os.path.dirname(__file__),
                                os.pardir, os.pardir, 'telemetry')
  absolute_telemetry_path = os.path.abspath(telemetry_path)
  sys.path.append(absolute_telemetry_path)
  telemetry_tools_path = os.path.join(os.path.dirname(__file__),
                                      os.pardir, os.pardir, 'telemetry_tools')
  absolute_telemetry_tools_path = os.path.abspath(telemetry_tools_path)
  sys.path.append(absolute_telemetry_tools_path)


_RemoveAllStalePycFiles()
Init()
