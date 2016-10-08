#!/usr/bin/python
# Copyright 2016 Google Inc. All rights reserved.
#
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file or at
# https://developers.google.com/open-source/licenses/bsd

"""A utility to parse and generate PSSH boxes."""

import argparse
import base64
import itertools
import os
import struct
import sys

# Append the local protobuf location.  Use a path relative to the tools/pssh
# folder where this file should be found.  This allows the file to be executed
# from any directory.
_pssh_dir = os.path.dirname(os.path.realpath(__file__))
sys.path.insert(0, os.path.join(_pssh_dir, '../../third_party/protobuf/python'))
# Import the widevine protobuf.  Use either Release or Debug.
_proto_path_format = os.path.join(
    _pssh_dir, '../../../out/%s/pyproto/packager/media/base')
if os.path.isdir(_proto_path_format % 'Release'):
  sys.path.insert(0, _proto_path_format % 'Release')
else:
  sys.path.insert(0, _proto_path_format % 'Debug')
try:
  import widevine_pssh_data_pb2  # pylint: disable=g-import-not-at-top
except ImportError:
  print >> sys.stderr, 'Cannot find proto file, make sure to build first'
  raise


COMMON_SYSTEM_ID = base64.b16decode('1077EFECC0B24D02ACE33C1E52E2FB4B')
WIDEVINE_SYSTEM_ID = base64.b16decode('EDEF8BA979D64ACEA3C827DCD51D21ED')
PLAYREADY_SYSTEM_ID = base64.b16decode('9A04F07998404286AB92E65BE0885F95')


class BinaryReader(object):
  """A helper class used to read binary data from an binary string."""

  def __init__(self, data, little_endian):
    self.data = data
    self.little_endian = little_endian
    self.position = 0

  def has_data(self):
    """Returns whether the reader has any data left to read."""
    return self.position < len(self.data)

  def read_bytes(self, count):
    """Reads the given number of bytes into an array."""
    if len(self.data) < self.position + count:
      raise Exception('Invalid PSSH box, not enough data')
    ret = self.data[self.position:self.position+count]
    self.position += count
    return ret

  def read_int(self, size):
    """Reads an integer of the given size (in bytes)."""
    data = self.read_bytes(size)
    ret = 0
    for i in range(0, size):
      if self.little_endian:
        ret |= (ord(data[i]) << (8 * i))
      else:
        ret |= (ord(data[i]) << (8 * (size - i - 1)))
    return ret


class Pssh(object):
  """Defines a PSSH box and related functions."""

  def __init__(self, version, system_id, key_ids, pssh_data):
    """Parses a PSSH box from the given data.

    Args:
      version: The version number of the box
      system_id: A binary string of the System ID
      key_ids: An array of binary strings for the key IDs
      pssh_data: A binary string of the PSSH data
    """
    self.version = version
    self.system_id = system_id
    self.key_ids = key_ids or []
    self.pssh_data = pssh_data or ''

  def binary_string(self):
    """Converts the PSSH box to a binary string."""
    ret = b'pssh' + _create_bin_int(self.version << 24)
    ret += self.system_id
    if self.version == 1:
      ret += _create_bin_int(len(self.key_ids))
      for key in self.key_ids:
        ret += key
    ret += _create_bin_int(len(self.pssh_data))
    ret += self.pssh_data
    return _create_bin_int(len(ret) + 4) + ret

  def human_string(self):
    """Converts the PSSH box to a human readable string."""
    system_name = ''
    convert_data = None
    if self.system_id == WIDEVINE_SYSTEM_ID:
      system_name = 'Widevine'
      convert_data = _parse_widevine_data
    elif self.system_id == PLAYREADY_SYSTEM_ID:
      system_name = 'PlayReady'
      convert_data = _parse_playready_data
    elif self.system_id == COMMON_SYSTEM_ID:
      system_name = 'Common'

    lines = [
        'PSSH Box v%d' % self.version,
        '  System ID: %s %s' % (system_name, _create_uuid(self.system_id))
    ]
    if self.version == 1:
      lines.append('  Key IDs (%d):' % len(self.key_ids))
      lines.extend(['    ' + _create_uuid(key) for key in self.key_ids])

    lines.append('  PSSH Data (size: %d):' % len(self.pssh_data))
    if self.pssh_data:
      if convert_data:
        lines.append('    ' + system_name + ' Data:')
        try:
          extra = convert_data(self.pssh_data)
          lines.extend(['      ' + x for x in extra])
        # pylint: disable=broad-except
        except Exception as e:
          lines.append('      ERROR: ' + e.message)
      else:
        lines.extend([
            '    Raw Data (base64):',
            '      ' + base64.b64encode(self.pssh_data)
        ])

    return '\n'.join(lines)


def _split_list_on(elems, sep):
  """Splits the given list on the given separator."""
  return [list(g) for k, g in itertools.groupby(elems, lambda x: x == sep)
          if not k]


