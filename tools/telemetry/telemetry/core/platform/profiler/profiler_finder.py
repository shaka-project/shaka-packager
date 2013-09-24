# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

from telemetry.core import discover
from telemetry.core import util
from telemetry.core.platform import profiler


def _DiscoverProfilers():
  profiler_dir = os.path.dirname(__file__)
  return discover.DiscoverClasses(profiler_dir, util.GetTelemetryDir(),
                                  profiler.Profiler).values()


def FindProfiler(name):
  for p in _DiscoverProfilers():
    if p.name() == name:
      return p
  return None


def GetAllAvailableProfilers(options):
  return sorted([p.name() for p in _DiscoverProfilers()
                 if p.is_supported(options)])
