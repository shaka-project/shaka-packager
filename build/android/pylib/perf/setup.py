# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Generates test runner factory and tests for performance tests."""

import json
import logging
import os
import psutil
import signal
import time

from pylib import android_commands
from pylib import cmd_helper
from pylib import forwarder
from pylib import ports

import test_runner


def _KillPendingServers():
  for retry in range(5):
    for server in ['lighttpd', 'web-page-replay']:
      pids = [p.pid for p in psutil.process_iter() if server in p.name]
      for pid in pids:
        try:
          logging.warning('Killing %s %s', server, pid)
          os.kill(pid, signal.SIGQUIT)
        except Exception as e:
          logging.warning('Failed killing %s %s %s', server, pid, e)
  # Restart the adb server with taskset to set a single CPU affinity.
  cmd_helper.RunCmd(['adb', 'kill-server'])
  cmd_helper.RunCmd(['taskset', '-c', '0', 'adb', 'start-server'])
  cmd_helper.RunCmd(['taskset', '-c', '0', 'adb', 'root'])
  i = 1
  while not android_commands.GetAttachedDevices():
    time.sleep(i)
    i *= 2
    if i > 10:
      break
  # Reset the test port allocation. It's important to do it before starting
  # to dispatch any step.
  if not ports.ResetTestServerPortAllocation():
    raise Exception('Failed to reset test server port.')

  forwarder.Forwarder.UseMultiprocessing()


def Setup(test_options):
  """Create and return the test runner factory and tests.

  Args:
    test_options: A PerformanceOptions object.

  Returns:
    A tuple of (TestRunnerFactory, tests).
  """
  # Before running the tests, kill any leftover server.
  _KillPendingServers()

  with file(test_options.steps, 'r') as f:
    tests = json.load(f)

  flaky_steps = []
  if test_options.flaky_steps:
    with file(test_options.flaky_steps, 'r') as f:
      flaky_steps = json.load(f)

  def TestRunnerFactory(device, shard_index):
    return test_runner.TestRunner(
        test_options, device, tests, flaky_steps)

  return (TestRunnerFactory, sorted(tests.keys()))