def _create_bin_int(value):
  """Creates a 4-byte binary string from the given integer."""
  return (chr(value >> 24) + chr((value >> 16) & 0xff) +
          chr((value >> 8) & 0xff) + chr(value & 0xff))


def _create_uuid(data):
  """Creates a human readable UUID string from the given binary string."""
  ret = base64.b16encode(data).lower()
  return (ret[:8] + '-' + ret[8:12] + '-' + ret[12:16] + '-' + ret[16:20] +
          '-' + ret[20:])


def _generate_widevine_data(key_ids, content_id, provider, protection_scheme):
  """Generate widevine pssh data."""
  wv = widevine_pssh_data_pb2.WidevinePsshData()
  wv.key_id.extend(key_ids)
  wv.provider = provider or ''
  wv.content_id = content_id
  # 'cenc' is the default, so omitted to save bytes.
  if protection_scheme and protection_scheme != 'cenc':
    wv.protection_scheme = struct.unpack('>L', protection_scheme)[0]
  return wv.SerializeToString()


def _parse_widevine_data(data):
  """Parses Widevine PSSH box from the given binary string."""
  wv = widevine_pssh_data_pb2.WidevinePsshData()
  wv.ParseFromString(data)

  ret = []
  if wv.key_id:
    ret.append('Key IDs (%d):' % len(wv.key_id))
    ret.extend(['  ' + _create_uuid(x) for x in wv.key_id])

  if wv.HasField('provider'):
    ret.append('Provider: ' + wv.provider)
  if wv.HasField('content_id'):
    ret.append('Content ID: ' + base64.b16encode(wv.content_id))
  if wv.HasField('policy'):
    ret.append('Policy: ' + wv.policy)
  if wv.HasField('crypto_period_index'):
    ret.append('Crypto Period Index: %d' % wv.crypto_period_index)
  if wv.HasField('protection_scheme'):
    protection_scheme = struct.pack('>L', wv.protection_scheme)
    ret.append('Protection Scheme: %s' % protection_scheme)

  return ret


def _parse_playready_data(data):
  """Parses PlayReady PSSH data from the given binary string."""
  reader = BinaryReader(data, little_endian=True)
  size = reader.read_int(4)
  if size != len(data):
    raise Exception('Length incorrect')

  ret = []
  count = reader.read_int(2)
  while count > 0:
    count -= 1
    record_type = reader.read_int(2)
    record_len = reader.read_int(2)
    record_data = reader.read_bytes(record_len)

    ret.append('Record (size %d):' % record_len)
    if record_type == 1:
      xml = record_data.decode('utf-16 LE')
      ret.extend([
          '  Record Type: Rights Management Header (1)',
          '  Record XML:',
          '    ' + xml
      ])
    elif record_type == 3:
      ret.extend([
          '  Record Type: License Store (1)',
          '  License Data:',
          '    ' + base64.b64encode(record_data)
      ])
    else:
      raise Exception('Invalid record type %d' % record_type)

  if reader.has_data():
    raise Exception('Extra data after records')

  return ret


def _parse_boxes(data):
  """Parses one or more PSSH boxes for the given binary data."""
  reader = BinaryReader(data, little_endian=False)
  boxes = []
  while reader.has_data():
    start = reader.position
    size = reader.read_int(4)

    box_type = reader.read_bytes(4)
    if box_type != b'pssh':
      raise Exception(
          'Invalid box type 0x%s, not \'pssh\'' % box_type.encode('hex'))

    version_and_flags = reader.read_int(4)
    version = version_and_flags >> 24
    if version > 1:
      raise Exception('Invalid PSSH version %d' % version)

    system_id = reader.read_bytes(16)

    key_ids = []
    if version == 1:
      count = reader.read_int(4)
      while count > 0:
        key = reader.read_bytes(16)
        key_ids.append(key)
        count -= 1

    pssh_data_size = reader.read_int(4)
    pssh_data = reader.read_bytes(pssh_data_size)

    if start + size != reader.position:
      raise Exception('Box size does not match size of data')

    pssh = Pssh(version, system_id, key_ids, pssh_data)
    boxes.append(pssh)
  return boxes


