#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
This file holds a list of reasons why a particular build needs to be clobbered
(or a list of 'landmines').

This script runs every build as a hook. If it detects that the build should
be clobbered, it will touch the file <build_dir>/.landmine_triggered. The
various build scripts will then check for the presence of this file and clobber
accordingly. The script will also emit the reasons for the clobber to stdout.

A landmine is tripped when a builder checks out a different revision, and the
diff between the new landmines and the old ones is non-null. At this point, the
build is clobbered.
"""

import difflib
import functools
import gyp_helper
import logging
import optparse
import os
import shlex
import sys
import time

SRC_DIR = os.path.dirname(os.path.dirname(os.path.realpath(__file__)))

def memoize(default=None):
  """This decorator caches the return value of a parameterless pure function"""
  def memoizer(func):
    val = []
    @functools.wraps(func)
    def inner():
      if not val:
        ret = func()
        val.append(ret if ret is not None else default)
        if logging.getLogger().isEnabledFor(logging.INFO):
          print '%s -> %r' % (func.__name__, val[0])
      return val[0]
    return inner
  return memoizer


@memoize()
def IsWindows():
  return sys.platform in ['win32', 'cygwin']


@memoize()
def IsLinux():
  return sys.platform.startswith('linux')


@memoize()
def IsMac():
  return sys.platform == 'darwin'


@memoize()
def gyp_defines():
  """Parses and returns GYP_DEFINES env var as a dictionary."""
  return dict(arg.split('=', 1)
      for arg in shlex.split(os.environ.get('GYP_DEFINES', '')))

@memoize()
def gyp_msvs_version():
  return os.environ.get('GYP_MSVS_VERSION', '')

@memoize()
def distributor():
  """
  Returns a string which is the distributed build engine in use (if any).
  Possible values: 'goma', 'ib', ''
  """
  if 'goma' in gyp_defines():
    return 'goma'
  elif IsWindows():
    if 'CHROME_HEADLESS' in os.environ:
      return 'ib' # use (win and !goma and headless) as approximation of ib


@memoize()
def platform():
  """
  Returns a string representing the platform this build is targetted for.
  Possible values: 'win', 'mac', 'linux', 'ios', 'android'
  """
  if 'OS' in gyp_defines():
    if 'android' in gyp_defines()['OS']:
      return 'android'
    else:
      return gyp_defines()['OS']
  elif IsWindows():
    return 'win'
  elif IsLinux():
    return 'linux'
  else:
    return 'mac'


@memoize()
def builder():
  """
  Returns a string representing the build engine (not compiler) to use.
  Possible values: 'make', 'ninja', 'xcode', 'msvs', 'scons'
  """
  if 'GYP_GENERATORS' in os.environ:
    # for simplicity, only support the first explicit generator
    generator = os.environ['GYP_GENERATORS'].split(',')[0]
    if generator.endswith('-android'):
      return generator.split('-')[0]
    elif generator.endswith('-ninja'):
      return 'ninja'
    else:
      return generator
  else:
    if platform() == 'android':
      # Good enough for now? Do any android bots use make?
      return 'ninja'
    elif platform() == 'ios':
      return 'xcode'
    elif IsWindows():
      return 'msvs'
    elif IsLinux():
      return 'ninja'
    elif IsMac():
      return 'xcode'
    else:
      assert False, 'Don\'t know what builder we\'re using!'


def get_landmines(target):
  """
  ALL LANDMINES ARE DEFINED HERE.
  target is 'Release' or 'Debug'
  """
  landmines = []
  add = lambda item: landmines.append(item + '\n')

  if (distributor() == 'goma' and platform() == 'win32' and
      builder() == 'ninja'):
    add('Need to clobber winja goma due to backend cwd cache fix.')
  if platform() == 'android':
    add('Clobber: Resources removed in r195014 require clobber.')
  if platform() == 'win' and builder() == 'ninja':
    add('Compile on cc_unittests fails due to symbols removed in r185063.')
  if platform() == 'linux' and builder() == 'ninja':
    add('Builders switching from make to ninja will clobber on this.')
  if platform() == 'mac':
    add('Switching from bundle to unbundled dylib (issue 14743002).')
  if (platform() == 'win' and builder() == 'ninja' and
      gyp_msvs_version() == '2012' and
      gyp_defines().get('target_arch') == 'x64' and
      gyp_defines().get('dcheck_always_on') == '1'):
    add("Switched win x64 trybots from VS2010 to VS2012.")
  add('Need to clobber everything due to an IDL change in r154579 (blink)')

  return landmines


def get_target_build_dir(build_tool, target, is_iphone=False):
  """
  Returns output directory absolute path dependent on build and targets.
  Examples:
    r'c:\b\build\slave\win\build\src\out\Release'
    '/mnt/data/b/build/slave/linux/build/src/out/Debug'
    '/b/build/slave/ios_rel_device/build/src/xcodebuild/Release-iphoneos'

  Keep this function in sync with tools/build/scripts/slave/compile.py
  """
  ret = None
  if build_tool == 'xcode':
    ret = os.path.join(SRC_DIR, 'xcodebuild',
        target + ('-iphoneos' if is_iphone else ''))
  elif build_tool in ['make', 'ninja', 'ninja-ios']:  # TODO: Remove ninja-ios.
    ret = os.path.join(SRC_DIR, 'out', target)
  elif build_tool in ['msvs', 'vs', 'ib']:
    ret = os.path.join(SRC_DIR, 'build', target)
  elif build_tool == 'scons':
    ret = os.path.join(SRC_DIR, 'sconsbuild', target)
  else:
    raise NotImplementedError('Unexpected GYP_GENERATORS (%s)' % build_tool)
  return os.path.abspath(ret)


def set_up_landmines(target):
  """Does the work of setting, planting, and triggering landmines."""
  out_dir = get_target_build_dir(builder(), target, platform() == 'ios')

  landmines_path = os.path.join(out_dir, '.landmines')
  if not os.path.exists(out_dir):
    os.makedirs(out_dir)

  new_landmines = get_landmines(target)

  if not os.path.exists(landmines_path):
    with open(landmines_path, 'w') as f:
      f.writelines(new_landmines)
  else:
    triggered = os.path.join(out_dir, '.landmines_triggered')
    with open(landmines_path, 'r') as f:
      old_landmines = f.readlines()
    if old_landmines != new_landmines:
      old_date = time.ctime(os.stat(landmines_path).st_ctime)
      diff = difflib.unified_diff(old_landmines, new_landmines,
          fromfile='old_landmines', tofile='new_landmines',
          fromfiledate=old_date, tofiledate=time.ctime(), n=0)

      with open(triggered, 'w') as f:
        f.writelines(diff)
    elif os.path.exists(triggered):
      # Remove false triggered landmines.
      os.remove(triggered)


def main():
  parser = optparse.OptionParser()
  parser.add_option('-v', '--verbose', action='store_true',
      default=('LANDMINES_VERBOSE' in os.environ),
      help=('Emit some extra debugging information (default off). This option '
          'is also enabled by the presence of a LANDMINES_VERBOSE environment '
          'variable.'))
  options, args = parser.parse_args()

  if args:
    parser.error('Unknown arguments %s' % args)

  logging.basicConfig(
      level=logging.DEBUG if options.verbose else logging.ERROR)

  gyp_helper.apply_chromium_gyp_env()

  for target in ('Debug', 'Release', 'Debug_x64', 'Release_x64'):
    set_up_landmines(target)

  return 0


if __name__ == '__main__':
  sys.exit(main())
