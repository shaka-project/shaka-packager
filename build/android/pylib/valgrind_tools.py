# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Classes in this file define additional actions that need to be taken to run a
test under some kind of runtime error detection tool.

The interface is intended to be used as follows.

1. For tests that simply run a native process (i.e. no activity is spawned):

Call tool.CopyFiles().
Prepend test command line with tool.GetTestWrapper().

2. For tests that spawn an activity:

Call tool.CopyFiles().
Call tool.SetupEnvironment().
Run the test as usual.
Call tool.CleanUpEnvironment().
"""

import os.path
import sys
from glob import glob

from constants import DIR_SOURCE_ROOT


def SetChromeTimeoutScale(adb, scale):
  """Sets the timeout scale in /data/local/tmp/chrome_timeout_scale to scale."""
  path = '/data/local/tmp/chrome_timeout_scale'
  if not scale or scale == 1.0:
    # Delete if scale is None/0.0/1.0 since the default timeout scale is 1.0
    adb.RunShellCommand('rm %s' % path)
  else:
    adb.SetProtectedFileContents(path, '%f' % scale)


class BaseTool(object):
  """A tool that does nothing."""

  def GetTestWrapper(self):
    """Returns a string that is to be prepended to the test command line."""
    return ''

  def GetUtilWrapper(self):
    """Returns the wrapper name for the utilities.

    Returns:
       A string that is to be prepended to the command line of utility
    processes (forwarder, etc.).
    """
    return ''

  def CopyFiles(self):
    """Copies tool-specific files to the device, create directories, etc."""
    pass

  def SetupEnvironment(self):
    """Sets up the system environment for a test.

    This is a good place to set system properties.
    """
    pass

  def CleanUpEnvironment(self):
    """Cleans up environment."""
    pass

  def GetTimeoutScale(self):
    """Returns a multiplier that should be applied to timeout values."""
    return 1.0

  def NeedsDebugInfo(self):
    """Whether this tool requires debug info.

    Returns:
      True if this tool can not work with stripped binaries.
    """
    return False


class AddressSanitizerTool(BaseTool):
  """AddressSanitizer tool."""

  TMP_DIR = '/data/local/tmp/asan'
  WRAPPER_NAME = 'asanwrapper.sh'

  def __init__(self, adb):
    self._adb = adb
    self._wrap_properties = ['wrap.com.google.android.apps.ch',
                             'wrap.org.chromium.native_test',
                             'wrap.org.chromium.content_shell',
                             'wrap.org.chromium.chrome.testsh',
                             'wrap.org.chromium.android_webvi']
    # Configure AndroidCommands to run utils (such as md5sum_bin) under ASan.
    # This is required because ASan is a compiler-based tool, and md5sum
    # includes instrumented code from base.
    adb.SetUtilWrapper(self.GetUtilWrapper())

  def CopyFiles(self):
    """Copies ASan tools to the device."""
    files = (['tools/android/asan/asanwrapper.sh'] +
              glob('third_party/llvm-build/Release+Asserts/lib/clang/*/lib/'
                   'linux/libclang_rt.asan-arm-android.so'))
    for f in files:
      self._adb.PushIfNeeded(os.path.join(DIR_SOURCE_ROOT, f),
                             os.path.join(AddressSanitizerTool.TMP_DIR,
                                          os.path.basename(f)))

  def GetTestWrapper(self):
    return os.path.join(AddressSanitizerTool.TMP_DIR,
                        AddressSanitizerTool.WRAPPER_NAME)

  def GetUtilWrapper(self):
    """Returns the wrapper for utilities, such as forwarder.

    AddressSanitizer wrapper must be added to all instrumented binaries,
    including forwarder and the like. This can be removed if such binaries
    were built without instrumentation. """
    return self.GetTestWrapper()

  def SetupEnvironment(self):
    self._adb.EnableAdbRoot()
    for prop in self._wrap_properties:
      self._adb.RunShellCommand('setprop %s "logwrapper %s"' % (
          prop, self.GetTestWrapper()))
    SetChromeTimeoutScale(self._adb, self.GetTimeoutScale())

  def CleanUpEnvironment(self):
    for prop in self._wrap_properties:
      self._adb.RunShellCommand('setprop %s ""' % (prop,))
    SetChromeTimeoutScale(self._adb, None)

  def GetTimeoutScale(self):
    # Very slow startup.
    return 20.0


class ValgrindTool(BaseTool):
  """Base abstract class for Valgrind tools."""

  VG_DIR = '/data/local/tmp/valgrind'
  VGLOGS_DIR = '/data/local/tmp/vglogs'

  def __init__(self, adb):
    self._adb = adb
    # exactly 31 chars, SystemProperties::PROP_NAME_MAX
    self._wrap_properties = ['wrap.com.google.android.apps.ch',
                             'wrap.org.chromium.native_test']

  def CopyFiles(self):
    """Copies Valgrind tools to the device."""
    self._adb.RunShellCommand('rm -r %s; mkdir %s' %
                              (ValgrindTool.VG_DIR, ValgrindTool.VG_DIR))
    self._adb.RunShellCommand('rm -r %s; mkdir %s' %
                              (ValgrindTool.VGLOGS_DIR,
                               ValgrindTool.VGLOGS_DIR))
    files = self.GetFilesForTool()
    for f in files:
      self._adb.PushIfNeeded(os.path.join(DIR_SOURCE_ROOT, f),
                             os.path.join(ValgrindTool.VG_DIR,
                                          os.path.basename(f)))

  def SetupEnvironment(self):
    """Sets up device environment."""
    self._adb.RunShellCommand('chmod 777 /data/local/tmp')
    for prop in self._wrap_properties:
      self._adb.RunShellCommand('setprop %s "logwrapper %s"' % (
          prop, self.GetTestWrapper()))
    SetChromeTimeoutScale(self._adb, self.GetTimeoutScale())

  def CleanUpEnvironment(self):
    """Cleans up device environment."""
    for prop in self._wrap_properties:
      self._adb.RunShellCommand('setprop %s ""' % (prop,))
    SetChromeTimeoutScale(self._adb, None)

  def GetFilesForTool(self):
    """Returns a list of file names for the tool."""
    raise NotImplementedError()

  def NeedsDebugInfo(self):
    """Whether this tool requires debug info.

    Returns:
      True if this tool can not work with stripped binaries.
    """
    return True


class MemcheckTool(ValgrindTool):
  """Memcheck tool."""

  def __init__(self, adb):
    super(MemcheckTool, self).__init__(adb)

  def GetFilesForTool(self):
    """Returns a list of file names for the tool."""
    return ['tools/valgrind/android/vg-chrome-wrapper.sh',
            'tools/valgrind/memcheck/suppressions.txt',
            'tools/valgrind/memcheck/suppressions_android.txt']

  def GetTestWrapper(self):
    """Returns a string that is to be prepended to the test command line."""
    return ValgrindTool.VG_DIR + '/' + 'vg-chrome-wrapper.sh'

  def GetTimeoutScale(self):
    """Returns a multiplier that should be applied to timeout values."""
    return 30


class TSanTool(ValgrindTool):
  """ThreadSanitizer tool. See http://code.google.com/p/data-race-test ."""

  def __init__(self, adb):
    super(TSanTool, self).__init__(adb)

  def GetFilesForTool(self):
    """Returns a list of file names for the tool."""
    return ['tools/valgrind/android/vg-chrome-wrapper-tsan.sh',
            'tools/valgrind/tsan/suppressions.txt',
            'tools/valgrind/tsan/suppressions_android.txt',
            'tools/valgrind/tsan/ignores.txt']

  def GetTestWrapper(self):
    """Returns a string that is to be prepended to the test command line."""
    return ValgrindTool.VG_DIR + '/' + 'vg-chrome-wrapper-tsan.sh'

  def GetTimeoutScale(self):
    """Returns a multiplier that should be applied to timeout values."""
    return 30.0


TOOL_REGISTRY = {
    'memcheck': lambda x: MemcheckTool(x),
    'memcheck-renderer': lambda x: MemcheckTool(x),
    'tsan': lambda x: TSanTool(x),
    'tsan-renderer': lambda x: TSanTool(x),
    'asan': lambda x: AddressSanitizerTool(x),
}


def CreateTool(tool_name, adb):
  """Creates a tool with the specified tool name.

  Args:
    tool_name: Name of the tool to create.
    adb: ADB interface the tool will use.
  Returns:
    A tool for the specified tool_name.
  """
  if not tool_name:
    return BaseTool()

  ctor = TOOL_REGISTRY.get(tool_name)
  if ctor:
    return ctor(adb)
  else:
    print 'Unknown tool %s, available tools: %s' % (
        tool_name, ', '.join(sorted(TOOL_REGISTRY.keys())))
    sys.exit(1)
