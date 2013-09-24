#!/usr/bin/env python
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys

import bb_utils
import bb_annotations

sys.path.append(os.path.join(os.path.dirname(__file__), '..'))
from pylib import constants


SLAVE_SCRIPTS_DIR = os.path.join(bb_utils.BB_BUILD_DIR, 'scripts', 'slave')
VALID_HOST_TESTS = set(['check_webview_licenses', 'findbugs'])
EXPERIMENTAL_TARGETS = ['android_experimental']

# Short hand for RunCmd which is used extensively in this file.
RunCmd = bb_utils.RunCmd


def SrcPath(*path):
  return os.path.join(constants.DIR_SOURCE_ROOT, *path)


def CheckWebViewLicenses(_):
  bb_annotations.PrintNamedStep('check_licenses')
  RunCmd([SrcPath('android_webview', 'tools', 'webview_licenses.py'), 'scan'],
         warning_code=1)


def RunHooks(build_type):
  RunCmd([SrcPath('build', 'landmines.py')])
  build_path = SrcPath('out', build_type)
  landmine_path = os.path.join(build_path, '.landmines_triggered')
  clobber_env = os.environ.get('BUILDBOT_CLOBBER')
  if clobber_env or os.path.isfile(landmine_path):
    bb_annotations.PrintNamedStep('Clobber')
    if not clobber_env:
      print 'Clobbering due to triggered landmines:'
      with open(landmine_path) as f:
        print f.read()
    RunCmd(['rm', '-rf', build_path])

  bb_annotations.PrintNamedStep('runhooks')
  RunCmd(['gclient', 'runhooks'], halt_on_failure=True)


def Compile(options):
  RunHooks(options.target)
  cmd = [os.path.join(SLAVE_SCRIPTS_DIR, 'compile.py'),
         '--build-tool=ninja',
         '--compiler=goma',
         '--target=%s' % options.target,
         '--goma-dir=%s' % bb_utils.GOMA_DIR]
  build_targets = options.build_targets.split(',')
  bb_annotations.PrintNamedStep('compile')
  for build_target in build_targets:
    RunCmd(cmd + ['--build-args=%s' % build_target], halt_on_failure=True)
  if options.experimental:
    for compile_target in EXPERIMENTAL_TARGETS:
      bb_annotations.PrintNamedStep('Experimental Compile %s' % compile_target)
      RunCmd(cmd + ['--build-args=%s' % compile_target], flunk_on_failure=False)


def ZipBuild(options):
  bb_annotations.PrintNamedStep('zip_build')
  RunCmd([
      os.path.join(SLAVE_SCRIPTS_DIR, 'zip_build.py'),
      '--src-dir', constants.DIR_SOURCE_ROOT,
      '--build-dir', SrcPath('out'),
      '--exclude-files', 'lib.target,gen,android_webview,jingle_unittests']
      + bb_utils.EncodeProperties(options))


def ExtractBuild(options):
  bb_annotations.PrintNamedStep('extract_build')
  RunCmd(
      [os.path.join(SLAVE_SCRIPTS_DIR, 'extract_build.py'),
       '--build-dir', SrcPath('build'), '--build-output-dir',
       SrcPath('out')] + bb_utils.EncodeProperties(options),
       warning_code=1)


def FindBugs(options):
  bb_annotations.PrintNamedStep('findbugs')
  build_type = []
  if options.target == 'Release':
    build_type = ['--release-build']
  RunCmd([SrcPath('build', 'android', 'findbugs_diff.py')] + build_type)
  RunCmd([SrcPath(
      'tools', 'android', 'findbugs_plugin', 'test',
      'run_findbugs_plugin_tests.py')] + build_type)


def BisectPerfRegression(_):
  bb_annotations.PrintNamedStep('Bisect Perf Regression')
  RunCmd([SrcPath('tools', 'prepare-bisect-perf-regression.py'),
          '-w', os.path.join(constants.DIR_SOURCE_ROOT, os.pardir)])
  RunCmd([SrcPath('tools', 'run-bisect-perf-regression.py'),
          '-w', os.path.join(constants.DIR_SOURCE_ROOT, os.pardir)])


def GetHostStepCmds():
  return [
      ('compile', Compile),
      ('extract_build', ExtractBuild),
      ('check_webview_licenses', CheckWebViewLicenses),
      ('bisect_perf_regression', BisectPerfRegression),
      ('findbugs', FindBugs),
      ('zip_build', ZipBuild)
  ]


def GetHostStepsOptParser():
  parser = bb_utils.GetParser()
  parser.add_option('--steps', help='Comma separated list of host tests.')
  parser.add_option('--build-targets', default='All',
                    help='Comma separated list of build targets.')
  parser.add_option('--experimental', action='store_true',
                    help='Indicate whether to compile experimental targets.')

  return parser


def main(argv):
  parser = GetHostStepsOptParser()
  options, args = parser.parse_args(argv[1:])
  if args:
    return sys.exit('Unused args %s' % args)

  setattr(options, 'target', options.factory_properties.get('target', 'Debug'))

  if options.steps:
    bb_utils.RunSteps(options.steps.split(','), GetHostStepCmds(), options)


if __name__ == '__main__':
  sys.exit(main(sys.argv))
