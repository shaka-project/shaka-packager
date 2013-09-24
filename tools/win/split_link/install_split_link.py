# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import _winreg
import os
import shutil
import subprocess
import sys


BASE_DIR = os.path.dirname(os.path.abspath(__file__))


def IsExe(fpath):
  return os.path.isfile(fpath) and os.access(fpath, os.X_OK)


def FindInPath(program):
  fpath, _ = os.path.split(program)
  if fpath:
    if IsExe(program):
      return program
  else:
    for path in os.environ['PATH'].split(os.pathsep):
      path = path.strip('"')
      exe_file = os.path.join(path, program)
      if not path or not os.path.isabs(path):
        continue
      if IsExe(exe_file):
        return exe_file
  return None


def EscapeForCommandLineAndCString(path):
  """Quoted sufficiently to be passed on the compile command line as a define
  to be turned into a string in the target C program."""
  path = '"' + path + '"'
  return path.replace('\\', '\\\\').replace('"', '\\"')


def main():
  # Switch to our own dir.
  os.chdir(BASE_DIR)

  link = FindInPath('link.exe')
  mt = FindInPath('mt.exe')
  if not link or not mt:
    print("Couldn't find link.exe or mt.exe in PATH. "
          "Must run from Administrator Visual Studio Command Prompt.")
    return 1

  link_backup = os.path.join(os.path.split(link)[0], 'link.exe.split_link.exe')

  # Don't re-backup link.exe, so only copy link.exe to backup if it's
  # not there already.
  if not os.path.exists(link_backup):
    try:
      print 'Saving original link.exe...'
      shutil.copyfile(link, link_backup)
    except IOError:
      print(("Wasn't able to back up %s to %s. "
             "Not running with Administrator privileges?")
              % (link, link_backup))
      return 1

  # Build our linker shim.
  print 'Building split_link.exe...'
  split_link_py = os.path.abspath('split_link.py')
  script_path = EscapeForCommandLineAndCString(split_link_py)
  python = EscapeForCommandLineAndCString(sys.executable)
  subprocess.check_call('cl.exe /nologo /Ox /Zi /W4 /WX /D_UNICODE /DUNICODE'
                        ' /D_CRT_SECURE_NO_WARNINGS /EHsc split_link.cc'
                        ' /DPYTHON_PATH="%s"'
                        ' /DSPLIT_LINK_SCRIPT_PATH="%s"'
                        ' /link shell32.lib shlwapi.lib /out:split_link.exe' % (
                            python, script_path))

  # Copy shim into place.
  print 'Copying split_link.exe over link.exe...'
  try:
    shutil.copyfile('split_link.exe', link)
    _winreg.SetValue(_winreg.HKEY_CURRENT_USER,
                     'Software\\Chromium\\split_link_installed',
                     _winreg.REG_SZ,
                     link_backup)
    _winreg.SetValue(_winreg.HKEY_CURRENT_USER,
                     'Software\\Chromium\\split_link_mt_path',
                     _winreg.REG_SZ,
                     mt)
  except IOError:
    print("Wasn't able to copy split_link.exe over %s. "
          "Not running with Administrator privileges?" % link)
    return 1

  return 0


if __name__ == '__main__':
  sys.exit(main())
