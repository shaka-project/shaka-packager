#!/usr/bin/env python
#
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Signs and zipaligns APK.

"""

import optparse
import os
import shutil
import sys
import tempfile

from util import build_utils

def SignApk(keystore_path, unsigned_path, signed_path):
  shutil.copy(unsigned_path, signed_path)
  sign_cmd = [
      'jarsigner',
      '-sigalg', 'MD5withRSA',
      '-digestalg', 'SHA1',
      '-keystore', keystore_path,
      '-storepass', 'chromium',
      signed_path,
      'chromiumdebugkey',
    ]
  build_utils.CheckCallDie(sign_cmd)


def AlignApk(android_sdk_root, unaligned_path, final_path):
  align_cmd = [
      os.path.join(android_sdk_root, 'tools', 'zipalign'),
      '-f', '4',  # 4 bytes
      unaligned_path,
      final_path,
      ]
  build_utils.CheckCallDie(align_cmd)


def main(argv):
  parser = optparse.OptionParser()

  parser.add_option('--android-sdk-root', help='Android sdk root directory.')
  parser.add_option('--unsigned-apk-path', help='Path to input unsigned APK.')
  parser.add_option('--final-apk-path',
      help='Path to output signed and aligned APK.')
  parser.add_option('--keystore-path', help='Path to keystore for signing.')
  parser.add_option('--stamp', help='Path to touch on success.')

  # TODO(newt): remove this once http://crbug.com/177552 is fixed in ninja.
  parser.add_option('--ignore', help='Ignored.')

  options, _ = parser.parse_args()

  with tempfile.NamedTemporaryFile() as intermediate_file:
    signed_apk_path = intermediate_file.name
    SignApk(options.keystore_path, options.unsigned_apk_path, signed_apk_path)
    AlignApk(options.android_sdk_root, signed_apk_path, options.final_apk_path)

  if options.stamp:
    build_utils.Touch(options.stamp)


if __name__ == '__main__':
  sys.exit(main(sys.argv))