def _create_argument_parser():
  """Creates an argument parser."""

  def hex_16_bytes(string):
    if not string or len(string) != 32:
      raise argparse.ArgumentTypeError(
          'Must be a 32-character hex string, %d given' % len(string))
    return base64.b16decode(string.upper())

  def hex_bytes(string):
    return base64.b16decode(string.upper())

  parser = argparse.ArgumentParser(
      formatter_class=argparse.RawDescriptionHelpFormatter,
      usage='[--base64 | --hex | --human] options [-- options [-- ...]',
      epilog="""\
This utility can be used to generate one or more PSSH boxes.  This is done by
passing a system ID and either --key-id or --pssh-data.  Multiple boxes can be
generated by separating boxes with --.  Using --key-id will generate v1 pssh
boxes, if none are given, it will generate v0.

You can also import PSSH boxes using --from-base64 and --from-hex.  These must
be valid PSSH boxes, but can be multiple concatenated together.  These arguments
can appear anywhere in the string.  If it appears 'inside' another definition,
it will appear before the generated one.

An alternative to --pssh-data is to generate Widevine PSSH data.  This is only
valid with --widevine-system-id.  Passing --content-id will make it generate
Widevine PSSH data instead.  You can optionally add --provider and/or
--protection-scheme.  It will generate a v0 PSSH box for compatibility
reasons.""")

  formats = parser.add_mutually_exclusive_group()
  formats.add_argument('--base64',
                       dest='format',
                       action='store_const',
                       const='base64',
                       help='Output base64 encoded')
  formats.add_argument('--hex',
                       dest='format',
                       action='store_const',
                       const='hex',
                       help='Output hexadecimal encoded')
  formats.add_argument('--human',
                       dest='format',
                       action='store_const',
                       const='human',
                       help='Output a human readable string')

  inputs = parser.add_mutually_exclusive_group()
  inputs.add_argument('--from-base64',
                      metavar='<base64-string>',
                      dest='input',
                      type=base64.b64decode,
                      help='Parse the given base64 encoded PSSH box')
  inputs.add_argument('--from-hex',
                      metavar='<hex-string>',
                      dest='input',
                      type=hex_bytes,
                      help='Parse the given hexadecimal encoded PSSH box')

  system_ids = parser.add_mutually_exclusive_group()
  system_ids.add_argument('--system-id',
                          metavar='<hex-string>',
                          dest='system_id',
                          type=hex_16_bytes,
                          help='Sets the system ID')
  system_ids.add_argument('--common-system-id',
                          dest='system_id',
                          action='store_const',
                          const=COMMON_SYSTEM_ID,
                          help='Use the Common system ID')
  system_ids.add_argument('--widevine-system-id',
                          dest='system_id',
                          action='store_const',
                          const=WIDEVINE_SYSTEM_ID,
                          help='Use the Widevine system ID')

  extra = parser.add_argument_group()
  extra.add_argument('--key-id',
                     metavar='<hex-string>',
                     action='append',
                     type=hex_16_bytes,
                     help='Adds a key ID (can appear multiple times)')
  extra.add_argument('--pssh-data',
                     metavar='<base64-string>',
                     type=base64.b64decode,
                     help='Sets the extra data')
  extra.add_argument('--content-id',
                     metavar='<hex-string>',
                     type=hex_bytes,
                     help='Sets the content ID of the Widevine PSSH data')
  extra.add_argument('--provider',
                     metavar='<string>',
                     help='Sets the provider of the Widevine PSSH data')
  extra.add_argument('--protection-scheme',
                     choices=['cenc', 'cbcs', 'cens', 'cbc1'],
                     help='Set the protection scheme of the Widevine PSSH data')

  return parser


def main(all_args):
  boxes = []
  output_format = None
  parser = _create_argument_parser()
  if not all_args:
    parser.print_help()
    sys.exit(1)
  arg_groups = _split_list_on(all_args, '--')
  for args in arg_groups:
    ns = parser.parse_args(args)

    if ns.format:
      if output_format:
        raise Exception('Can only specify one of: --base64, --hex, --human')
      else:
        output_format = ns.format

    if ns.input:
      boxes.extend(_parse_boxes(ns.input))

    pssh_data = ns.pssh_data
    if pssh_data and ns.content_id:
      raise Exception('Cannot specify both --pssh-data and --content-id')
    if ns.protection_scheme:
      if ns.system_id != WIDEVINE_SYSTEM_ID:
        raise Exception(
            '--protection-scheme only valid with Widevine system ID')
    if ns.content_id:
      if ns.system_id != WIDEVINE_SYSTEM_ID:
        raise Exception('--content-id only valid with Widevine system ID')
      pssh_data = _generate_widevine_data(ns.key_id, ns.content_id, ns.provider,
                                          ns.protection_scheme)

    # Ignore if we have no data.
    if not pssh_data and not ns.key_id and not ns.system_id:
      continue
    if not ns.system_id:
      raise Exception('System ID is required')

    version = 1 if ns.key_id and not ns.content_id else 0
    boxes.append(Pssh(version, ns.system_id, ns.key_id, pssh_data))

  if output_format == 'human' or not output_format:
    for box in boxes:
      print box.human_string()
  else:
    box_data = ''.join([x.binary_string() for x in boxes])
    if output_format == 'hex':
      print base64.b16encode(box_data)
    else:
      print base64.b64encode(box_data)


if __name__ == '__main__':
  main(sys.argv[1:])
