#!/usr/bin/env python
# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Installs deps for using SDK emulator for testing.

The script will download the SDK and system images, if they are not present, and
install and enable KVM, if virtualization has been enabled in the BIOS.
"""


import logging
import os
import shutil
import sys

from pylib import cmd_helper
from pylib import constants
from pylib.utils import run_tests_helper

# From the Android Developer's website.
SDK_BASE_URL = 'http://dl.google.com/android/adt'
SDK_ZIP = 'adt-bundle-linux-x86_64-20130522.zip'

# Android x86 system image from the Intel website:
# http://software.intel.com/en-us/articles/intel-eula-x86-android-4-2-jelly-bean-bin
X86_IMG_URL = 'http://download-software.intel.com/sites/landingpage/android/sysimg_x86-17_r01.zip'

# Android API level
API_TARGET = 'android-%s' % constants.ANDROID_SDK_VERSION


def CheckSDK():
  """Check if SDK is already installed.

  Returns:
    True if android_tools directory exists in current directory.
  """
  return os.path.exists(os.path.join(constants.EMULATOR_SDK_ROOT,
                                     'android_tools'))


def CheckX86Image():
  """Check if Android system images have been installed.

  Returns:
    True if android_tools/sdk/system-images directory exists.
  """
  return os.path.exists(os.path.join(constants.EMULATOR_SDK_ROOT,
                                     'android_tools', 'sdk', 'system-images',
                                     API_TARGET, 'x86'))


def CheckKVM():
  """Check if KVM is enabled.

  Returns:
    True if kvm-ok returns 0 (already enabled)
  """
  try:
    return not cmd_helper.RunCmd(['kvm-ok'])
  except OSError:
    logging.info('kvm-ok not installed')
    return False


def GetSDK():
  """Download the SDK and unzip in android_tools directory."""
  logging.info('Download Android SDK.')
  sdk_url = '%s/%s' % (SDK_BASE_URL, SDK_ZIP)
  try:
    cmd_helper.RunCmd(['curl', '-o', '/tmp/sdk.zip', sdk_url])
    print 'curled unzipping...'
    rc = cmd_helper.RunCmd(['unzip', '-o', '/tmp/sdk.zip', '-d', '/tmp/'])
    if rc:
      logging.critical('ERROR: could not download/unzip Android SDK.')
      raise
    # Get the name of the sub-directory that everything will be extracted to.
    dirname, _ = os.path.splitext(SDK_ZIP)
    zip_dir = '/tmp/%s' % dirname
    # Move the extracted directory to EMULATOR_SDK_ROOT
    dst = os.path.join(constants.EMULATOR_SDK_ROOT, 'android_tools')
    shutil.move(zip_dir, dst)
  finally:
    os.unlink('/tmp/sdk.zip')


def InstallKVM():
  """Installs KVM packages."""
  rc = cmd_helper.RunCmd(['sudo', 'apt-get', 'install', 'kvm'])
  if rc:
    logging.critical('ERROR: Did not install KVM. Make sure hardware '
                     'virtualization is enabled in BIOS (i.e. Intel VT-x or '
                     'AMD SVM).')
  # TODO(navabi): Use modprobe kvm-amd on AMD processors.
  rc = cmd_helper.RunCmd(['sudo', 'modprobe', 'kvm-intel'])
  if rc:
    logging.critical('ERROR: Did not add KVM module to Linux Kernal. Make sure '
                     'hardware virtualization is enabled in BIOS.')
  # Now check to ensure KVM acceleration can be used.
  rc = cmd_helper.RunCmd(['kvm-ok'])
  if rc:
    logging.critical('ERROR: Can not use KVM acceleration. Make sure hardware '
                     'virtualization is enabled in BIOS (i.e. Intel VT-x or '
                     'AMD SVM).')


def GetX86Image():
  """Download x86 system image from Intel's website."""
  logging.info('Download x86 system image directory into sdk directory.')
  try:
    cmd_helper.RunCmd(['curl', '-o', '/tmp/x86_img.zip', X86_IMG_URL])
    rc = cmd_helper.RunCmd(['unzip', '-o', '/tmp/x86_img.zip', '-d', '/tmp/'])
    if rc:
      logging.critical('ERROR: Could not download/unzip image zip.')
      raise
    sys_imgs = os.path.join(constants.EMULATOR_SDK_ROOT, 'android_tools', 'sdk',
                            'system-images', API_TARGET, 'x86')
    shutil.move('/tmp/x86', sys_imgs)
  finally:
    os.unlink('/tmp/x86_img.zip')


def main(argv):
  logging.basicConfig(level=logging.INFO,
                      format='# %(asctime)-15s: %(message)s')
  run_tests_helper.SetLogLevel(verbose_count=1)

  # Calls below will download emulator SDK and/or system images only if needed.
  if CheckSDK():
    logging.info('android_tools directory already exists (not downloading).')
  else:
    GetSDK()

  logging.info('Emulator deps for ARM emulator complete.')

  if CheckX86Image():
    logging.info('system-images directory already exists.')
  else:
    GetX86Image()

  # Make sure KVM packages are installed and enabled.
  if CheckKVM():
    logging.info('KVM already installed and enabled.')
  else:
    InstallKVM()


if __name__ == '__main__':
  sys.exit(main(sys.argv))
