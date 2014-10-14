#!/usr/bin/python
#
# Copyright 2014 Google Inc. All Rights Reserved.
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
import sys


# Define static global objects and attributes.
SRC_DIR = os.path.join(os.path.dirname(os.path.realpath(__file__)), '../../..')


# Parse arguments and calculate dynamic global objects and attributes.
parser = argparse.ArgumentParser()
common = parser.add_argument_group(
    'encryption flags',
    'These flags are required to enable AES and/or RSA encryption tests.')
common.add_argument('--key_server_url')
common.add_argument('--content_id')
common.add_argument('--signer')
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
if (options.key_server_url and options.content_id and options.signer and
    options.aes_signing_key and options.aes_signing_iv):
  has_aes_flags = True
has_rsa_flags = False
if (options.key_server_url and options.content_id and options.signer and
    options.rsa_signing_key_path):
  has_rsa_flags = True
