#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import hashlib
import json
import logging
import os
import re
import shutil
import subprocess
import sys
import tempfile

from proc_maps import ProcMaps


BASE_PATH = os.path.dirname(os.path.abspath(__file__))
REDUCE_DEBUGLINE_PATH = os.path.join(BASE_PATH, 'reduce_debugline.py')
LOGGER = logging.getLogger('prepare_symbol_info')


def _dump_command_result(command, output_dir_path, basename, suffix):
  handle_out, filename_out = tempfile.mkstemp(
      suffix=suffix, prefix=basename + '.', dir=output_dir_path)
  handle_err, filename_err = tempfile.mkstemp(
      suffix=suffix + '.err', prefix=basename + '.', dir=output_dir_path)
  error = False
  try:
    subprocess.check_call(
        command, stdout=handle_out, stderr=handle_err, shell=True)
  except (OSError, subprocess.CalledProcessError):
    error = True
  finally:
    os.close(handle_err)
    os.close(handle_out)

  if os.path.exists(filename_err):
    if LOGGER.getEffectiveLevel() <= logging.DEBUG:
      with open(filename_err, 'r') as f:
        for line in f:
          LOGGER.debug(line.rstrip())
    os.remove(filename_err)

  if os.path.exists(filename_out) and (
      os.path.getsize(filename_out) == 0 or error):
    os.remove(filename_out)
    return None

  if not os.path.exists(filename_out):
    return None

  return filename_out


