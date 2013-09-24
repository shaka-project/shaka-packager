#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Script to install ARM root image for cross building of ARM chrome on linux.
# This script can be run manually but is more often run as part of gclient
# hooks. When run from hooks this script should be a no-op on non-linux
# platforms.

# The sysroot image could be constructed from scratch based on the current
# state or precise/arm but for consistency we currently use a pre-built root
# image which was originally designed for building trusted NaCl code. The image
# will normally need to be rebuilt every time chrome's build dependancies are
# changed.

import os
import shutil
import subprocess
import sys


SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
URL_PREFIX = 'https://commondatastorage.googleapis.com'
URL_PATH = 'nativeclient-archive2/toolchain'
REVISION = 10991
TARBALL = 'naclsdk_linux_arm-trusted.tgz'


def main(args):
  if '--linux-only' in args:
    # This argument is passed when run from the gclient hooks.
    # In this case we return early on non-linux platforms
    # or if GYP_DEFINES doesn't include target_arch=arm
    if not sys.platform.startswith('linux'):
      return 0

    if "target_arch=arm" not in os.environ.get('GYP_DEFINES', ''):
      return 0

  src_root = os.path.dirname(os.path.dirname(SCRIPT_DIR))
  sysroot = os.path.join(src_root, 'arm-sysroot')
  url = "%s/%s/%s/%s" % (URL_PREFIX, URL_PATH, REVISION, TARBALL)

  stamp = os.path.join(sysroot, ".stamp")
  if os.path.exists(stamp):
    with open(stamp) as s:
      if s.read() == url:
        print "ARM root image already up-to-date: %s" % sysroot
        return 0

  print "Installing ARM root image: %s" % sysroot
  if os.path.isdir(sysroot):
    shutil.rmtree(sysroot)
  os.mkdir(sysroot)
  tarball = os.path.join(sysroot, TARBALL)
  subprocess.check_call(['curl', '-L', url, '-o', tarball])
  subprocess.check_call(['tar', 'xf', tarball, '-C', sysroot])
  os.remove(tarball)

  with open(stamp, 'w') as s:
    s.write(url)
  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
