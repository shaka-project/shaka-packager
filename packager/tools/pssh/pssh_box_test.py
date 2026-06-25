#!/usr/bin/env python3
# Copyright 2026 Google LLC. All rights reserved.
#
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file or at
# https://developers.google.com/open-source/licenses/bsd

"""End-to-end tests for pssh-box.py.

Runs the installed script as a subprocess from its build directory so it
picks up the bundled pssh-box-protos/ runtime. Regression coverage for:

  * https://github.com/shaka-project/shaka-packager/issues/1571 -- import of
    google.protobuf must succeed (python_edition_defaults.py present).
  * https://github.com/shaka-project/shaka-packager/issues/1511 -- a Pssh
    constructed without explicit pssh_data must not crash binary_string().
"""

import base64
import os
import subprocess
import sys
import unittest

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PSSH_BOX = os.path.join(SCRIPT_DIR, 'pssh-box.py')

WIDEVINE_KEY_ID = '11223344556677889900AABBCCDDEEFF'
WIDEVINE_CONTENT_ID = '6361666562616265'  # b16 of "cafebabe"
PLAYREADY_SYSTEM_ID = '9A04F07998404286AB92E65BE0885F95'


def run(*args):
  # Use stdout/stderr=PIPE and universal_newlines instead of capture_output
  # and text, which were only added in Python 3.7. Some supported distros
  # (e.g. OpenSUSE Leap 15) still ship Python 3.6.
  result = subprocess.run([sys.executable, PSSH_BOX] + list(args),
                          cwd=SCRIPT_DIR,
                          stdout=subprocess.PIPE,
                          stderr=subprocess.PIPE,
                          universal_newlines=True,
                          check=True)
  return result.stdout.strip()


class PsshBoxTest(unittest.TestCase):

  def test_widevine_human(self):
    out = run('--human', '--widevine-system-id',
              '--key-id', WIDEVINE_KEY_ID,
              '--content-id', WIDEVINE_CONTENT_ID)
    self.assertIn('Widevine edef8ba9-79d6-4ace-a3c8-27dcd51d21ed', out)
    self.assertIn('11223344-5566-7788-9900-aabbccddeeff', out)
    self.assertIn('Content ID: ' + WIDEVINE_CONTENT_ID, out)

  def test_widevine_hex_and_base64_agree(self):
    hex_out = run('--hex', '--widevine-system-id',
                  '--key-id', WIDEVINE_KEY_ID,
                  '--content-id', WIDEVINE_CONTENT_ID)
    b64_out = run('--base64', '--widevine-system-id',
                  '--key-id', WIDEVINE_KEY_ID,
                  '--content-id', WIDEVINE_CONTENT_ID)
    self.assertEqual(bytes.fromhex(hex_out), base64.b64decode(b64_out))

  def test_round_trip_widevine(self):
    """Encode -> decode -> re-encode must be byte-identical."""
    hex_a = run('--hex', '--widevine-system-id',
                '--key-id', WIDEVINE_KEY_ID,
                '--content-id', WIDEVINE_CONTENT_ID)
    b64 = base64.b64encode(bytes.fromhex(hex_a)).decode()
    hex_b = run('--hex', '--from-base64', b64)
    self.assertEqual(hex_a, hex_b)

  def test_non_widevine_key_id_only_does_not_crash(self):
    """Regression for #1511: a non-Widevine box with only --key-id used to
    raise `TypeError: can't concat str to bytes` because the default
    pssh_data was '' (str) instead of b''."""
    hex_out = run('--hex',
                  '--system-id', PLAYREADY_SYSTEM_ID,
                  '--key-id', WIDEVINE_KEY_ID)
    raw = bytes.fromhex(hex_out)
    self.assertEqual(raw[4:8], b'pssh')
    self.assertEqual(raw[8], 1)  # version 1, since --key-id was given

  def test_two_box_mixed_systems(self):
    """The original reporter command from #1511: Widevine box followed by a
    PlayReady box with only a key-id. Must succeed and round-trip cleanly."""
    hex_out = run('--hex', '--widevine-system-id',
                  '--key-id', WIDEVINE_KEY_ID,
                  '--content-id', WIDEVINE_CONTENT_ID,
                  '--', '--system-id', PLAYREADY_SYSTEM_ID,
                  '--key-id', WIDEVINE_KEY_ID)
    b64 = base64.b64encode(bytes.fromhex(hex_out)).decode()
    human = run('--human', '--from-base64', b64)
    self.assertIn('PSSH Box v0', human)
    self.assertIn('PSSH Box v1', human)
    self.assertIn('Widevine', human)
    self.assertIn('PlayReady', human)

  def test_explicit_pssh_data_passthrough(self):
    """--pssh-data takes a base64 string and goes through unchanged. This
    exercises the path where pssh_data is already bytes on entry, ensuring
    the b'' default change did not regress it."""
    payload = b'hello world'
    out = run('--human',
              '--system-id', PLAYREADY_SYSTEM_ID,
              '--pssh-data', base64.b64encode(payload).decode())
    self.assertIn('PlayReady', out)
    self.assertIn('PSSH Data (size: %d)' % len(payload), out)


if __name__ == '__main__':
  unittest.main()