def prepare_symbol_info(maps_path,
                        output_dir_path=None,
                        alternative_dirs=None,
                        use_tempdir=False,
                        use_source_file_name=False):
  """Prepares (collects) symbol information files for find_runtime_symbols.

  1) If |output_dir_path| is specified, it tries collecting symbol information
  files in the given directory |output_dir_path|.
  1-a) If |output_dir_path| doesn't exist, create the directory and use it.
  1-b) If |output_dir_path| is an empty directory, use it.
  1-c) If |output_dir_path| is a directory which has 'files.json', assumes that
       files are already collected and just ignores it.
  1-d) Otherwise, depends on |use_tempdir|.

  2) If |output_dir_path| is not specified, it tries to create a new directory
  depending on 'maps_path'.

  If it cannot create a new directory, creates a temporary directory depending
  on |use_tempdir|.  If |use_tempdir| is False, returns None.

  Args:
      maps_path: A path to a file which contains '/proc/<pid>/maps'.
      alternative_dirs: A mapping from a directory '/path/on/target' where the
          target process runs to a directory '/path/on/host' where the script
          reads the binary.  Considered to be used for Android binaries.
      output_dir_path: A path to a directory where files are prepared.
      use_tempdir: If True, it creates a temporary directory when it cannot
          create a new directory.
      use_source_file_name: If True, it adds reduced result of 'readelf -wL'
          to find source file names.

  Returns:
      A pair of a path to the prepared directory and a boolean representing
      if it created a temporary directory or not.
  """
  alternative_dirs = alternative_dirs or {}
  if not output_dir_path:
    matched = re.match('^(.*)\.maps$', os.path.basename(maps_path))
    if matched:
      output_dir_path = matched.group(1) + '.pre'
  if not output_dir_path:
    matched = re.match('^/proc/(.*)/maps$', os.path.realpath(maps_path))
    if matched:
      output_dir_path = matched.group(1) + '.pre'
  if not output_dir_path:
    output_dir_path = os.path.basename(maps_path) + '.pre'
  # TODO(dmikurube): Find another candidate for output_dir_path.

  used_tempdir = False
  LOGGER.info('Data for profiling will be collected in "%s".' % output_dir_path)
  if os.path.exists(output_dir_path):
    if os.path.isdir(output_dir_path) and not os.listdir(output_dir_path):
      LOGGER.warn('Using an empty existing directory "%s".' % output_dir_path)
    else:
      LOGGER.warn('A file or a directory exists at "%s".' % output_dir_path)
      if os.path.exists(os.path.join(output_dir_path, 'files.json')):
        LOGGER.warn('Using the existing directory "%s".' % output_dir_path)
        return output_dir_path, used_tempdir
      else:
        if use_tempdir:
          output_dir_path = tempfile.mkdtemp()
          used_tempdir = True
          LOGGER.warn('Using a temporary directory "%s".' % output_dir_path)
        else:
          LOGGER.warn('The directory "%s" is not available.' % output_dir_path)
          return None, used_tempdir
  else:
    LOGGER.info('Creating a new directory "%s".' % output_dir_path)
    try:
      os.mkdir(output_dir_path)
    except OSError:
      LOGGER.warn('A directory "%s" cannot be created.' % output_dir_path)
      if use_tempdir:
        output_dir_path = tempfile.mkdtemp()
        used_tempdir = True
        LOGGER.warn('Using a temporary directory "%s".' % output_dir_path)
      else:
        LOGGER.warn('The directory "%s" is not available.' % output_dir_path)
        return None, used_tempdir

  shutil.copyfile(maps_path, os.path.join(output_dir_path, 'maps'))

  with open(maps_path, mode='r') as f:
    maps = ProcMaps.load(f)

  LOGGER.debug('Listing up symbols.')
  files = {}
  for entry in maps.iter(ProcMaps.executable):
    LOGGER.debug('  %016x-%016x +%06x %s' % (
        entry.begin, entry.end, entry.offset, entry.name))
    binary_path = entry.name
    for target_path, host_path in alternative_dirs.iteritems():
      if entry.name.startswith(target_path):
        binary_path = entry.name.replace(target_path, host_path, 1)
    nm_filename = _dump_command_result(
        'nm -n --format bsd %s | c++filt' % binary_path,
        output_dir_path, os.path.basename(binary_path), '.nm')
    if not nm_filename:
      continue
    readelf_e_filename = _dump_command_result(
        'readelf -eW %s' % binary_path,
        output_dir_path, os.path.basename(binary_path), '.readelf-e')
    if not readelf_e_filename:
      continue
    readelf_debug_decodedline_file = None
    if use_source_file_name:
      readelf_debug_decodedline_file = _dump_command_result(
          'readelf -wL %s | %s' % (binary_path, REDUCE_DEBUGLINE_PATH),
          output_dir_path, os.path.basename(binary_path), '.readelf-wL')

    files[entry.name] = {}
    files[entry.name]['nm'] = {
        'file': os.path.basename(nm_filename),
        'format': 'bsd',
        'mangled': False}
    files[entry.name]['readelf-e'] = {
        'file': os.path.basename(readelf_e_filename)}
    if readelf_debug_decodedline_file:
      files[entry.name]['readelf-debug-decodedline-file'] = {
          'file': os.path.basename(readelf_debug_decodedline_file)}

    files[entry.name]['size'] = os.stat(binary_path).st_size

    with open(binary_path, 'rb') as entry_f:
      md5 = hashlib.md5()
      sha1 = hashlib.sha1()
      chunk = entry_f.read(1024 * 1024)
      while chunk:
        md5.update(chunk)
        sha1.update(chunk)
        chunk = entry_f.read(1024 * 1024)
      files[entry.name]['sha1'] = sha1.hexdigest()
      files[entry.name]['md5'] = md5.hexdigest()

  with open(os.path.join(output_dir_path, 'files.json'), 'w') as f:
    json.dump(files, f, indent=2, sort_keys=True)

  LOGGER.info('Collected symbol information at "%s".' % output_dir_path)
  return output_dir_path, used_tempdir


def main():
  if not sys.platform.startswith('linux'):
    sys.stderr.write('This script work only on Linux.')
    return 1

  LOGGER.setLevel(logging.DEBUG)
  handler = logging.StreamHandler()
  handler.setLevel(logging.INFO)
  formatter = logging.Formatter('%(message)s')
  handler.setFormatter(formatter)
  LOGGER.addHandler(handler)

  # TODO(dmikurube): Specify |alternative_dirs| from command line.
  if len(sys.argv) < 2:
    sys.stderr.write("""Usage:
%s /path/to/maps [/path/to/output_data_dir/]
""" % sys.argv[0])
    return 1
  elif len(sys.argv) == 2:
    result, _ = prepare_symbol_info(sys.argv[1])
  else:
    result, _ = prepare_symbol_info(sys.argv[1], sys.argv[2])

  return not result


if __name__ == '__main__':
  sys.exit(main())
