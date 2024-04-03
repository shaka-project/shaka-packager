# Copyright 2014 Google LLC. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file or at
# https://developers.google.com/open-source/licenses/bsd

"""Packager testing global objects and attributes.

Please do not refer to this module directly. Please set attributes
either by updating the default values below, or by passing the requested
flags through the command line interface.
"""


import argparse
import os
import platform
import sys


def GetBinaryName(name):
  if platform.system() == 'Windows':
    name += '.exe'
  return name

# Define static global objects and attributes.
SCRIPT_DIR = os.path.dirname(os.path.realpath(__file__))
SRC_DIR = os.environ.get('PACKAGER_SRC_DIR')
if not SRC_DIR:
  # fallback to computing src dir from script dir
  SRC_DIR = os.path.join(SCRIPT_DIR, os.pardir, os.pardir)

PACKAGER_BIN = os.environ.get('PACKAGER_BIN')
if not PACKAGER_BIN:
  PACKAGER_BIN = os.path.join(SCRIPT_DIR,
                              GetBinaryName('packager'))

MPD_GENERATOR_BIN = os.environ.get('MPD_GENERATOR_BIN')
if not MPD_GENERATOR_BIN:
  MPD_GENERATOR_BIN = os.path.join(SCRIPT_DIR,
                                   GetBinaryName('mpd_generator'))

BUILD_TYPE = os.environ.get('BUILD_TYPE', 'static')

# Parse arguments and calculate dynamic global objects and attributes.
parser = argparse.ArgumentParser()
parser.add_argument('--test_update_golden_files', action='store_true')

parser.add_argument('--v')
parser.add_argument('--vmodule')
# Overwrite the test to encryption key/iv specified in the command line.
parser.add_argument('--encryption_key')
parser.add_argument('--encryption_iv')

parser.add_argument('--remove_temp_files_after_test',
                    dest='remove_test_files_after_test', action='store_true')
parser.add_argument('--no-remove_temp_files_after_test',
                    dest='remove_temp_files_after_test', action='store_false')
parser.set_defaults(remove_temp_files_after_test=True)

aes = parser.add_argument_group(
    'aes flags',
    'These flags are required to enable AES signed encryption tests.')
aes.add_argument('--aes_signing_key')
aes.add_argument('--aes_signing_iv')
rsa = parser.add_argument_group(
    'rsa flags',
    'These flags are required to enable RSA signed encryption tests.')
rsa.add_argument('--rsa_signing_key_path')

options, args = parser.parse_known_args()
sys.argv[1:] = args
has_aes_flags = False
if options.aes_signing_key and options.aes_signing_iv:
  has_aes_flags = True
has_rsa_flags = False
if options.rsa_signing_key_path:
  has_rsa_flags = True
