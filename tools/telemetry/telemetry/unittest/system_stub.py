# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Provides stubs for os, sys and subprocess for testing

This test allows one to test code that itself uses os, sys, and subprocess.
"""

import os
import re
import shlex
import sys as real_sys

class Override(object):
  def __init__(self, base_module, module_list):
    stubs = {'adb_commands': AdbCommandsModuleStub,
             'os': OsModuleStub,
             'subprocess': SubprocessModuleStub,
             'sys': SysModuleStub,
    }
    self.adb_commands = None
    self.os = None
    self.subprocess = None
    self.sys = None

    self._base_module = base_module
    self._overrides = {}

    for module_name in module_list:
      self._overrides[module_name] = getattr(base_module, module_name)
      setattr(self, module_name, stubs[module_name]())
      setattr(base_module, module_name, getattr(self, module_name))

    if self.os and self.sys:
      self.os.path.sys = self.sys

  def __del__(self):
    assert not len(self._overrides)

  def Restore(self):
    for module_name, original_module in self._overrides.iteritems():
      setattr(self._base_module, module_name, original_module)
    self._overrides = {}

class AdbCommandsModuleStub(object):
# adb not even found
# android_browser_finder not returning
  class AdbCommandsStub(object):
    def __init__(self, module, device):
      self._module = module
      self._device = device
      self.is_root_enabled = True

    def RunShellCommand(self, args):
      if isinstance(args, basestring):
        args = shlex.split(args)
      handler = self._module.shell_command_handlers[args[0]]
      return handler(args)

    def IsRootEnabled(self):
      return self.is_root_enabled

  def __init__(self):
    self.attached_devices = []
    self.shell_command_handlers = {}

    def AdbCommandsStubConstructor(device=None):
      return AdbCommandsModuleStub.AdbCommandsStub(self, device)
    self.AdbCommands = AdbCommandsStubConstructor

  @staticmethod
  def IsAndroidSupported():
    return True

  def GetAttachedDevices(self):
    return self.attached_devices

  @staticmethod
  def HasForwarder(_=None):
    return True

class OsModuleStub(object):
  class OsPathModuleStub(object):
    def __init__(self, sys_module):
      self.sys = sys_module
      self.files = []

    def exists(self, path):
      return path in self.files

    def join(self, *paths):
      def IsAbsolutePath(path):
        if self.sys.platform.startswith('win'):
          return re.match('[a-zA-Z]:\\\\', path)
        else:
          return path.startswith('/')

      # Per Python specification, if any component is an absolute path,
      # discard previous components.
      for index, path in reversed(list(enumerate(paths))):
        if IsAbsolutePath(path):
          paths = paths[index:]
          break

      if self.sys.platform.startswith('win'):
        tmp = os.path.join(*paths)
        return tmp.replace('/', '\\')
      else:
        tmp = os.path.join(*paths)
        return tmp.replace('\\', '/')

    def expanduser(self, filename):
      return os.path.expanduser(filename)

    def dirname(self, filename): # pylint: disable=R0201
      return os.path.dirname(filename)

  def __init__(self, sys_module=real_sys):
    self.path = OsModuleStub.OsPathModuleStub(sys_module)
    self.display = ':0'
    self.local_app_data = None
    self.program_files = None
    self.program_files_x86 = None
    self.devnull = os.devnull

  def getenv(self, name):
    if name == 'DISPLAY':
      return self.display
    elif name == 'LOCALAPPDATA':
      return self.local_app_data
    elif name == 'PROGRAMFILES':
      return self.program_files
    elif name == 'PROGRAMFILES(X86)':
      return self.program_files_x86
    raise Exception('Unsupported getenv')

class SubprocessModuleStub(object):
  class PopenStub(object):
    def __init__(self):
      self.communicate_result = ('', '')

    def __call__(self, args, **kwargs):
      return self

    def communicate(self):
      return self.communicate_result

  def __init__(self):
    self.Popen = SubprocessModuleStub.PopenStub()
    self.PIPE = None

  def call(self, *args, **kwargs):
    raise NotImplementedError()

class SysModuleStub(object):
  def __init__(self):
    self.platform = ''
