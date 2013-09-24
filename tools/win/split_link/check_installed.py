# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys


def IsAvailable():
  _winreg = None
  if sys.platform == 'win32':
    import _winreg
  elif sys.platform == 'cygwin':
    try:
      import cygwinreg as _winreg
    except ImportError:
      pass  # If not available, be safe and write 0.

  if not _winreg:
    return False

  try:
    val = _winreg.QueryValue(_winreg.HKEY_CURRENT_USER,
                             'Software\\Chromium\\split_link_installed')
    return os.path.exists(val)
  except WindowsError:
    pass

  return False


def main():
  # Can be called from gyp to set variable.
  if IsAvailable():
    sys.stdout.write('1')
  else:
    print >> sys.stderr, "Couldn't find split_link installation."
    print >> sys.stderr, ('Run python tools\\win\\split_link\\'
                          'install_split_link.py from an elevated Visual '
                          'Studio Command Prompt to install.')
    sys.stdout.write('0')
    return 1


if __name__ == '__main__':
  sys.exit(main())
