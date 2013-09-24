# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Parses options for the instrumentation tests."""

import os


# TODO(gkanwar): Some downstream scripts current rely on these functions
# existing. This dependency should be removed, and this file deleted, in the
# future.
def AddBuildTypeOption(option_parser):
  """Decorates OptionParser with build type option."""
  default_build_type = 'Debug'
  if 'BUILDTYPE' in os.environ:
    default_build_type = os.environ['BUILDTYPE']
  option_parser.add_option('--debug', action='store_const', const='Debug',
                           dest='build_type', default=default_build_type,
                           help='If set, run test suites under out/Debug. '
                           'Default is env var BUILDTYPE or Debug')
  option_parser.add_option('--release', action='store_const', const='Release',
                           dest='build_type',
                           help='If set, run test suites under out/Release. '
                           'Default is env var BUILDTYPE or Debug.')


def AddTestRunnerOptions(option_parser, default_timeout=60):
  """Decorates OptionParser with options applicable to all tests."""

  option_parser.add_option('-t', dest='timeout',
                           help='Timeout to wait for each test',
                           type='int',
                           default=default_timeout)
  option_parser.add_option('-c', dest='cleanup_test_files',
                           help='Cleanup test files on the device after run',
                           action='store_true')
  option_parser.add_option('--num_retries', dest='num_retries', type='int',
                           default=2,
                           help='Number of retries for a test before '
                           'giving up.')
  option_parser.add_option('-v',
                           '--verbose',
                           dest='verbose_count',
                           default=0,
                           action='count',
                           help='Verbose level (multiple times for more)')
  profilers = ['devicestatsmonitor', 'chrometrace', 'dumpheap', 'smaps',
               'traceview']
  option_parser.add_option('--profiler', dest='profilers', action='append',
                           choices=profilers,
                           help='Profiling tool to run during test. '
                           'Pass multiple times to run multiple profilers. '
                           'Available profilers: %s' % profilers)
  option_parser.add_option('--tool',
                           dest='tool',
                           help='Run the test under a tool '
                           '(use --tool help to list them)')
  option_parser.add_option('--flakiness-dashboard-server',
                           dest='flakiness_dashboard_server',
                           help=('Address of the server that is hosting the '
                                 'Chrome for Android flakiness dashboard.'))
  option_parser.add_option('--skip-deps-push', dest='push_deps',
                           action='store_false', default=True,
                           help='Do not push dependencies to the device. '
                           'Use this at own risk for speeding up test '
                           'execution on local machine.')
  AddBuildTypeOption(option_parser)
