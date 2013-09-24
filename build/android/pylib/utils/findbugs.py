#!/usr/bin/env python
#
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import optparse
import os
import re
import shlex
import subprocess
import sys

from pylib import cmd_helper
from pylib import constants


def _PrintMessage(warnings, title, action, known_bugs_file):
  if warnings:
    print
    print '*' * 80
    print '%s warnings.' % title
    print '%s %s' % (action, known_bugs_file)
    print '-' * 80
    for warning in warnings:
      print warning
    print '-' * 80
    print


def _StripLineNumbers(current_warnings):
  re_line = r':\[line.*?\]$'
  return [re.sub(re_line, '', x) for x in current_warnings]


def _DiffKnownWarnings(current_warnings_set, known_bugs_file):
  with open(known_bugs_file, 'r') as known_bugs:
    known_bugs_set = set(known_bugs.read().splitlines())

  new_warnings = current_warnings_set - known_bugs_set
  _PrintMessage(sorted(new_warnings), 'New', 'Please fix, or perhaps add to',
                known_bugs_file)

  obsolete_warnings = known_bugs_set - current_warnings_set
  _PrintMessage(sorted(obsolete_warnings), 'Obsolete', 'Please remove from',
                known_bugs_file)

  count = len(new_warnings) + len(obsolete_warnings)
  if count:
    print '*** %d FindBugs warning%s! ***' % (count, 's' * (count > 1))
    if len(new_warnings):
      print '*** %d: new ***' % len(new_warnings)
    if len(obsolete_warnings):
      print '*** %d: obsolete ***' % len(obsolete_warnings)
    print
    print 'Alternatively,  rebaseline with --rebaseline command option'
    print
  else:
    print 'No new FindBugs warnings.'
  print
  return count


def _Rebaseline(current_warnings_set, known_bugs_file):
  with file(known_bugs_file, 'w') as known_bugs:
    for warning in sorted(current_warnings_set):
      print >>known_bugs, warning
  return 0


def _GetChromeClasses(release_version):
  version = 'Debug'
  if release_version:
    version = 'Release'
  path = os.path.join(constants.DIR_SOURCE_ROOT, 'out', version)
  cmd = 'find %s -name "*.class"' % path
  out = cmd_helper.GetCmdOutput(shlex.split(cmd))
  if not out:
    print 'No classes found in %s' % path
  return out


def _Run(exclude, known_bugs, classes_to_analyze, auxiliary_classes,
        rebaseline, release_version, findbug_args):
  """Run the FindBugs.

  Args:
    exclude: the exclude xml file, refer to FindBugs's -exclude command option.
    known_bugs: the text file of known bugs. The bugs in it will not be
                reported.
    classes_to_analyze: the list of classes need to analyze, refer to FindBug's
                        -onlyAnalyze command line option.
    auxiliary_classes: the classes help to analyze, refer to FindBug's
                       -auxclasspath command line option.
    rebaseline: True if the known_bugs file needs rebaseline.
    release_version: True if the release version needs check, otherwise check
                     debug version.
    findbug_args: addtional command line options needs pass to Findbugs.
  """

  chrome_src = constants.DIR_SOURCE_ROOT
  sdk_root = constants.ANDROID_SDK_ROOT
  sdk_version = constants.ANDROID_SDK_VERSION

  system_classes = []
  system_classes.append(os.path.join(sdk_root, 'platforms',
                                     'android-%s' % sdk_version, 'android.jar'))
  if auxiliary_classes:
    for classes in auxiliary_classes:
      system_classes.append(os.path.abspath(classes))

  cmd = '%s -textui -sortByClass ' % os.path.join(chrome_src, 'third_party',
                                                  'findbugs', 'bin', 'findbugs')
  cmd = '%s -pluginList %s' % (cmd, os.path.join(chrome_src, 'tools', 'android',
                                                 'findbugs_plugin', 'lib',
                                                 'chromiumPlugin.jar'))
  if len(system_classes):
    cmd = '%s -auxclasspath %s ' % (cmd, ':'.join(system_classes))

  if classes_to_analyze:
    cmd = '%s -onlyAnalyze %s ' % (cmd, classes_to_analyze)

  if exclude:
    cmd = '%s -exclude %s ' % (cmd, os.path.abspath(exclude))

  if findbug_args:
    cmd = '%s %s ' % (cmd, fingbug_args)


  chrome_classes = _GetChromeClasses(release_version)
  if not chrome_classes:
    return 1
  cmd = '%s %s ' % (cmd, chrome_classes)

  proc = subprocess.Popen(shlex.split(cmd),
                          stdout=subprocess.PIPE, stderr=subprocess.PIPE)
  out, err = proc.communicate()
  current_warnings_set = set(_StripLineNumbers(filter(None, out.splitlines())))

  if rebaseline:
    return _Rebaseline(current_warnings_set, known_bugs)
  else:
    return _DiffKnownWarnings(current_warnings_set, known_bugs)

def Run(options):
  exclude_file = None
  known_bugs_file = None

  if options.exclude:
    exclude_file = options.exclude
  elif options.base_dir:
    exclude_file = os.path.join(options.base_dir, 'findbugs_exclude.xml')

  if options.known_bugs:
    known_bugs_file = options.known_bugs
  elif options.base_dir:
    known_bugs_file = os.path.join(options.base_dir, 'findbugs_known_bugs.txt')

  auxclasspath = None
  if options.auxclasspath:
    auxclasspath = options.auxclasspath.split(':')
  return _Run(exclude_file, known_bugs_file, options.only_analyze, auxclasspath,
              options.rebaseline, options.release_build, options.findbug_args)


def GetCommonParser():
  parser = optparse.OptionParser()
  parser.add_option('-r',
                    '--rebaseline',
                    action='store_true',
                    dest='rebaseline',
                    help='Rebaseline known findbugs issues.')

  parser.add_option('-a',
                    '--auxclasspath',
                    action='store',
                    default=None,
                    dest='auxclasspath',
                    help='Set aux classpath for analysis.')

  parser.add_option('-o',
                    '--only-analyze',
                    action='store',
                    default=None,
                    dest='only_analyze',
                    help='Only analyze the given classes and packages.')

  parser.add_option('-e',
                    '--exclude',
                    action='store',
                    default=None,
                    dest='exclude',
                    help='Exclude bugs matching given filter.')

  parser.add_option('-k',
                    '--known-bugs',
                    action='store',
                    default=None,
                    dest='known_bugs',
                    help='Not report the bugs in the given file.')

  parser.add_option('-l',
                    '--release-build',
                    action='store_true',
                    dest='release_build',
                    help='Analyze release build instead of debug.')

  parser.add_option('-f',
                    '--findbug-args',
                    action='store',
                    default=None,
                    dest='findbug_args',
                    help='Additional findbug arguments.')

  parser.add_option('-b',
                    '--base-dir',
                    action='store',
                    default=None,
                    dest='base_dir',
                    help='Base directory for configuration file.')

  return parser


def main(argv):
  parser = GetCommonParser()
  options, _ = parser.parse_args()

  return Run(options)


if __name__ == '__main__':
  sys.exit(main(sys.argv))
