# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import subprocess
import tempfile
import zipfile

from lib.subcommand import SubCommand
from lib.symbol import SymbolDataSources


LOGGER = logging.getLogger('dmprof')


class UploadCommand(SubCommand):
  def __init__(self):
    super(UploadCommand, self).__init__(
        'Usage: %prog upload [--gsutil path/to/gsutil] '
        '<first-dump> <destination-gs-path>')
    self._parser.add_option('--gsutil', default='gsutil',
                            help='path to GSUTIL', metavar='GSUTIL')

  def do(self, sys_argv):
    options, args = self._parse_args(sys_argv, 2)
    dump_path = args[1]
    gs_path = args[2]

    dump_files = SubCommand._find_all_dumps(dump_path)
    bucket_files = SubCommand._find_all_buckets(dump_path)
    prefix = SubCommand._find_prefix(dump_path)
    symbol_data_sources = SymbolDataSources(prefix)
    symbol_data_sources.prepare()
    symbol_path = symbol_data_sources.path()

    handle_zip, filename_zip = tempfile.mkstemp('.zip', 'dmprof')
    os.close(handle_zip)

    try:
      file_zip = zipfile.ZipFile(filename_zip, 'w', zipfile.ZIP_DEFLATED)
      for filename in dump_files:
        file_zip.write(filename, os.path.basename(os.path.abspath(filename)))
      for filename in bucket_files:
        file_zip.write(filename, os.path.basename(os.path.abspath(filename)))

      symbol_basename = os.path.basename(os.path.abspath(symbol_path))
      for filename in os.listdir(symbol_path):
        if not filename.startswith('.'):
          file_zip.write(os.path.join(symbol_path, filename),
                         os.path.join(symbol_basename, os.path.basename(
                             os.path.abspath(filename))))
      file_zip.close()

      returncode = UploadCommand._run_gsutil(
          options.gsutil, 'cp', '-a', 'public-read', filename_zip, gs_path)
    finally:
      os.remove(filename_zip)

    return returncode

  @staticmethod
  def _run_gsutil(gsutil, *args):
    """Run gsutil as a subprocess.

    Args:
        *args: Arguments to pass to gsutil. The first argument should be an
            operation such as ls, cp or cat.
    Returns:
        The return code from the process.
    """
    command = [gsutil] + list(args)
    LOGGER.info("Running: %s", command)

    try:
      return subprocess.call(command)
    except OSError, e:
      LOGGER.error('Error to run gsutil: %s', e)
